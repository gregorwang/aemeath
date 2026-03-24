#include "app/application_bootstrap.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QClipboard>
#include <QDir>
#include <QIcon>
#include <QMessageBox>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

#include "runtime/app_logger.h"
#include "runtime/idle_invasion_controller.h"
#include "runtime/app_paths.h"
#include "runtime/auto_start_manager.h"
#include "runtime/character_switch_matcher.h"
#include "runtime/runtime_director.h"
#include "runtime/single_instance_guard.h"
#include "runtime/version_manifest.h"
#include "runtime/voice_command_matcher.h"
#include "services/audio_service.h"
#include "services/audio_output_monitor_service.h"
#include "services/fullscreen_monitor_service.h"
#include "services/hotkey_service_win.h"
#include "services/idle_monitor_service.h"
#include "services/openai_commentary_service.h"
#include "services/vision_service.h"
#include "services/voice_input_service.h"
#include "ui/entity_widget.h"
#include "ui/settings_dialog.h"
#include "ui/trajectory_player.h"
#include "ui/tray_controller.h"

namespace {

constexpr int kScriptedTrajectoryWatchdogPaddingMs = 1800;
constexpr int kScriptedTrajectoryWatchdogMinimumMs = 3000;
constexpr int kScriptedTrajectoryWatchdogMaximumMs = 45000;

int scriptedTrajectoryWatchdogMs(const TrajectoryPlayer &player)
{
    const int durationMs = player.currentDurationMs();
    const int requestedMs = durationMs + kScriptedTrajectoryWatchdogPaddingMs;
    return qBound(
        kScriptedTrajectoryWatchdogMinimumMs,
        requestedMs,
        kScriptedTrajectoryWatchdogMaximumMs);
}

QString summarizedRuntimeError(const QString &message)
{
    const QString cleaned = message.simplified().trimmed();
    if (cleaned.isEmpty()) {
        return QStringLiteral("请查看日志并通过“反馈问题”提交 issue。");
    }
    return QStringLiteral("%1 可在托盘中使用“复制最近日志”并“反馈问题”。")
        .arg(cleaned.left(180));
}

QString joinReasons(const QStringList &reasons)
{
    return reasons.isEmpty()
        ? QStringLiteral("未满足前置条件")
        : reasons.join(QStringLiteral("; "));
}

EntityPositions::Edge preferredEdgeForConfig(const AppConfig &config)
{
    return config.preferredPosition.trimmed().compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0
        ? EntityPositions::Edge::Left
        : EntityPositions::Edge::Right;
}

}

ApplicationBootstrap::ApplicationBootstrap(QApplication &app)
    : QObject(nullptr)
    , m_app(app)
{
}

ApplicationBootstrap::~ApplicationBootstrap()
{
    if (m_trayController) {
        m_trayController->hide();
    }
}

bool ApplicationBootstrap::initialize()
{
    if (m_initialized) {
        return true;
    }

    setupApplicationMetadata();

    m_configRepository = std::make_unique<ConfigRepository>(AppPaths::configFilePath());
    m_configRepository->bootstrapFromLegacy(AppPaths::legacyConfigFilePath());
    m_config = m_configRepository->load();
    m_autoStartManager = std::make_unique<AutoStartManager>();
    m_characterManifestCatalog = std::make_unique<CharacterManifestCatalog>(AppPaths::resolveOptionalAsset(QStringLiteral("characters")));
    if (m_characterManifestCatalog) {
        CharacterManifest manifest = m_characterManifestCatalog->findById(m_config.activeCharacterId);
        const QVector<CharacterManifest> manifests = m_characterManifestCatalog->manifests();
        if (!manifest.isValid() && !manifests.isEmpty()) {
            manifest = manifests.constFirst();
        }
        if (manifest.isValid()) {
            m_activeCharacterManifest = manifest;
            m_config.activeCharacterId = manifest.id;
            if (m_config.voiceScriptsPath.trimmed().isEmpty() && !manifest.scriptsPath.trimmed().isEmpty()) {
                m_config.voiceScriptsPath = manifest.scriptsPath;
            }
            if (m_config.ttsVoice.trimmed().isEmpty() && !manifest.defaultVoice.trimmed().isEmpty()) {
                m_config.ttsVoice = manifest.defaultVoice;
            }
        }
    }

    AppLogger::initialize(AppPaths::logFilePath(), m_config.debugMode);
    syncAutoStartSetting();

    m_singleInstanceGuard = std::make_unique<SingleInstanceGuard>(AppPaths::userDataDir());
    if (!m_singleInstanceGuard->acquire()) {
        QMessageBox::information(
            nullptr,
            QStringLiteral("CyberCompanionCpp"),
            QStringLiteral("CyberCompanionCpp 已在运行，请先关闭已有实例。"));
        return false;
    }

    m_entityWidget = std::make_unique<EntityWidget>();
    m_entityWidget->setScreenEdge(preferredEdgeForConfig(m_config));
    m_entityWidget->applyAppearanceConfig(m_config.appearanceAsciiWidth, m_config.appearanceFontSizePx);
    m_runtimeDirector = std::make_unique<RuntimeDirector>(this);
    m_runtimeDirector->setScriptedEntranceEnabled(m_config.scriptedEntranceEnabled);
    m_runtimeDirector->setScriptedTrajectoryPath(m_config.scriptedTrajectoryPath);
    m_runtimeDirector->setVoiceScriptsPath(m_config.voiceScriptsPath);
    m_runtimeDirector->setAudioOutputReactive(m_config.audioOutputReactive);
    m_runtimeDirector->setAutoDismissSeconds(m_config.autoDismissSeconds);
    m_runtimeDirector->setIdleInvasionEnabled(m_config.idleInvasion.enabled);
    m_runtimeDirector->setIdleInvasionStartDelayMs(m_config.idleInvasion.startDelayMs);
    m_runtimeDirector->setOfflineMode(m_config.offlineMode);
    m_runtimeDirector->setScreenCommentaryAutoEnabled(m_config.screenCommentaryAutoEnabled);
    m_runtimeDirector->setScreenCommentaryAutoIntervalMinutes(m_config.screenCommentaryAutoIntervalMinutes);
    m_runtimeDirector->setScreenCommentarySpeechOptions(
        m_config.commentaryStreamingEnabled,
        m_config.commentaryStreamChunkChars,
        m_config.commentaryMaxResponseChars,
        m_config.commentaryPreambleText);
    m_runtimeDirector->setCameraEnabled(m_config.cameraEnabled && m_config.cameraConsentGranted);
    m_runtimeDirector->setPresenceTargetFps(m_config.cameraTargetFps);
    m_runtimeDirector->setEyeTrackingEnabled(m_config.eyeTrackingEnabled);
    m_runtimeDirector->setPeriodicScanEnabled(m_config.periodicScanEnabled);
    m_runtimeDirector->setPeriodicScanIntervalMinutes(m_config.periodicScanIntervalMinutes);
    m_runtimeDirector->setFullScreenPauseEnabled(m_config.fullScreenPause);
    m_runtimeDirector->setResidentModeEnabled(m_config.residentMode);
    m_idleInvasionController = std::make_unique<IdleInvasionController>(this);
    m_idleInvasionController->applyConfig(m_config.idleInvasion);
    m_trajectoryPlayer = std::make_unique<TrajectoryPlayer>(m_entityWidget.get(), this);
    m_scriptedTrajectoryWatchdog = new QTimer(this);
    m_scriptedTrajectoryWatchdog->setSingleShot(true);
    m_audioService = std::make_unique<QtAudioService>(
        AppPaths::resolveOptionalAsset(QStringLiteral("assets/notification.wav")),
        AppPaths::ttsCacheDir().absolutePath(),
        this);
    m_audioService->setTtsProvider(m_config.ttsProvider);
    m_audioService->configureVoice(m_config.ttsVoice, m_config.ttsRate);
    m_audioService->setVolume(m_config.audioVolume);
    m_audioService->setCacheEnabled(m_config.audioCacheEnabled);
    m_hotkeyService = std::make_unique<WindowsHotkeyService>(m_app, this);
    m_idleMonitorService = std::make_unique<IdleMonitorServiceNative>(this);
    rearmIdleMonitorThreshold();
    m_fullscreenMonitorService = std::make_unique<WindowsFullscreenMonitorService>(this);
    rebuildAudioOutputMonitorService();
    rebuildScreenCommentaryService();
    rebuildVisionService();
    rebuildVoiceInputService();
    maybeWarnLegacyTtsProviderMigration();
    maybeWarnLegacyAsrProviderMigration();
    if (m_activeCharacterManifest.isValid()) {
        applyCharacterManifest(m_activeCharacterManifest, false, false);
    }
    if (m_config.cameraEnabled && !m_config.cameraConsentGranted) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("camera-consent-missing"),
            QStringLiteral("摄像头"),
            QStringLiteral("已启用摄像头相关功能，但尚未授权访问。当前不会启动视觉服务。"));
    }

    const QIcon icon = AppPaths::resolveTrayIcon();
    m_trayController = std::make_unique<TrayController>(icon);
    m_trayController->setOfflineChecked(m_config.offlineMode);
    m_trayController->setResidentChecked(m_config.residentMode);
    m_trayController->setAutoCommentaryChecked(m_config.screenCommentaryAutoEnabled);
    m_trayController->setCameraChecked(m_config.cameraEnabled);
    m_trayController->setMicrophoneChecked(m_config.microphoneEnabled);
    m_trayController->setWakeupChecked(m_config.wakeupEnabled);
    m_trayController->setEyeTrackingChecked(m_config.eyeTrackingEnabled);
    m_trayController->setPeriodicScanChecked(m_config.periodicScanEnabled);
    m_trayController->setAudioReactiveChecked(m_config.audioOutputReactive);
    m_trayController->setContinuousVoiceModeChecked(
        m_config.voiceInputMode.trimmed().compare(QStringLiteral("continuous"), Qt::CaseInsensitive) == 0);
    m_trayController->setScriptedEntranceChecked(m_config.scriptedEntranceEnabled);
    m_trayController->setFullscreenPauseChecked(m_config.fullScreenPause);
    m_trayController->setIdleInvasionChecked(m_config.idleInvasion.enabled);
    if (m_characterManifestCatalog) {
        m_trayController->updateCharacters(m_characterManifestCatalog->manifests(), m_config.activeCharacterId);
    }

    wireSignals();
    restoreRuntimeState();
    m_entityWidget->setStateByName(QStringLiteral("state1"));
    m_entityWidget->setAutonomousEnabled(false);

    m_trayController->show();

    m_initialized = true;
    m_runtimeDirector->start();
    m_hotkeyService->start();
    if (m_config.audioOutputReactive) {
        m_audioOutputMonitorService->start();
    }
    if (m_voiceInputService && m_config.microphoneEnabled) {
        m_voiceInputService->startContinuousListening();
    }
    startVisionServiceIfNeeded();
    m_idleMonitorService->start();
    if (m_fullscreenMonitorService) {
        m_fullscreenMonitorService->start();
    }

    const QString startupSummary = buildStartupSummary();
    if (m_config.firstRun) {
        m_trayController->showStartupMessage(QStringLiteral("赛博伴侣已启动"), startupSummary);
        m_trayController->showStartupMessage(
            QStringLiteral("欢迎使用 CyberCompanionCpp"),
            QStringLiteral("正在打开快速上手指南。"));
        m_config.firstRun = false;
        saveConfigWithNotification(QStringLiteral("首次启动标记"));
        openQuickStartGuide();
    } else if (!isAutoStartLaunch()) {
        m_trayController->showStartupMessage(QStringLiteral("赛博伴侣已启动"), startupSummary);
    }
    return true;
}

void ApplicationBootstrap::toggleEntityVisibility()
{
    if (!m_entityWidget) {
        return;
    }
    if (m_entityWidget->isVisible()) {
        hideEntity();
    } else {
        showEntity();
    }
}

void ApplicationBootstrap::showEntity()
{
    if (!m_entityWidget) {
        return;
    }
    m_entityWidget->transitionToVisualState(EntityWidget::VisualState::Engaged);
    m_entityWidget->raise();
    m_entityWidget->activateWindow();
}

void ApplicationBootstrap::hideEntity()
{
    if (m_entityWidget) {
        m_entityWidget->hideNow();
    }
}

void ApplicationBootstrap::resetEntityPosition()
{
    if (!m_entityWidget) {
        return;
    }
    m_entityWidget->moveToDefaultCorner();
    m_entityWidget->show();
}

void ApplicationBootstrap::setDndModeEnabled(bool enabled)
{
    if (m_trayController) {
        m_trayController->setDndChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setDndMode(enabled);
    }
    if (m_idleInvasionController) {
        m_idleInvasionController->setDndEnabled(enabled);
    }
    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("请勿打扰"),
            enabled ? QStringLiteral("已开启，自动出场与空闲入侵已暂停。")
                    : QStringLiteral("已关闭，自动行为已恢复。"));
    }
}

void ApplicationBootstrap::setOfflineModeEnabled(bool enabled)
{
    if (m_config.offlineMode == enabled) {
        if (m_trayController) {
            m_trayController->setOfflineChecked(enabled);
        }
        return;
    }

    m_config.offlineMode = enabled;
    if (m_trayController) {
        m_trayController->setOfflineChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setOfflineMode(enabled);
    }
    rebuildScreenCommentaryService();
    rebuildVoiceInputService();
    saveConfigWithNotification(QStringLiteral("离线模式切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("离线模式"),
            enabled
                ? QStringLiteral("已开启：屏幕评论、远程语音转写和连续唤醒已停用。")
                : QStringLiteral("已关闭：远程 AI 能力已恢复。"));
    }
}

void ApplicationBootstrap::setResidentModeEnabled(bool enabled)
{
    if (m_config.residentMode == enabled) {
        if (m_trayController) {
            m_trayController->setResidentChecked(enabled);
        }
        return;
    }

    m_config.residentMode = enabled;
    if (m_trayController) {
        m_trayController->setResidentChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setResidentModeEnabled(enabled);
    }
    saveConfigWithNotification(QStringLiteral("常驻模式切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("常驻模式"),
            enabled
                ? QStringLiteral("已开启：角色会尽量保持可见，全屏时仍会自动隐藏。")
                : QStringLiteral("已关闭：角色会恢复普通自动出现/撤退逻辑。"));
    }
}

void ApplicationBootstrap::setAutoCommentaryEnabled(bool enabled)
{
    if (m_config.screenCommentaryAutoEnabled == enabled) {
        if (m_trayController) {
            m_trayController->setAutoCommentaryChecked(enabled);
        }
        return;
    }

    m_config.screenCommentaryAutoEnabled = enabled;
    if (m_trayController) {
        m_trayController->setAutoCommentaryChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setScreenCommentaryAutoEnabled(enabled);
    }
    saveConfigWithNotification(QStringLiteral("自动评论切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("自动评论"),
            enabled
                ? (m_config.offlineMode
                    ? QStringLiteral("已开启，但当前处于离线模式；联网后才会自动触发评论。")
                    : QStringLiteral("已开启：后续会按设定间隔自动解读屏幕。"))
                : QStringLiteral("已关闭：后续不会自动触发屏幕评论。"));
    }
}

void ApplicationBootstrap::setCameraEnabled(bool enabled)
{
    if (m_config.cameraEnabled == enabled) {
        if (m_trayController) {
            m_trayController->setCameraChecked(enabled);
        }
        return;
    }

    m_config.cameraEnabled = enabled;
    if (m_trayController) {
        m_trayController->setCameraChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setCameraEnabled(m_config.cameraEnabled && m_config.cameraConsentGranted);
    }
    rebuildVisionService();
    saveConfigWithNotification(QStringLiteral("摄像头开关切换"));

    if (enabled && !m_config.cameraConsentGranted) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("camera-consent-missing"),
            QStringLiteral("摄像头"),
            QStringLiteral("已启用摄像头，但尚未在设置中授予 consent。当前不会启动视觉服务。"));
    }

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("摄像头"),
            enabled
                ? (m_config.cameraConsentGranted
                    ? QStringLiteral("已开启：角色出现或调试巡检时会按需拉起视觉服务。")
                    : QStringLiteral("已开启，但尚未授权；请在设置中勾选摄像头 consent。"))
                : QStringLiteral("已关闭：视觉服务会停止并释放摄像头资源。"));
    }
}

void ApplicationBootstrap::setMicrophoneEnabled(bool enabled)
{
    if (m_config.microphoneEnabled == enabled) {
        if (m_trayController) {
            m_trayController->setMicrophoneChecked(enabled);
        }
        return;
    }

    m_config.microphoneEnabled = enabled;
    if (m_trayController) {
        m_trayController->setMicrophoneChecked(enabled);
    }
    rebuildVoiceInputService();
    saveConfigWithNotification(QStringLiteral("麦克风开关切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("麦克风"),
            enabled
                ? (m_config.offlineMode
                    ? QStringLiteral("已开启，但当前处于离线模式；远程语音转写和唤醒仍会保持停用。")
                    : QStringLiteral("已开启：按键转写或连续唤醒会按当前模式生效。"))
                : QStringLiteral("已关闭：语音转写和唤醒已停止。"));
    }
}

void ApplicationBootstrap::setWakeupEnabled(bool enabled)
{
    if (m_config.wakeupEnabled == enabled) {
        if (m_trayController) {
            m_trayController->setWakeupChecked(enabled);
        }
        return;
    }

    m_config.wakeupEnabled = enabled;
    if (m_trayController) {
        m_trayController->setWakeupChecked(enabled);
    }
    rebuildVoiceInputService();
    saveConfigWithNotification(QStringLiteral("语音唤醒切换"));

    if (m_trayController) {
        QString message;
        if (!enabled) {
            message = QStringLiteral("已关闭：不会再自动监听唤醒词。");
        } else if (!m_config.microphoneEnabled) {
            message = QStringLiteral("已开启，但麦克风未启用；请先开启麦克风。");
        } else if (m_config.offlineMode) {
            message = QStringLiteral("已开启，但当前处于离线模式；联网后连续唤醒才会生效。");
        } else {
            message = QStringLiteral("已开启：将按当前唤醒词配置进行连续监听。");
        }
        m_trayController->showStartupMessage(QStringLiteral("语音唤醒"), message);
    }
}

void ApplicationBootstrap::setEyeTrackingEnabled(bool enabled)
{
    if (m_config.eyeTrackingEnabled == enabled) {
        if (m_trayController) {
            m_trayController->setEyeTrackingChecked(enabled);
        }
        return;
    }

    m_config.eyeTrackingEnabled = enabled;
    if (m_trayController) {
        m_trayController->setEyeTrackingChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setEyeTrackingEnabled(enabled);
    }
    saveConfigWithNotification(QStringLiteral("视线跟踪切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("视线跟踪"),
            enabled
                ? QStringLiteral("已开启：角色会根据检测到的人脸位置进行 gaze follow。")
                : QStringLiteral("已关闭：角色不再根据人脸位置移动视线。"));
    }
}

void ApplicationBootstrap::setPeriodicScanEnabled(bool enabled)
{
    if (m_config.periodicScanEnabled == enabled) {
        if (m_trayController) {
            m_trayController->setPeriodicScanChecked(enabled);
        }
        return;
    }

    m_config.periodicScanEnabled = enabled;
    if (m_trayController) {
        m_trayController->setPeriodicScanChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setPeriodicScanEnabled(enabled);
    }
    saveConfigWithNotification(QStringLiteral("周期巡检切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("周期巡检"),
            enabled
                ? QStringLiteral("已开启：角色会按设定间隔做摄像头 presence 巡检。")
                : QStringLiteral("已关闭：不会再自动发起周期性摄像头巡检。"));
    }
}

void ApplicationBootstrap::setAudioReactiveEnabled(bool enabled)
{
    if (m_config.audioOutputReactive == enabled) {
        if (m_trayController) {
            m_trayController->setAudioReactiveChecked(enabled);
        }
        return;
    }

    m_config.audioOutputReactive = enabled;
    if (m_trayController) {
        m_trayController->setAudioReactiveChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setAudioOutputReactive(enabled);
    }
    rebuildAudioOutputMonitorService();
    saveConfigWithNotification(QStringLiteral("音频反应切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("音频反应"),
            enabled
                ? QStringLiteral("已开启：外部媒体播放会影响角色行为模式。")
                : QStringLiteral("已关闭：角色不再根据系统音频输出切换行为。"));
    }
}

void ApplicationBootstrap::setContinuousVoiceModeEnabled(bool enabled)
{
    const QString targetMode = enabled ? QStringLiteral("continuous") : QStringLiteral("push_to_talk");
    if (m_config.voiceInputMode.trimmed().compare(targetMode, Qt::CaseInsensitive) == 0) {
        if (m_trayController) {
            m_trayController->setContinuousVoiceModeChecked(enabled);
        }
        return;
    }

    m_config.voiceInputMode = targetMode;
    if (m_trayController) {
        m_trayController->setContinuousVoiceModeChecked(enabled);
    }
    rebuildVoiceInputService();
    saveConfigWithNotification(QStringLiteral("语音输入模式切换"));

    if (m_trayController) {
        QString message;
        if (enabled) {
            if (!m_config.microphoneEnabled) {
                message = QStringLiteral("已切到连续唤醒模式，但麦克风未启用。");
            } else if (m_config.offlineMode) {
                message = QStringLiteral("已切到连续唤醒模式，但当前处于离线模式。");
            } else {
                message = QStringLiteral("已切到连续唤醒模式。");
            }
        } else {
            message = QStringLiteral("已切到按键转写模式（Ctrl+B）。");
        }
        m_trayController->showStartupMessage(QStringLiteral("语音模式"), message);
    }
}

void ApplicationBootstrap::setScriptedEntranceEnabled(bool enabled)
{
    if (m_config.scriptedEntranceEnabled == enabled) {
        if (m_trayController) {
            m_trayController->setScriptedEntranceChecked(enabled);
        }
        return;
    }

    m_config.scriptedEntranceEnabled = enabled;
    if (m_trayController) {
        m_trayController->setScriptedEntranceChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setScriptedEntranceEnabled(enabled);
    }
    saveConfigWithNotification(QStringLiteral("脚本式登场切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("脚本式登场"),
            enabled
                ? QStringLiteral("已开启：隐藏态召唤会优先尝试 trajectory 登场。")
                : QStringLiteral("已关闭：召唤将回退到普通 peek/engage 进入。"));
    }
}

void ApplicationBootstrap::setFullscreenPauseEnabled(bool enabled)
{
    if (m_config.fullScreenPause == enabled) {
        if (m_trayController) {
            m_trayController->setFullscreenPauseChecked(enabled);
        }
        return;
    }

    m_config.fullScreenPause = enabled;
    if (m_trayController) {
        m_trayController->setFullscreenPauseChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setFullScreenPauseEnabled(enabled);
    }
    startVisionServiceIfNeeded();
    saveConfigWithNotification(QStringLiteral("全屏暂停切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("全屏暂停"),
            enabled
                ? QStringLiteral("已开启：检测到全屏应用时会暂停角色和视觉资源。")
                : QStringLiteral("已关闭：全屏时不再自动暂停角色。"));
    }
}

void ApplicationBootstrap::setIdleInvasionEnabled(bool enabled)
{
    if (m_config.idleInvasion.enabled == enabled) {
        if (m_trayController) {
            m_trayController->setIdleInvasionChecked(enabled);
        }
        return;
    }

    m_config.idleInvasion.enabled = enabled;
    if (m_trayController) {
        m_trayController->setIdleInvasionChecked(enabled);
    }
    if (m_runtimeDirector) {
        m_runtimeDirector->setIdleInvasionEnabled(enabled);
        m_runtimeDirector->setIdleInvasionStartDelayMs(m_config.idleInvasion.startDelayMs);
    }
    if (m_idleInvasionController) {
        m_idleInvasionController->applyConfig(m_config.idleInvasion);
        m_idleInvasionController->setDndEnabled(m_trayController ? m_trayController->isDndChecked() : false);
    }
    saveConfigWithNotification(QStringLiteral("空闲入侵切换"));

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("空闲入侵"),
            enabled
                ? QStringLiteral("已开启：长时间空闲后会按配置逐步生成入侵角色。")
                : QStringLiteral("已关闭：空闲入侵不会再自动启动。"));
    }
}

void ApplicationBootstrap::showSettingsDialog()
{
    const QVector<CharacterManifest> manifests = m_characterManifestCatalog
        ? m_characterManifestCatalog->manifests()
        : QVector<CharacterManifest>{};
    const QString previousCharacterId = m_config.activeCharacterId;
    SettingsDialog dialog(m_config, manifests, nullptr);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_config = dialog.editedConfig();
    if (m_characterManifestCatalog
        && m_config.activeCharacterId.compare(previousCharacterId, Qt::CaseInsensitive) != 0) {
        const CharacterManifest manifest = m_characterManifestCatalog->findById(m_config.activeCharacterId);
        if (manifest.isValid()) {
            applyCharacterManifest(manifest, false, false);
        }
    }
    if (m_configRepository) {
        m_config.version = QCoreApplication::applicationVersion();
        saveConfigWithNotification(QStringLiteral("设置保存"));
    }
    syncAutoStartSetting();

    if (m_runtimeDirector) {
        m_runtimeDirector->setScriptedEntranceEnabled(m_config.scriptedEntranceEnabled);
        m_runtimeDirector->setScriptedTrajectoryPath(m_config.scriptedTrajectoryPath);
        m_runtimeDirector->setVoiceScriptsPath(m_config.voiceScriptsPath);
        m_runtimeDirector->setAudioOutputReactive(m_config.audioOutputReactive);
        m_runtimeDirector->setAutoDismissSeconds(m_config.autoDismissSeconds);
        m_runtimeDirector->setIdleInvasionEnabled(m_config.idleInvasion.enabled);
        m_runtimeDirector->setIdleInvasionStartDelayMs(m_config.idleInvasion.startDelayMs);
        m_runtimeDirector->setOfflineMode(m_config.offlineMode);
        m_runtimeDirector->setScreenCommentaryAutoEnabled(m_config.screenCommentaryAutoEnabled);
        m_runtimeDirector->setScreenCommentaryAutoIntervalMinutes(m_config.screenCommentaryAutoIntervalMinutes);
        m_runtimeDirector->setScreenCommentarySpeechOptions(
            m_config.commentaryStreamingEnabled,
            m_config.commentaryStreamChunkChars,
            m_config.commentaryMaxResponseChars,
            m_config.commentaryPreambleText);
        m_runtimeDirector->setCameraEnabled(m_config.cameraEnabled && m_config.cameraConsentGranted);
        m_runtimeDirector->setPresenceTargetFps(m_config.cameraTargetFps);
        m_runtimeDirector->setEyeTrackingEnabled(m_config.eyeTrackingEnabled);
        m_runtimeDirector->setPeriodicScanEnabled(m_config.periodicScanEnabled);
        m_runtimeDirector->setPeriodicScanIntervalMinutes(m_config.periodicScanIntervalMinutes);
        m_runtimeDirector->setFullScreenPauseEnabled(m_config.fullScreenPause);
        m_runtimeDirector->setResidentModeEnabled(m_config.residentMode);
    }
    if (m_idleInvasionController) {
        m_idleInvasionController->applyConfig(m_config.idleInvasion);
    }
    if (m_audioService) {
        m_audioService->setTtsProvider(m_config.ttsProvider);
        m_audioService->configureVoice(m_config.ttsVoice, m_config.ttsRate);
        m_audioService->setVolume(m_config.audioVolume);
        m_audioService->setCacheEnabled(m_config.audioCacheEnabled);
    }
    if (m_entityWidget) {
        m_entityWidget->setScreenEdge(preferredEdgeForConfig(m_config));
        m_entityWidget->applyAppearanceConfig(m_config.appearanceAsciiWidth, m_config.appearanceFontSizePx);
        if (m_config.preferredPosition.trimmed().compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0) {
            m_entityWidget->moveToDefaultCorner();
        }
    }
    rearmIdleMonitorThreshold();
    rebuildAudioOutputMonitorService();
    rebuildScreenCommentaryService();
    rebuildVisionService();
    rebuildVoiceInputService();
    maybeWarnLegacyTtsProviderMigration();
    maybeWarnLegacyAsrProviderMigration();
    if (m_trayController) {
        m_trayController->setOfflineChecked(m_config.offlineMode);
        m_trayController->setResidentChecked(m_config.residentMode);
        m_trayController->setAutoCommentaryChecked(m_config.screenCommentaryAutoEnabled);
        m_trayController->setCameraChecked(m_config.cameraEnabled);
        m_trayController->setMicrophoneChecked(m_config.microphoneEnabled);
        m_trayController->setWakeupChecked(m_config.wakeupEnabled);
        m_trayController->setEyeTrackingChecked(m_config.eyeTrackingEnabled);
        m_trayController->setPeriodicScanChecked(m_config.periodicScanEnabled);
        m_trayController->setAudioReactiveChecked(m_config.audioOutputReactive);
        m_trayController->setContinuousVoiceModeChecked(
            m_config.voiceInputMode.trimmed().compare(QStringLiteral("continuous"), Qt::CaseInsensitive) == 0);
        m_trayController->setScriptedEntranceChecked(m_config.scriptedEntranceEnabled);
        m_trayController->setFullscreenPauseChecked(m_config.fullScreenPause);
        m_trayController->setIdleInvasionChecked(m_config.idleInvasion.enabled);
    }
    if (m_config.cameraEnabled && !m_config.cameraConsentGranted) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("camera-consent-missing"),
            QStringLiteral("摄像头"),
            QStringLiteral("已启用摄像头相关功能，但尚未授权访问。当前不会启动视觉服务。"));
    }

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("设置已保存"),
            QStringLiteral("关键设置已应用；部分运行态行为将在下次触发时生效。"));
    }
}

void ApplicationBootstrap::openQuickStartGuide()
{
    QDesktopServices::openUrl(AppPaths::quickStartGuideUrl());
}

void ApplicationBootstrap::copyRecentLogs()
{
    const QString logs = AppPaths::recentLogTail(50);
    if (logs.trimmed().isEmpty()) {
        if (m_trayController) {
            m_trayController->showStartupMessage(
                QStringLiteral("复制最近日志"),
                QStringLiteral("当前没有可复制的日志内容。"));
        }
        return;
    }

    if (QClipboard *clipboard = QApplication::clipboard()) {
        clipboard->setText(logs);
    }
    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("复制最近日志"),
            QStringLiteral("最近 50 行日志已复制到剪贴板。"));
    }
}

void ApplicationBootstrap::checkForUpdates()
{
    if (!m_trayController) {
        return;
    }
    if (m_config.offlineMode) {
        m_trayController->showStartupMessage(
            QStringLiteral("检查更新"),
            QStringLiteral("当前处于离线模式，已跳过联网更新检查。"));
        return;
    }
    if (m_updateCheckInFlight) {
        m_trayController->showStartupMessage(QStringLiteral("检查更新"), QStringLiteral("更新检查正在进行中，请稍候。"));
        return;
    }
    if (!m_updateNetworkAccessManager) {
        m_updateNetworkAccessManager = std::make_unique<QNetworkAccessManager>();
    }

    const LocalVersionManifest manifest = VersionManifest::loadLocal();
    const QString manifestVersion = VersionManifest::normalizeVersionString(manifest.version);
    const QString appVersion = VersionManifest::normalizeVersionString(QCoreApplication::applicationVersion());
    QString currentVersion = manifestVersion;
    if (currentVersion.isEmpty()
        || (!appVersion.isEmpty() && VersionManifest::compareVersions(currentVersion, appVersion) < 0)) {
        currentVersion = appVersion;
    }
    const QString envUpdateUrl = qEnvironmentVariable("CYBERCOMPANION_UPDATE_URL").trimmed();
    const QString updateUrl = !envUpdateUrl.isEmpty()
        ? envUpdateUrl
        : (!manifest.updateUrl.trimmed().isEmpty() ? manifest.updateUrl.trimmed() : VersionManifest::defaultUpdateUrl());

    if (updateUrl.isEmpty()) {
        showRuntimeErrorNotification(QStringLiteral("检查更新"), QStringLiteral("未找到更新源地址。"));
        return;
    }

    QNetworkRequest request{QUrl(updateUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CyberCompanionCpp/%1").arg(currentVersion));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(10000);

    m_updateCheckInFlight = true;
    QNetworkReply *reply = m_updateNetworkAccessManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, currentVersion]() {
        const auto finalize = [this, reply]() {
            m_updateCheckInFlight = false;
            reply->deleteLater();
        };

        if (reply->error() != QNetworkReply::NoError) {
            showRuntimeErrorNotification(
                QStringLiteral("检查更新"),
                QStringLiteral("获取最新版本失败：%1").arg(reply->errorString()));
            finalize();
            return;
        }

        const RemoteReleaseInfo release = VersionManifest::parseRemoteReleasePayload(reply->readAll());
        if (!release.isValid()) {
            showRuntimeErrorNotification(
                QStringLiteral("检查更新"),
                QStringLiteral("更新响应格式无效，无法解析最新版本信息。"));
            finalize();
            return;
        }

        const int compare = VersionManifest::compareVersions(currentVersion, release.version);
        if (compare < 0) {
            m_trayController->showStartupMessage(
                QStringLiteral("检查更新"),
                QStringLiteral("发现新版本 %1，正在打开发布页。").arg(release.version));
            QDesktopServices::openUrl(QUrl(release.htmlUrl));
        } else {
            m_trayController->showStartupMessage(
                QStringLiteral("检查更新"),
                QStringLiteral("当前已是最新版本：%1").arg(currentVersion));
        }

        finalize();
    });
}

void ApplicationBootstrap::openFeedbackIssue()
{
    QDesktopServices::openUrl(AppPaths::feedbackIssueUrl());
}

void ApplicationBootstrap::openConfigFile()
{
    const QString configFile = AppPaths::configFilePath();
    const QFileInfo info(configFile);
    if (!info.exists()) {
        saveConfigWithNotification(QStringLiteral("初始化配置文件"));
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(configFile));
}

void ApplicationBootstrap::openDataDirectory()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::userDataDir().absolutePath()));
}

void ApplicationBootstrap::openLogsDirectory()
{
    const QUrl logsUrl = QUrl::fromLocalFile(AppPaths::logDir().absolutePath());
    QDesktopServices::openUrl(logsUrl);
}

void ApplicationBootstrap::showAboutDialog()
{
    QMessageBox::information(
        nullptr,
        QStringLiteral("关于 CyberCompanionCpp"),
        QStringLiteral("CyberCompanionCpp 1.0.0\n\n"
                       "原生 Qt 6 / C++ 桌面运行时。\n\n"
                       "已覆盖：\n"
                       "- 托盘、单实例、日志、配置与自启动\n"
                       "- 角色窗口、脚本登场、自治行为与 idle invasion\n"
                       "- 热键、音频播放、语音输入、屏幕评论与更新检查\n"
                       "- 摄像头 presence / gaze / periodic scan 与状态联动\n"
                       "- 原生设置页、角色包切换、语音命令与发布安装链\n\n"
                       "更多细节请查看 README 与 docs/cpp_phase0.md。"));
}

void ApplicationBootstrap::handleQuitRequested()
{
    m_app.quit();
}

void ApplicationBootstrap::setupApplicationMetadata() const
{
    QCoreApplication::setOrganizationName(QStringLiteral("Aemeath"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("local.aemeath"));
    QCoreApplication::setApplicationName(QStringLiteral("CyberCompanionCpp"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
}

void ApplicationBootstrap::restoreRuntimeState()
{
    if (!m_entityWidget) {
        return;
    }

    m_entityWidget->setScreenEdge(preferredEdgeForConfig(m_config));
    m_entityWidget->restoreWindowPosition(m_config.windowX, m_config.windowY);
    if (!isForcedStartMinimizedLaunch() && !m_config.startMinimized && m_config.lastVisible) {
        m_entityWidget->show();
    } else {
        m_entityWidget->hide();
    }
}

void ApplicationBootstrap::persistRuntimeState()
{
    if (!m_configRepository) {
        return;
    }
    if (m_entityWidget) {
        m_config.windowX = m_entityWidget->x();
        m_config.windowY = m_entityWidget->y();
        m_config.lastVisible = m_entityWidget->isVisible();
    }
    m_config.version = QStringLiteral("1.0.0");
    saveConfigWithNotification(QStringLiteral("退出时保存状态"));
}

bool ApplicationBootstrap::saveConfigWithNotification(const QString &context)
{
    if (!m_configRepository) {
        return false;
    }

    QString errorMessage;
    if (m_configRepository->save(m_config, &errorMessage)) {
        return true;
    }

    const QString reason = errorMessage.trimmed().isEmpty()
        ? QStringLiteral("未知错误")
        : errorMessage.trimmed();
    showRuntimeErrorNotificationOnce(
        QStringLiteral("config-save:%1:%2").arg(context, reason),
        QStringLiteral("配置保存"),
        QStringLiteral("未能保存配置（%1）：%2").arg(context, reason));
    return false;
}

void ApplicationBootstrap::syncAutoStartSetting()
{
    if (!m_autoStartManager) {
        return;
    }

    QString errorMessage;
    if (m_autoStartManager->syncEnabled(
            m_config.autoStartOnLogin,
            QCoreApplication::applicationFilePath(),
            m_config.startMinimized,
            &errorMessage)) {
        return;
    }

    const QString reason = errorMessage.trimmed().isEmpty()
        ? QStringLiteral("未知错误")
        : errorMessage.trimmed();
    showRuntimeErrorNotificationOnce(
        QStringLiteral("autostart-sync:%1:%2")
            .arg(m_config.autoStartOnLogin ? QStringLiteral("on") : QStringLiteral("off"), reason),
        QStringLiteral("开机自启动"),
        QStringLiteral("未能同步开机自启动设置：%1").arg(reason));
}

bool ApplicationBootstrap::isAutoStartLaunch() const
{
    return m_app.property("launch.source.autostart").toBool();
}

bool ApplicationBootstrap::isForcedStartMinimizedLaunch() const
{
    return m_app.property("launch.forceStartMinimized").toBool();
}

void ApplicationBootstrap::showRuntimeErrorNotification(const QString &feature, const QString &message)
{
    const QString featureTitle = feature.trimmed().isEmpty()
        ? QStringLiteral("运行时")
        : feature.trimmed();
    const QString summary = summarizedRuntimeError(message);
    qWarning().noquote() << QStringLiteral("[UserVisibleError]") << featureTitle << summary;
    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("%1 出错了").arg(featureTitle),
            summary);
    }
}

void ApplicationBootstrap::showRuntimeErrorNotificationOnce(
    const QString &dedupeKey,
    const QString &feature,
    const QString &message)
{
    const QString cleanedKey = dedupeKey.trimmed();
    if (cleanedKey.isEmpty()) {
        showRuntimeErrorNotification(feature, message);
        return;
    }
    if (m_notifiedRuntimeErrorKeys.contains(cleanedKey)) {
        return;
    }
    m_notifiedRuntimeErrorKeys.insert(cleanedKey);
    showRuntimeErrorNotification(feature, message);
}

void ApplicationBootstrap::wireSignals()
{
    connect(&m_app, &QCoreApplication::aboutToQuit, this, [this]() {
        stopScriptedTrajectoryWatchdog();
        persistRuntimeState();
        if (m_hotkeyService) {
            m_hotkeyService->stop();
        }
        if (m_audioOutputMonitorService && m_config.audioOutputReactive) {
            m_audioOutputMonitorService->stop();
        }
        if (m_idleMonitorService) {
            m_idleMonitorService->stop();
        }
        if (m_visionService) {
            m_visionService->stop();
        }
        if (m_voiceInputService) {
            m_voiceInputService->stop();
        }
        if (m_fullscreenMonitorService) {
            m_fullscreenMonitorService->stop();
        }
        if (m_idleInvasionController) {
            m_idleInvasionController->shutdown();
        }
        if (m_screenCommentaryService) {
            m_screenCommentaryService->cancel();
        }
        if (m_audioService) {
            m_audioService->shutdown();
        }
        if (m_singleInstanceGuard) {
            m_singleInstanceGuard->release();
        }
    });

    connect(m_trayController.get(), &TrayController::toggleRequested, this, &ApplicationBootstrap::toggleEntityVisibility);
    connect(m_trayController.get(), &TrayController::showRequested, this, &ApplicationBootstrap::showEntity);
    connect(m_trayController.get(), &TrayController::hideRequested, this, &ApplicationBootstrap::hideEntity);
    connect(m_trayController.get(), &TrayController::summonRequested, m_runtimeDirector.get(), &RuntimeDirector::summonNow);
    connect(m_trayController.get(), &TrayController::scriptedTrajectoryRequested, this, [this]() {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerScriptedTrajectoryDebug();
        }
    });
    connect(m_trayController.get(), &TrayController::sadComfortDebugRequested, this, [this]() {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerSadComfortDebug();
        }
    });
    connect(m_trayController.get(), &TrayController::noFaceDebugRequested, this, [this]() {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerNoFaceTestDebug();
        }
    });
    connect(m_trayController.get(), &TrayController::peekRequested, m_runtimeDirector.get(), &RuntimeDirector::triggerPeek);
    connect(m_trayController.get(), &TrayController::fleeRequested, m_runtimeDirector.get(), &RuntimeDirector::triggerFlee);
    connect(m_trayController.get(), &TrayController::demoTrajectoryRequested, m_runtimeDirector.get(), &RuntimeDirector::playDemoTrajectory);
    connect(m_trayController.get(), &TrayController::commentaryRequested, m_runtimeDirector.get(), &RuntimeDirector::requestScreenCommentary);
    connect(m_trayController.get(), &TrayController::cameraScanDebugRequested, this, &ApplicationBootstrap::requestCameraDebugScan);
    connect(m_trayController.get(), &TrayController::invasionDebugRequested, this, [this]() {
        if (m_idleInvasionController) {
            m_idleInvasionController->triggerDebugInvasion();
        }
    });
    connect(m_trayController.get(), &TrayController::dndToggled, this, &ApplicationBootstrap::setDndModeEnabled);
    connect(m_trayController.get(), &TrayController::offlineModeToggled, this, &ApplicationBootstrap::setOfflineModeEnabled);
    connect(m_trayController.get(), &TrayController::residentModeToggled, this, &ApplicationBootstrap::setResidentModeEnabled);
    connect(m_trayController.get(), &TrayController::autoCommentaryToggled, this, &ApplicationBootstrap::setAutoCommentaryEnabled);
    connect(m_trayController.get(), &TrayController::cameraToggled, this, &ApplicationBootstrap::setCameraEnabled);
    connect(m_trayController.get(), &TrayController::microphoneToggled, this, &ApplicationBootstrap::setMicrophoneEnabled);
    connect(m_trayController.get(), &TrayController::wakeupToggled, this, &ApplicationBootstrap::setWakeupEnabled);
    connect(m_trayController.get(), &TrayController::eyeTrackingToggled, this, &ApplicationBootstrap::setEyeTrackingEnabled);
    connect(m_trayController.get(), &TrayController::periodicScanToggled, this, &ApplicationBootstrap::setPeriodicScanEnabled);
    connect(m_trayController.get(), &TrayController::audioReactiveToggled, this, &ApplicationBootstrap::setAudioReactiveEnabled);
    connect(m_trayController.get(), &TrayController::continuousVoiceModeToggled, this, &ApplicationBootstrap::setContinuousVoiceModeEnabled);
    connect(m_trayController.get(), &TrayController::scriptedEntranceToggled, this, &ApplicationBootstrap::setScriptedEntranceEnabled);
    connect(m_trayController.get(), &TrayController::fullscreenPauseToggled, this, &ApplicationBootstrap::setFullscreenPauseEnabled);
    connect(m_trayController.get(), &TrayController::idleInvasionToggled, this, &ApplicationBootstrap::setIdleInvasionEnabled);
    connect(m_trayController.get(), &TrayController::resetPositionRequested, this, &ApplicationBootstrap::resetEntityPosition);
    connect(m_trayController.get(), &TrayController::settingsRequested, this, &ApplicationBootstrap::showSettingsDialog);
    connect(m_trayController.get(), &TrayController::guideRequested, this, &ApplicationBootstrap::openQuickStartGuide);
    connect(m_trayController.get(), &TrayController::statusRequested, this, &ApplicationBootstrap::showStatusSummary);
    connect(m_trayController.get(), &TrayController::editScriptsRequested, this, &ApplicationBootstrap::editScriptsFile);
    connect(m_trayController.get(), &TrayController::reloadCharactersRequested, this, &ApplicationBootstrap::reloadCharacters);
    connect(m_trayController.get(), &TrayController::reloadScriptsRequested, this, &ApplicationBootstrap::reloadScripts);
    connect(m_trayController.get(), &TrayController::copyRecentLogsRequested, this, &ApplicationBootstrap::copyRecentLogs);
    connect(m_trayController.get(), &TrayController::checkUpdatesRequested, this, &ApplicationBootstrap::checkForUpdates);
    connect(m_trayController.get(), &TrayController::feedbackRequested, this, &ApplicationBootstrap::openFeedbackIssue);
    connect(m_trayController.get(), &TrayController::characterSwitchRequested, this, &ApplicationBootstrap::switchCharacter);
    connect(m_trayController.get(), &TrayController::openConfigRequested, this, &ApplicationBootstrap::openConfigFile);
    connect(m_trayController.get(), &TrayController::openDataDirRequested, this, &ApplicationBootstrap::openDataDirectory);
    connect(m_trayController.get(), &TrayController::openLogsRequested, this, &ApplicationBootstrap::openLogsDirectory);
    connect(m_trayController.get(), &TrayController::aboutRequested, this, &ApplicationBootstrap::showAboutDialog);
    connect(m_trayController.get(), &TrayController::quitRequested, this, &ApplicationBootstrap::handleQuitRequested);

    connect(m_hotkeyService.get(), &HotkeyService::summonRequested, m_runtimeDirector.get(), &RuntimeDirector::summonNow);
    connect(m_hotkeyService.get(), &HotkeyService::pushToTalkRequested, this, [this]() {
        if (m_voiceInputService) {
            m_voiceInputService->startPushToTalkOnce();
        }
    });
    connect(m_idleMonitorService.get(), &IdleMonitorService::idleDetected, m_runtimeDirector.get(), &RuntimeDirector::onUserIdleDetected);
    connect(m_idleMonitorService.get(), &IdleMonitorService::idleTimeUpdated, m_runtimeDirector.get(), &RuntimeDirector::onIdleTimeUpdated);
    connect(m_idleMonitorService.get(), &IdleMonitorService::activityDetected, m_runtimeDirector.get(), &RuntimeDirector::onUserActivityDetected);
    connect(m_idleMonitorService.get(), &IdleMonitorService::idleTimeUpdated, m_idleInvasionController.get(), &IdleInvasionController::onIdleTimeUpdated);
    connect(m_idleMonitorService.get(), &IdleMonitorService::activityDetected, m_idleInvasionController.get(), &IdleInvasionController::onUserActivityDetected);
    connect(m_fullscreenMonitorService.get(), &FullscreenMonitorService::fullscreenChanged, m_runtimeDirector.get(), &RuntimeDirector::onFullscreenStateChanged);
    connect(m_fullscreenMonitorService.get(), &FullscreenMonitorService::fullscreenChanged, this, [this](bool fullscreenActive) {
        m_fullscreenActive = fullscreenActive;
        startVisionServiceIfNeeded();
    });
    connect(m_entityWidget.get(), &EntityWidget::doubleClicked, this, &ApplicationBootstrap::toggleEntityVisibility);
    connect(m_entityWidget.get(), &EntityWidget::contextMenuRequested, this, &ApplicationBootstrap::showEntityContextMenu);
    connect(m_runtimeDirector.get(), &RuntimeDirector::behaviorModeChanged, this, [this](RuntimeDirector::BehaviorMode mode) {
        if (!m_entityWidget) {
            return;
        }
        m_entityWidget->clearScriptVisualOverride();
        switch (mode) {
        case RuntimeDirector::BehaviorMode::Idle:
            m_entityWidget->setStateByName(QStringLiteral("state1"));
            m_entityWidget->setAutonomousEnabled(true);
            break;
        case RuntimeDirector::BehaviorMode::Busy:
            m_entityWidget->setStateByName(QStringLiteral("state4"));
            m_entityWidget->setAutonomousEnabled(false);
            break;
        case RuntimeDirector::BehaviorMode::MediaPlaying:
            m_entityWidget->setStateByName(QStringLiteral("state3"));
            m_entityWidget->setAutonomousEnabled(false);
            break;
        case RuntimeDirector::BehaviorMode::Summoning:
            m_entityWidget->setStateByName(QStringLiteral("state6"));
            m_entityWidget->setAutonomousEnabled(false);
            break;
        case RuntimeDirector::BehaviorMode::Commentary:
            m_entityWidget->setStateByName(QStringLiteral("state2"));
            m_entityWidget->setAutonomousEnabled(false);
            break;
        }
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::stateChanged, this, [this](RuntimeDirector::EntityState state) {
        m_presenceTrackingVisible =
            state == RuntimeDirector::EntityState::Peeking
            || state == RuntimeDirector::EntityState::Engaged;
        startVisionServiceIfNeeded();
        if (state == RuntimeDirector::EntityState::Hidden) {
            rearmIdleMonitorThreshold();
        }
        if (!m_entityWidget) {
            return;
        }
        if (m_audioService
            && (state == RuntimeDirector::EntityState::Hidden || state == RuntimeDirector::EntityState::Fleeing)) {
            m_audioService->interrupt();
        }
        if (m_screenCommentaryService
            && (state == RuntimeDirector::EntityState::Hidden || state == RuntimeDirector::EntityState::Fleeing)) {
            m_screenCommentaryService->cancel();
        }
        if ((state == RuntimeDirector::EntityState::Hidden || state == RuntimeDirector::EntityState::Fleeing)
            && m_trajectoryPlayer
            && m_trajectoryPlayer->isPlaying()) {
            stopScriptedTrajectoryWatchdog();
            m_trajectoryPlayer->stop();
        }
        EntityWidget::VisualState visualState = EntityWidget::VisualState::Idle;
        switch (state) {
        case RuntimeDirector::EntityState::Hidden:
            visualState = EntityWidget::VisualState::Hidden;
            break;
        case RuntimeDirector::EntityState::Peeking:
            visualState = EntityWidget::VisualState::Peeking;
            break;
        case RuntimeDirector::EntityState::Engaged:
            visualState = EntityWidget::VisualState::Engaged;
            break;
        case RuntimeDirector::EntityState::Fleeing:
            visualState = EntityWidget::VisualState::Fleeing;
            break;
        }
        m_entityWidget->transitionToVisualState(visualState);
        m_entityWidget->raise();
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::statusTextChanged, m_entityWidget.get(), &EntityWidget::setStatus);
    connect(m_runtimeDirector.get(), &RuntimeDirector::visionRuntimeRequested, this, [this](bool) {
        startVisionServiceIfNeeded(true);
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::voiceVisualRequested, m_entityWidget.get(), &EntityWidget::setScriptVisualOverride);
    connect(m_runtimeDirector.get(), &RuntimeDirector::entityStateOverrideRequested, this, [this](const QString &stateName) {
        if (m_entityWidget) {
            m_entityWidget->setStateByName(stateName, false);
        }
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::periodicCameraScanCompleted, this, [this](bool) {
        if (!m_pendingCameraDebugScan && !visionRuntimeRequested() && m_visionService) {
            m_visionService->stop();
        }
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::prolongedIdlePulseRequested, this, [this]() {
        if (!m_entityWidget || !m_runtimeDirector) {
            return;
        }
        if (m_runtimeDirector->currentState() != RuntimeDirector::EntityState::Hidden) {
            return;
        }
        m_entityWidget->peek();
        QTimer::singleShot(2200, this, [this]() {
            if (!m_entityWidget || !m_runtimeDirector) {
                return;
            }
            if (m_runtimeDirector->currentState() == RuntimeDirector::EntityState::Hidden) {
                m_entityWidget->hideNow();
            }
        });
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::gazeFollowRequested, m_entityWidget.get(), &EntityWidget::applyGazeFollow);
    connect(m_runtimeDirector.get(), &RuntimeDirector::trajectoryRequested, m_trajectoryPlayer.get(), &TrajectoryPlayer::play);
    connect(m_runtimeDirector.get(), &RuntimeDirector::scriptedTrajectoryRequested, this, [this](const QString &filePath) {
        if (!m_trajectoryPlayer || !m_entityWidget) {
            return;
        }
        m_entityWidget->hideNow();
        if (!m_trajectoryPlayer->playTimelineFile(filePath)) {
            stopScriptedTrajectoryWatchdog();
            m_runtimeDirector->onScriptedTrajectoryFinished();
            return;
        }
        m_scriptedTrajectoryWatchdog->start(scriptedTrajectoryWatchdogMs(*m_trajectoryPlayer));
    });
    connect(m_trajectoryPlayer.get(), &TrajectoryPlayer::stateCue, this, [this](int stateId) {
        if (!m_entityWidget) {
            return;
        }
        m_entityWidget->setStateByName(QStringLiteral("state%1").arg(stateId));
    });
    connect(m_trajectoryPlayer.get(), &TrajectoryPlayer::timelineFinished, this, [this]() {
        stopScriptedTrajectoryWatchdog();
        m_runtimeDirector->onScriptedTrajectoryFinished();
    });
    connect(m_trajectoryPlayer.get(), &TrajectoryPlayer::timelineAborted, this, [this]() {
        stopScriptedTrajectoryWatchdog();
    });
    connect(m_scriptedTrajectoryWatchdog, &QTimer::timeout, this, [this]() {
        if (!m_trajectoryPlayer || !m_runtimeDirector) {
            return;
        }
        if (m_trajectoryPlayer->isPlaying()) {
            m_trajectoryPlayer->stop();
        }
        m_runtimeDirector->onScriptedTrajectoryFinished();
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::speechRequestRequested, m_audioService.get(), &AudioService::speakRequest);
    connect(m_audioService.get(), &AudioService::playbackStarted, m_runtimeDirector.get(), &RuntimeDirector::onSelfPlaybackStarted);
    connect(m_audioService.get(), &AudioService::playbackFinished, m_runtimeDirector.get(), &RuntimeDirector::onSelfPlaybackFinished);
    connect(m_audioService.get(), &AudioService::playbackWarning, this, [this](const QString &warning) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("audio-warning:%1").arg(warning.simplified().left(120)),
            QStringLiteral("语音播放"),
            warning);
    });
    connect(m_runtimeDirector.get(), &RuntimeDirector::screenCommentaryRequested, this, [this]() {
        if (m_trajectoryPlayer && m_trajectoryPlayer->isPlaying()) {
            stopScriptedTrajectoryWatchdog();
            m_trajectoryPlayer->stop();
        }
        if (m_entityWidget) {
            m_entityWidget->transitionToVisualState(EntityWidget::VisualState::Commentary);
        }
        m_screenCommentaryService->requestCommentary();
    });
}

void ApplicationBootstrap::rebuildAudioOutputMonitorService()
{
    if (m_audioOutputMonitorService) {
        m_audioOutputMonitorService->stop();
    }

    AudioOutputMonitorOptions options;
    options.pollIntervalMs = m_config.audioOutputPollIntervalMs;
    options.ignoreCurrentProcessAudio = m_config.audioMonitorIgnoreCurrentProcessAudio;
    options.preferMediaSessions = m_config.audioMonitorPreferMediaSessions;
    options.includeMasterPeakFallback = m_config.audioMonitorIncludeMasterPeakFallback;

    m_audioOutputMonitorService = std::make_unique<WindowsAudioOutputMonitorService>(options, this);
    connect(m_audioOutputMonitorService.get(), &AudioOutputMonitorService::audioOutputStarted, m_runtimeDirector.get(), &RuntimeDirector::onAudioOutputStarted);
    connect(m_audioOutputMonitorService.get(), &AudioOutputMonitorService::audioOutputStopped, m_runtimeDirector.get(), &RuntimeDirector::onAudioOutputStopped);

    if (m_initialized && m_config.audioOutputReactive) {
        m_audioOutputMonitorService->start();
    }
}

void ApplicationBootstrap::rebuildScreenCommentaryService()
{
    if (m_screenCommentaryService) {
        m_screenCommentaryService->cancel();
    }

    OpenAiCompatibleConfig commentaryConfig;
    commentaryConfig.provider = m_config.llmProvider;
    commentaryConfig.model = m_config.llmModel;
    commentaryConfig.apiKey = OpenAiCompatibleClient::resolveApiKey(m_config.llmProvider, m_config.llmApiKey);
    commentaryConfig.baseUrl = m_config.llmBaseUrl;
    commentaryConfig.offlineMode = m_config.offlineMode;
    commentaryConfig.commentarySystemPrompt = m_config.commentarySystemPrompt;
    commentaryConfig.commentaryUserPrompt = m_config.commentaryUserPrompt;
    commentaryConfig.commentaryNoImagePrompt = m_config.commentaryNoImagePrompt;
    commentaryConfig.commentaryMaxTokens = m_config.commentaryMaxTokens;
    commentaryConfig.commentaryTemperature = m_config.commentaryTemperature;
    commentaryConfig.commentaryStreamingEnabled = m_config.commentaryStreamingEnabled;
    commentaryConfig.commentaryOcrFallbackEnabled = m_config.commentaryOcrFallbackEnabled;
    commentaryConfig.commentaryStreamChunkChars = m_config.commentaryStreamChunkChars;
    commentaryConfig.commentaryMaxResponseChars = m_config.commentaryMaxResponseChars;
    commentaryConfig.commentaryPreambleText = m_config.commentaryPreambleText;

    m_screenCommentaryService = std::make_unique<OpenAiCommentaryService>(commentaryConfig, this);
    connect(m_screenCommentaryService.get(), &ScreenCommentaryService::commentaryReady, m_runtimeDirector.get(), &RuntimeDirector::onCommentaryReady);
    connect(m_screenCommentaryService.get(), &ScreenCommentaryService::commentaryFailed, m_runtimeDirector.get(), &RuntimeDirector::onCommentaryFailed);
    connect(m_screenCommentaryService.get(), &ScreenCommentaryService::commentaryFailed, this, [this](const QString &error) {
        showRuntimeErrorNotification(QStringLiteral("屏幕评论"), error);
    });
}

void ApplicationBootstrap::rebuildVisionService()
{
    if (m_visionService) {
        m_visionService->stop();
    }

    VisionServiceOptions options;
    options.cameraIndex = m_config.cameraIndex;
    options.targetFps = m_config.cameraTargetFps;

    m_visionService = std::make_unique<QtVisionService>(options, this);
    if (m_runtimeDirector) {
        connect(m_visionService.get(), &VisionService::gazeUpdated, m_runtimeDirector.get(), &RuntimeDirector::onGazeUpdated);
        connect(m_visionService.get(), &VisionService::cameraError, m_runtimeDirector.get(), &RuntimeDirector::onCameraError);
        connect(m_visionService.get(), &VisionService::cameraStateChanged, m_runtimeDirector.get(), &RuntimeDirector::onCameraStateChanged);
    }
    connect(m_visionService.get(), &VisionService::cameraStateChanged, this, [this](bool running) {
        if (!running || !m_pendingCameraDebugScan || !m_runtimeDirector) {
            return;
        }
        m_pendingCameraDebugScan = false;
        m_runtimeDirector->triggerPeriodicCameraCheckDebug();
    });
    connect(m_visionService.get(), &VisionService::cameraError, this, [this](const QString &message) {
        m_pendingCameraDebugScan = false;
        showRuntimeErrorNotificationOnce(
            QStringLiteral("vision-error:%1").arg(message.simplified().left(120)),
            QStringLiteral("摄像头"),
            message);
    });

    if (m_initialized) {
        startVisionServiceIfNeeded();
    }
}

void ApplicationBootstrap::rebuildVoiceInputService()
{
    if (m_voiceInputService) {
        m_voiceInputService->stop();
    }

    VoiceInputConfig config;
    config.microphoneEnabled = m_config.microphoneEnabled;
    config.offlineMode = m_config.offlineMode;
    config.voiceInputMode = m_config.voiceInputMode;
    config.asrProvider = m_config.asrProvider;
    config.asrApiKey = m_config.asrApiKey;
    config.asrModel = m_config.asrModel;
    config.asrBaseUrl = m_config.asrBaseUrl;
    config.asrTemperature = m_config.asrTemperature;
    config.asrPrompt = m_config.asrPrompt;
    config.wakeupEnabled = m_config.wakeupEnabled;
    config.wakeupPhrases = m_config.wakeupPhrases;
    config.wakeupLanguage = m_config.wakeupLanguage;
    config.llmApiKey = m_config.llmApiKey;
    config.llmBaseUrl = m_config.llmBaseUrl;

    m_voiceInputService = std::make_unique<OpenAiVoiceInputService>(this);
    m_voiceInputService->configure(config);
    connect(m_voiceInputService.get(), &VoiceInputService::transcriptReady, this, &ApplicationBootstrap::handleVoiceTranscript);
    connect(m_voiceInputService.get(), &VoiceInputService::listenerError, this, [this](const QString &message) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("voice-input-error:%1").arg(message.simplified().left(120)),
            QStringLiteral("语音输入"),
            message);
    });
    connect(m_voiceInputService.get(), &VoiceInputService::listenerWarning, this, [this](const QString &message) {
        if (m_trayController) {
            m_trayController->showStartupMessage(QStringLiteral("语音输入"), message);
        }
    });
    connect(m_voiceInputService.get(), &VoiceInputService::continuousListeningDegraded, this, [this](const QString &message) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("voice-wakeup-degraded:%1").arg(message.simplified().left(120)),
            QStringLiteral("语音降级"),
            QStringLiteral("%1\n本次会话已暂停连续唤醒，可在设置中修复后重试。").arg(message));
    });
    connect(m_voiceInputService.get(), &VoiceInputService::captureStateChanged, this, [this](bool active, const QString &source) {
        if (!active || !m_trayController) {
            return;
        }
        if (source == QStringLiteral("push_to_talk")) {
            m_trayController->showStartupMessage(QStringLiteral("语音转写"), QStringLiteral("开始收音，请说话…"));
        } else if (source == QStringLiteral("wakeup") && m_config.debugMode) {
            m_trayController->showStartupMessage(QStringLiteral("语音唤醒"), QStringLiteral("正在后台监听唤醒词。"));
        }
    });

    if (m_initialized && m_config.microphoneEnabled && !m_config.offlineMode) {
        m_voiceInputService->startContinuousListening();
    }
}

void ApplicationBootstrap::maybeWarnLegacyAsrProviderMigration()
{
    if (!m_config.asrProviderMigratedFromGoogle) {
        return;
    }
    if (!m_config.microphoneEnabled || m_config.offlineMode) {
        return;
    }
    showRuntimeErrorNotificationOnce(
        QStringLiteral("migrated-asr-provider-google"),
        QStringLiteral("语音输入"),
        QStringLiteral("检测到旧版 Google Web Speech 配置。原生 C++ 运行时已自动迁移为 zhipu_asr，请确认 ASR API Key、模型和 Base URL 设置。"));
}

void ApplicationBootstrap::maybeWarnLegacyTtsProviderMigration()
{
    if (!m_config.ttsProviderMigratedToEdge) {
        return;
    }

    showRuntimeErrorNotificationOnce(
        QStringLiteral("migrated-tts-provider-edge"),
        QStringLiteral("TTS"),
        QStringLiteral("检测到旧版非 edge TTS 配置。原生 C++ 运行时已自动回落到 edge-tts。"));
}

bool ApplicationBootstrap::visionRuntimeRequested() const
{
    return m_config.cameraEnabled
        && m_config.cameraConsentGranted
        && !(m_config.fullScreenPause && m_fullscreenActive)
        && (m_presenceTrackingVisible || m_pendingCameraDebugScan);
}

void ApplicationBootstrap::handleVoiceTranscript(const QString &text, const QString &source)
{
    const QString cleaned = text.trimmed();
    const bool wakeupSource = source.trimmed().compare(QStringLiteral("wakeup"), Qt::CaseInsensitive) == 0;
    if (cleaned.isEmpty()) {
        if (m_trayController) {
            m_trayController->showStartupMessage(QStringLiteral("语音转写"), QStringLiteral("未识别到有效语音，请重试。"));
        }
        return;
    }

    if (m_trayController) {
        m_trayController->showStartupMessage(QStringLiteral("语音转写"), cleaned);
    }

    if (m_characterManifestCatalog) {
        const CharacterManifest manifest =
            CharacterSwitchMatcher::match(cleaned, m_characterManifestCatalog->manifests());
        if (manifest.isValid()) {
            switchCharacter(manifest.id);
            if (m_trayController) {
                m_trayController->showStartupMessage(
                    QStringLiteral("语音命令"),
                    QStringLiteral("%1 -> switch_character (%2)")
                        .arg(cleaned)
                        .arg(manifest.id));
            }
            return;
        }
    }

    const VoiceCommandMatch match = VoiceCommandMatcher::match(cleaned, 66);
    if (match.action.isEmpty()) {
        if (wakeupSource && m_runtimeDirector) {
            m_runtimeDirector->summonNow();
            if (VoiceCommandMatcher::containsScreenIntent(cleaned)) {
                QTimer::singleShot(700, this, [this]() {
                    if (m_runtimeDirector) {
                        m_runtimeDirector->requestScreenCommentary();
                    }
                });
            }
            if (m_trayController) {
                m_trayController->showStartupMessage(
                    QStringLiteral("语音唤醒"),
                    VoiceCommandMatcher::containsScreenIntent(cleaned)
                        ? QStringLiteral("已识别唤醒词和屏幕意图，准备解读屏幕。")
                        : QStringLiteral("已识别唤醒词，角色正在现身。"));
            }
            return;
        }
        if (m_trayController) {
            m_trayController->showStartupMessage(
                QStringLiteral("语音命令"),
                QStringLiteral("未匹配到动作：%1").arg(cleaned));
        }
        return;
    }

    if (match.action == QStringLiteral("summon")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->summonNow();
        }
    } else if (match.action == QStringLiteral("screen_commentary")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->summonNow();
            QTimer::singleShot(700, this, [this]() {
                if (m_runtimeDirector) {
                    m_runtimeDirector->requestScreenCommentary();
                }
            });
        }
    } else if (match.action == QStringLiteral("hide")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->hideNow();
        }
    } else if (match.action == QStringLiteral("toggle_visibility")) {
        toggleEntityVisibility();
    } else if (match.action == QStringLiteral("status")) {
        if (m_trayController) {
            m_trayController->showStartupMessage(QStringLiteral("状态"), buildStatusSummary());
        }
        return;
    } else if (match.action == QStringLiteral("open_settings")) {
        showSettingsDialog();
        return;
    } else if (match.action == QStringLiteral("open_guide")) {
        openQuickStartGuide();
        return;
    } else if (match.action == QStringLiteral("edit_scripts")) {
        editScriptsFile();
        return;
    } else if (match.action == QStringLiteral("reload_characters")) {
        reloadCharacters();
        return;
    } else if (match.action == QStringLiteral("reload_scripts")) {
        reloadScripts();
        return;
    } else if (match.action == QStringLiteral("copy_recent_logs")) {
        copyRecentLogs();
        return;
    } else if (match.action == QStringLiteral("check_updates")) {
        checkForUpdates();
        return;
    } else if (match.action == QStringLiteral("open_config")) {
        openConfigFile();
        return;
    } else if (match.action == QStringLiteral("open_data_dir")) {
        openDataDirectory();
        return;
    } else if (match.action == QStringLiteral("open_logs")) {
        openLogsDirectory();
        return;
    } else if (match.action == QStringLiteral("feedback")) {
        openFeedbackIssue();
        return;
    } else if (match.action == QStringLiteral("about")) {
        showAboutDialog();
        return;
    } else if (match.action == QStringLiteral("dnd_on")) {
        setDndModeEnabled(true);
    } else if (match.action == QStringLiteral("dnd_off")) {
        setDndModeEnabled(false);
    } else if (match.action == QStringLiteral("offline_mode_on")) {
        setOfflineModeEnabled(true);
    } else if (match.action == QStringLiteral("offline_mode_off")) {
        setOfflineModeEnabled(false);
    } else if (match.action == QStringLiteral("resident_mode_on")) {
        setResidentModeEnabled(true);
    } else if (match.action == QStringLiteral("resident_mode_off")) {
        setResidentModeEnabled(false);
    } else if (match.action == QStringLiteral("camera_on")) {
        setCameraEnabled(true);
    } else if (match.action == QStringLiteral("camera_off")) {
        setCameraEnabled(false);
    } else if (match.action == QStringLiteral("microphone_on")) {
        setMicrophoneEnabled(true);
    } else if (match.action == QStringLiteral("microphone_off")) {
        setMicrophoneEnabled(false);
    } else if (match.action == QStringLiteral("wakeup_on")) {
        setWakeupEnabled(true);
    } else if (match.action == QStringLiteral("wakeup_off")) {
        setWakeupEnabled(false);
    } else if (match.action == QStringLiteral("eye_tracking_on")) {
        setEyeTrackingEnabled(true);
    } else if (match.action == QStringLiteral("eye_tracking_off")) {
        setEyeTrackingEnabled(false);
    } else if (match.action == QStringLiteral("periodic_scan_on")) {
        setPeriodicScanEnabled(true);
    } else if (match.action == QStringLiteral("periodic_scan_off")) {
        setPeriodicScanEnabled(false);
    } else if (match.action == QStringLiteral("audio_reactive_on")) {
        setAudioReactiveEnabled(true);
    } else if (match.action == QStringLiteral("audio_reactive_off")) {
        setAudioReactiveEnabled(false);
    } else if (match.action == QStringLiteral("voice_mode_continuous")) {
        setContinuousVoiceModeEnabled(true);
    } else if (match.action == QStringLiteral("voice_mode_push_to_talk")) {
        setContinuousVoiceModeEnabled(false);
    } else if (match.action == QStringLiteral("scripted_entrance_on")) {
        setScriptedEntranceEnabled(true);
    } else if (match.action == QStringLiteral("scripted_entrance_off")) {
        setScriptedEntranceEnabled(false);
    } else if (match.action == QStringLiteral("fullscreen_pause_on")) {
        setFullscreenPauseEnabled(true);
    } else if (match.action == QStringLiteral("fullscreen_pause_off")) {
        setFullscreenPauseEnabled(false);
    } else if (match.action == QStringLiteral("idle_invasion_on")) {
        setIdleInvasionEnabled(true);
    } else if (match.action == QStringLiteral("idle_invasion_off")) {
        setIdleInvasionEnabled(false);
    } else if (match.action == QStringLiteral("auto_commentary_on")) {
        setAutoCommentaryEnabled(true);
    } else if (match.action == QStringLiteral("auto_commentary_off")) {
        setAutoCommentaryEnabled(false);
    } else if (match.action == QStringLiteral("peek")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerPeek();
        }
    } else if (match.action == QStringLiteral("flee")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerFlee();
        }
    } else if (match.action == QStringLiteral("scripted_trajectory_debug")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerScriptedTrajectoryDebug();
        }
    } else if (match.action == QStringLiteral("camera_scan_debug")) {
        requestCameraDebugScan();
    } else if (match.action == QStringLiteral("invasion_debug")) {
        if (m_idleInvasionController) {
            m_idleInvasionController->triggerDebugInvasion();
        }
    } else if (match.action == QStringLiteral("no_face_debug")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerNoFaceTestDebug();
        }
    } else if (match.action == QStringLiteral("sad_comfort_debug")) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerSadComfortDebug();
        }
    }

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("语音命令"),
            QStringLiteral("%1 -> %2 (%3)")
                .arg(cleaned)
                .arg(match.action)
                .arg(match.score));
    }
}

QString ApplicationBootstrap::buildStatusSummary() const
{
    auto stateLabel = [](RuntimeDirector::EntityState state) {
        switch (state) {
        case RuntimeDirector::EntityState::Hidden:
            return QStringLiteral("Hidden");
        case RuntimeDirector::EntityState::Peeking:
            return QStringLiteral("Peeking");
        case RuntimeDirector::EntityState::Engaged:
            return QStringLiteral("Engaged");
        case RuntimeDirector::EntityState::Fleeing:
            return QStringLiteral("Fleeing");
        }
        return QStringLiteral("Unknown");
    };

    auto modeLabel = [](RuntimeDirector::BehaviorMode mode) {
        switch (mode) {
        case RuntimeDirector::BehaviorMode::Idle:
            return QStringLiteral("Idle");
        case RuntimeDirector::BehaviorMode::Busy:
            return QStringLiteral("Busy");
        case RuntimeDirector::BehaviorMode::MediaPlaying:
            return QStringLiteral("MediaPlaying");
        case RuntimeDirector::BehaviorMode::Summoning:
            return QStringLiteral("Summoning");
        case RuntimeDirector::BehaviorMode::Commentary:
            return QStringLiteral("Commentary");
        }
        return QStringLiteral("Unknown");
    };

    auto presenceLabel = [](PresenceState state) {
        switch (state) {
        case PresenceState::Unknown:
            return QStringLiteral("Unknown");
        case PresenceState::PresentActive:
            return QStringLiteral("PresentActive");
        case PresenceState::PresentPassive:
            return QStringLiteral("PresentPassive");
        case PresenceState::Absent:
            return QStringLiteral("Absent");
        }
        return QStringLiteral("Unknown");
    };

    if (!m_runtimeDirector) {
        return QStringLiteral("RuntimeDirector 未初始化。");
    }

    const QString characterLabel = m_config.activeCharacterId.trimmed().isEmpty()
        ? QStringLiteral("default")
        : m_config.activeCharacterId.trimmed();
    const QString dndLabel = m_runtimeDirector->isDndMode() ? QStringLiteral("on") : QStringLiteral("off");
    const QString audioLabel = m_runtimeDirector->isAudioOutputActive()
        ? QStringLiteral("playing")
        : QStringLiteral("idle");
    const QString cameraLabel = m_config.cameraEnabled && m_config.cameraConsentGranted
        ? QStringLiteral("enabled")
        : m_config.cameraEnabled
            ? QStringLiteral("awaiting_consent")
            : QStringLiteral("disabled");
    const QString microphoneLabel = m_config.microphoneEnabled ? QStringLiteral("on") : QStringLiteral("off");
    const bool remoteVoiceAvailable = m_config.microphoneEnabled && !m_config.offlineMode;
    const QString voiceModeLabel = !remoteVoiceAvailable
        ? QStringLiteral("off")
        : m_config.voiceInputMode.trimmed().compare(QStringLiteral("continuous"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("continuous")
            : QStringLiteral("push_to_talk");
    const QString wakeupLabel = remoteVoiceAvailable
        && m_config.voiceInputMode.trimmed().compare(QStringLiteral("continuous"), Qt::CaseInsensitive) == 0
        && m_config.wakeupEnabled
            ? QStringLiteral("on")
            : QStringLiteral("off");
    QStringList hotkeys;
    if (m_hotkeyService && m_hotkeyService->isSummonRegistered()) {
        hotkeys.append(QStringLiteral("summon"));
    }
    if (m_hotkeyService && m_hotkeyService->isPushToTalkRegistered()) {
        hotkeys.append(QStringLiteral("ptt"));
    }
    const QString hotkeyLabel = hotkeys.isEmpty() ? QStringLiteral("none") : hotkeys.join(QStringLiteral("+"));
    const QString networkLabel = m_config.offlineMode ? QStringLiteral("offline") : QStringLiteral("online");
    const QString residentLabel = m_config.residentMode ? QStringLiteral("on") : QStringLiteral("off");
    const QString autoStartLabel = m_config.autoStartOnLogin ? QStringLiteral("on") : QStringLiteral("off");
    const QString autoCommentaryLabel = m_config.screenCommentaryAutoEnabled ? QStringLiteral("on") : QStringLiteral("off");
    const QString eyeTrackingLabel = m_config.eyeTrackingEnabled ? QStringLiteral("on") : QStringLiteral("off");
    const QString periodicScanLabel = m_config.periodicScanEnabled ? QStringLiteral("on") : QStringLiteral("off");
    const QString audioReactiveLabel = m_config.audioOutputReactive ? QStringLiteral("on") : QStringLiteral("off");
    const QString asrProviderLabel = m_config.asrProvider.trimmed().isEmpty()
        ? QStringLiteral("default")
        : m_config.asrProvider.trimmed();
    const QString ttsProviderLabel = m_config.ttsProvider.trimmed().isEmpty()
        ? QStringLiteral("edge")
        : m_config.ttsProvider.trimmed();

    return QStringLiteral(
        "character=%1 | state=%2 | mode=%3 | presence=%4 | mood=%5(%6) | audio=%7 | dnd=%8 | resident=%9 | autostart=%10 | camera=%11 | eye=%12 | scan=%13 | mic=%14 | voice=%15 | wakeup=%16 | auto_commentary=%17 | audio_reactive=%18 | hotkeys=%19 | asr=%20 | tts=%21 | network=%22")
        .arg(characterLabel)
        .arg(stateLabel(m_runtimeDirector->currentState()))
        .arg(modeLabel(m_runtimeDirector->currentBehaviorMode()))
        .arg(presenceLabel(m_runtimeDirector->currentPresenceState()))
        .arg(m_runtimeDirector->currentMoodLabel())
        .arg(m_runtimeDirector->currentMoodValue(), 0, 'f', 2)
        .arg(audioLabel)
        .arg(dndLabel)
        .arg(residentLabel)
        .arg(autoStartLabel)
        .arg(cameraLabel)
        .arg(eyeTrackingLabel)
        .arg(periodicScanLabel)
        .arg(microphoneLabel)
        .arg(voiceModeLabel)
        .arg(wakeupLabel)
        .arg(autoCommentaryLabel)
        .arg(audioReactiveLabel)
        .arg(hotkeyLabel)
        .arg(asrProviderLabel)
        .arg(ttsProviderLabel)
        .arg(networkLabel);
}

QString ApplicationBootstrap::buildStartupSummary() const
{
    QStringList parts;
    if (m_config.cameraEnabled && m_config.cameraConsentGranted) {
        parts.append(QStringLiteral("摄像头: 开 (仅在角色出现/互动时启用)"));
    } else if (m_config.cameraEnabled) {
        parts.append(QStringLiteral("摄像头: 待授权"));
    } else {
        parts.append(QStringLiteral("摄像头: 关"));
    }

    const QString voiceMode = m_config.voiceInputMode.trimmed().toLower();
    if (m_config.offlineMode) {
        parts.append(QStringLiteral("语音模式: 离线模式已启用，远程转写与唤醒已停用"));
    } else if (voiceMode == QStringLiteral("push_to_talk")) {
        parts.append(QStringLiteral("语音模式: 按键转写 (全局 Ctrl+B)"));
    } else if (m_config.wakeupEnabled && m_config.microphoneEnabled) {
        const QString phrases = m_config.wakeupPhrases.isEmpty()
            ? QStringLiteral("未配置")
            : m_config.wakeupPhrases.join(QStringLiteral(", "));
        parts.append(QStringLiteral("语音唤醒: 开 (唤醒词: %1)").arg(phrases));
    } else {
        QStringList reasons;
        if (!m_config.wakeupEnabled) {
            reasons.append(QStringLiteral("唤醒未启用"));
        }
        if (!m_config.microphoneEnabled) {
            reasons.append(QStringLiteral("麦克风未启用"));
        }
        if (m_config.offlineMode) {
            reasons.append(QStringLiteral("离线模式"));
        }
        parts.append(QStringLiteral("语音唤醒: 关 (%1)").arg(joinReasons(reasons)));
    }

    QStringList shortcuts;
    if (m_hotkeyService && m_hotkeyService->isSummonRegistered()) {
        shortcuts.append(QStringLiteral("Ctrl+Shift+S 召唤伴侣"));
    }
    if (m_hotkeyService && m_hotkeyService->isPushToTalkRegistered()) {
        shortcuts.append(QStringLiteral("Ctrl+B 单次语音转写"));
    }
    parts.append(shortcuts.isEmpty()
        ? QStringLiteral("快捷键: 未注册（可能被其他程序占用）")
        : QStringLiteral("快捷键: %1").arg(shortcuts.join(QStringLiteral("；"))));

    if (m_config.debugMode) {
        parts.append(QStringLiteral("调试模式: 开"));
        parts.append(QStringLiteral("日志文件: %1").arg(AppPaths::logFilePath()));
    }
    parts.append(QStringLiteral("网络模式: %1").arg(m_config.offlineMode ? QStringLiteral("离线") : QStringLiteral("在线")));
    parts.append(QStringLiteral("TTS Provider: %1").arg(m_config.ttsProvider.trimmed().isEmpty() ? QStringLiteral("edge") : m_config.ttsProvider.trimmed()));
    parts.append(QStringLiteral("开机自启动: %1").arg(m_config.autoStartOnLogin ? QStringLiteral("开") : QStringLiteral("关")));

    return parts.join(QStringLiteral("\n"));
}

QString ApplicationBootstrap::activeScriptsPath() const
{
    if (!m_config.voiceScriptsPath.trimmed().isEmpty()) {
        return m_config.voiceScriptsPath.trimmed();
    }
    if (m_activeCharacterManifest.isValid() && !m_activeCharacterManifest.scriptsPath.trimmed().isEmpty()) {
        return m_activeCharacterManifest.scriptsPath.trimmed();
    }
    return AppPaths::resolveOptionalAsset(QStringLiteral("characters/default/scripts.json"));
}

void ApplicationBootstrap::showStatusSummary()
{
    if (m_trayController) {
        m_trayController->showStartupMessage(QStringLiteral("状态"), buildStatusSummary());
    }
}

void ApplicationBootstrap::editScriptsFile()
{
    const QString scriptsPath = activeScriptsPath();
    if (scriptsPath.trimmed().isEmpty()) {
        showRuntimeErrorNotification(QStringLiteral("台词编辑"), QStringLiteral("当前没有可编辑的 scripts.json 路径。"));
        return;
    }

    const QFileInfo info(scriptsPath);
    QDir().mkpath(info.dir().absolutePath());
    if (!info.exists()) {
        QSaveFile file(scriptsPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            showRuntimeErrorNotification(QStringLiteral("台词编辑"), file.errorString());
            return;
        }
        file.write("{\n  \"idle_events\": [],\n  \"panic_events\": []\n}\n");
        if (!file.commit()) {
            showRuntimeErrorNotification(QStringLiteral("台词编辑"), file.errorString());
            return;
        }
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(scriptsPath))) {
        showRuntimeErrorNotification(
            QStringLiteral("台词编辑"),
            QStringLiteral("无法打开 scripts.json：%1").arg(scriptsPath));
        return;
    }

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("台词编辑"),
            QStringLiteral("已打开 scripts.json，保存后点击“重载台词”生效。"));
    }
}

void ApplicationBootstrap::reloadCharacters()
{
    if (!m_characterManifestCatalog) {
        showRuntimeErrorNotification(QStringLiteral("重载角色"), QStringLiteral("当前未初始化角色目录索引。"));
        return;
    }

    m_characterManifestCatalog->reload();
    const QVector<CharacterManifest> manifests = m_characterManifestCatalog->manifests();
    if (m_trayController) {
        m_trayController->updateCharacters(manifests, m_config.activeCharacterId);
    }

    CharacterManifest manifest = m_characterManifestCatalog->findById(m_config.activeCharacterId);
    if (!manifest.isValid() && !manifests.isEmpty()) {
        manifest = manifests.constFirst();
    }
    if (manifest.isValid()) {
        applyCharacterManifest(manifest, true, false);
    }

    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("重载角色"),
            manifests.isEmpty()
                ? QStringLiteral("未找到可用角色 manifest。")
                : QStringLiteral("已重载 %1 个角色包。").arg(manifests.size()));
    }
}

void ApplicationBootstrap::reloadScripts()
{
    if (!m_runtimeDirector) {
        return;
    }

    const QString scriptsPath = activeScriptsPath();
    m_config.voiceScriptsPath = scriptsPath;
    m_runtimeDirector->setVoiceScriptsPath(scriptsPath);
    saveConfigWithNotification(QStringLiteral("重载台词"));

    if (m_trayController) {
        m_trayController->showStartupMessage(QStringLiteral("台词重载"), QStringLiteral("台词已重新加载。"));
    }
}

void ApplicationBootstrap::switchCharacter(const QString &characterId)
{
    if (!m_characterManifestCatalog) {
        showRuntimeErrorNotification(QStringLiteral("切换角色"), QStringLiteral("当前未初始化角色目录索引。"));
        return;
    }

    const CharacterManifest manifest = m_characterManifestCatalog->findById(characterId);
    if (!manifest.isValid()) {
        showRuntimeErrorNotification(
            QStringLiteral("切换角色"),
            QStringLiteral("角色加载失败：%1").arg(characterId));
        return;
    }

    applyCharacterManifest(manifest, true, true);
}

bool ApplicationBootstrap::applyCharacterManifest(const CharacterManifest &manifest, bool persistConfig, bool notifyUser)
{
    if (!manifest.isValid()) {
        return false;
    }

    m_activeCharacterManifest = manifest;
    m_config.activeCharacterId = manifest.id;
    if (!manifest.scriptsPath.trimmed().isEmpty()) {
        m_config.voiceScriptsPath = manifest.scriptsPath.trimmed();
    }
    if (!manifest.defaultVoice.trimmed().isEmpty()) {
        m_config.ttsVoice = manifest.defaultVoice.trimmed();
    }

    if (m_runtimeDirector) {
        m_runtimeDirector->setVoiceScriptsPath(m_config.voiceScriptsPath);
    }
    if (m_audioService) {
        m_audioService->configureVoice(m_config.ttsVoice, m_config.ttsRate);
        m_audioService->setVolume(m_config.audioVolume);
        m_audioService->setCacheEnabled(m_config.audioCacheEnabled);
    }
    if (m_entityWidget) {
        m_entityWidget->setScreenEdge(preferredEdgeForConfig(m_config));
        m_entityWidget->setCharacterAssetRoot(manifest.rootDir, manifest.previewImagePath);
    }
    if (m_trayController && m_characterManifestCatalog) {
        m_trayController->updateCharacters(m_characterManifestCatalog->manifests(), m_config.activeCharacterId);
    }
    if (persistConfig) {
        saveConfigWithNotification(QStringLiteral("切换角色"));
    }
    if (notifyUser && m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("切换角色"),
            QStringLiteral("已切换到：%1").arg(manifest.name.isEmpty() ? manifest.id : manifest.name));
    }
    return true;
}

void ApplicationBootstrap::startVisionServiceIfNeeded(bool force)
{
    if (!m_visionService) {
        return;
    }
    if (!(force || visionRuntimeRequested())) {
        m_visionService->stop();
        return;
    }
    m_visionService->start();
}

int ApplicationBootstrap::nextIdleThresholdMs() const
{
    const int baseIdleThresholdMs = qMax(1, m_config.idleThresholdSeconds) * 1000;
    const int minJitterSeconds = m_config.idleJitterMinSeconds;
    const int maxJitterSeconds = qMax(m_config.idleJitterMinSeconds, m_config.idleJitterMaxSeconds);
    const int jitterSeconds = QRandomGenerator::global()->bounded(minJitterSeconds, maxJitterSeconds + 1);
    return qMax(1000, baseIdleThresholdMs + jitterSeconds * 1000);
}

void ApplicationBootstrap::rearmIdleMonitorThreshold()
{
    if (!m_idleMonitorService) {
        return;
    }
    m_idleMonitorService->resetToStandby();
    m_idleMonitorService->setThresholdMs(nextIdleThresholdMs());
}

void ApplicationBootstrap::requestCameraDebugScan()
{
    if (!m_runtimeDirector) {
        return;
    }
    if (!m_config.cameraEnabled) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("camera-debug-disabled"),
            QStringLiteral("摄像头巡检"),
            QStringLiteral("当前未启用摄像头功能，无法执行调试巡检。"));
        return;
    }
    if (!m_config.cameraConsentGranted) {
        showRuntimeErrorNotificationOnce(
            QStringLiteral("camera-debug-consent"),
            QStringLiteral("摄像头巡检"),
            QStringLiteral("当前尚未授权访问摄像头，无法执行调试巡检。"));
        return;
    }

    m_pendingCameraDebugScan = true;
    startVisionServiceIfNeeded(true);
    if (m_runtimeDirector->triggerPeriodicCameraCheckDebug()) {
        m_pendingCameraDebugScan = false;
    }
    if (m_trayController) {
        m_trayController->showStartupMessage(
            QStringLiteral("调试摄像头巡检"),
            QStringLiteral("正在按需启动摄像头并准备采样。"));
    }
}

void ApplicationBootstrap::showEntityContextMenu(const QPoint &globalPos)
{
    QMenu menu;

    QAction *toggleAction = menu.addAction(QStringLiteral("显示/隐藏"));
    QAction *summonAction = menu.addAction(QStringLiteral("立即召唤"));
    QAction *invasionDebugAction = menu.addAction(QStringLiteral("调试空闲入侵"));
    QAction *trajectoryDebugAction = menu.addAction(QStringLiteral("调试轨迹登场"));
    QAction *sadComfortAction = menu.addAction(QStringLiteral("调试悲伤安慰"));
    QAction *noFaceAction = menu.addAction(QStringLiteral("调试无人脸提醒"));
    QAction *cameraScanAction = menu.addAction(QStringLiteral("调试摄像头巡检"));
    QAction *commentaryAction = menu.addAction(QStringLiteral("你在看什么？"));
    QAction *guideAction = menu.addAction(QStringLiteral("使用指南"));
    QAction *editScriptsAction = menu.addAction(QStringLiteral("编辑台词"));
    QAction *reloadScriptsAction = menu.addAction(QStringLiteral("重载台词"));
    QAction *dndAction = menu.addAction(QStringLiteral("请勿打扰"));
    dndAction->setCheckable(true);
    dndAction->setChecked(m_runtimeDirector && m_runtimeDirector->isDndMode());
    QAction *openLogsAction = menu.addAction(QStringLiteral("打开日志目录"));
    QAction *copyRecentLogsAction = menu.addAction(QStringLiteral("复制最近日志"));
    QAction *feedbackAction = menu.addAction(QStringLiteral("反馈问题"));
    QAction *settingsAction = menu.addAction(QStringLiteral("设置"));
    menu.addSeparator();
    QAction *quitAction = menu.addAction(QStringLiteral("退出"));

    QAction *chosen = menu.exec(globalPos);
    if (chosen == nullptr) {
        return;
    }

    if (chosen == toggleAction) {
        toggleEntityVisibility();
    } else if (chosen == summonAction) {
        if (m_runtimeDirector) {
            m_runtimeDirector->summonNow();
        }
    } else if (chosen == invasionDebugAction) {
        if (m_idleInvasionController) {
            m_idleInvasionController->triggerDebugInvasion();
        }
    } else if (chosen == trajectoryDebugAction) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerScriptedTrajectoryDebug();
        }
    } else if (chosen == sadComfortAction) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerSadComfortDebug();
        }
    } else if (chosen == noFaceAction) {
        if (m_runtimeDirector) {
            m_runtimeDirector->triggerNoFaceTestDebug();
        }
    } else if (chosen == cameraScanAction) {
        requestCameraDebugScan();
    } else if (chosen == commentaryAction) {
        if (m_runtimeDirector) {
            m_runtimeDirector->requestScreenCommentary();
        }
    } else if (chosen == guideAction) {
        openQuickStartGuide();
    } else if (chosen == editScriptsAction) {
        editScriptsFile();
    } else if (chosen == reloadScriptsAction) {
        reloadScripts();
    } else if (chosen == dndAction) {
        setDndModeEnabled(dndAction->isChecked());
    } else if (chosen == openLogsAction) {
        openLogsDirectory();
    } else if (chosen == copyRecentLogsAction) {
        copyRecentLogs();
    } else if (chosen == feedbackAction) {
        openFeedbackIssue();
    } else if (chosen == settingsAction) {
        showSettingsDialog();
    } else if (chosen == quitAction) {
        handleQuitRequested();
    }
}

void ApplicationBootstrap::stopScriptedTrajectoryWatchdog()
{
    if (m_scriptedTrajectoryWatchdog) {
        m_scriptedTrajectoryWatchdog->stop();
    }
}




