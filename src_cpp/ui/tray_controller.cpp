#include "ui/tray_controller.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPoint>
#include <QSignalBlocker>
#include <QStyle>
#include <QSystemTrayIcon>

TrayController::TrayController(const QIcon &icon, QObject *parent)
    : QObject(parent)
    , m_trayIcon(std::make_unique<QSystemTrayIcon>(this))
    , m_menu(std::make_unique<QMenu>())
{
    const QIcon resolvedIcon = icon.isNull()
        ? QApplication::style()->standardIcon(QStyle::SP_ComputerIcon)
        : icon;

    m_trayIcon->setIcon(resolvedIcon);
    refreshToolTip();

    m_characterMenu = m_menu->addMenu(QStringLiteral("切换角色"));
    QAction *showAction = m_menu->addAction(QStringLiteral("显示"));
    QAction *hideAction = m_menu->addAction(QStringLiteral("隐藏"));
    QAction *toggleAction = m_menu->addAction(QStringLiteral("显示/隐藏"));
    QAction *summonAction = m_menu->addAction(QStringLiteral("立即召唤"));
    QAction *scriptedTrajectoryAction = m_menu->addAction(QStringLiteral("调试轨迹登场"));
    QAction *sadComfortAction = m_menu->addAction(QStringLiteral("调试悲伤安慰"));
    QAction *noFaceAction = m_menu->addAction(QStringLiteral("调试无人脸提醒"));
    QAction *peekAction = m_menu->addAction(QStringLiteral("调试探头"));
    QAction *fleeAction = m_menu->addAction(QStringLiteral("调试撤退"));
    QAction *demoTrajectoryAction = m_menu->addAction(QStringLiteral("演示轨迹"));
    QAction *commentaryAction = m_menu->addAction(QStringLiteral("屏幕评论"));
    QAction *cameraScanDebugAction = m_menu->addAction(QStringLiteral("调试摄像头巡检"));
    QAction *invasionDebugAction = m_menu->addAction(QStringLiteral("空闲入侵调试"));
    m_dndAction = m_menu->addAction(QStringLiteral("请勿打扰"));
    m_dndAction->setCheckable(true);
    m_dndAction->setChecked(false);
    m_offlineAction = m_menu->addAction(QStringLiteral("离线模式"));
    m_offlineAction->setCheckable(true);
    m_offlineAction->setChecked(false);
    m_residentAction = m_menu->addAction(QStringLiteral("常驻模式"));
    m_residentAction->setCheckable(true);
    m_residentAction->setChecked(false);
    m_autoCommentaryAction = m_menu->addAction(QStringLiteral("自动评论"));
    m_autoCommentaryAction->setCheckable(true);
    m_autoCommentaryAction->setChecked(false);
    m_cameraAction = m_menu->addAction(QStringLiteral("启用摄像头"));
    m_cameraAction->setCheckable(true);
    m_cameraAction->setChecked(false);
    m_microphoneAction = m_menu->addAction(QStringLiteral("启用麦克风"));
    m_microphoneAction->setCheckable(true);
    m_microphoneAction->setChecked(false);
    m_wakeupAction = m_menu->addAction(QStringLiteral("启用语音唤醒"));
    m_wakeupAction->setCheckable(true);
    m_wakeupAction->setChecked(false);
    m_eyeTrackingAction = m_menu->addAction(QStringLiteral("启用视线跟踪"));
    m_eyeTrackingAction->setCheckable(true);
    m_eyeTrackingAction->setChecked(false);
    m_periodicScanAction = m_menu->addAction(QStringLiteral("启用周期巡检"));
    m_periodicScanAction->setCheckable(true);
    m_periodicScanAction->setChecked(false);
    m_audioReactiveAction = m_menu->addAction(QStringLiteral("启用音频反应"));
    m_audioReactiveAction->setCheckable(true);
    m_audioReactiveAction->setChecked(false);
    m_continuousVoiceModeAction = m_menu->addAction(QStringLiteral("连续唤醒模式"));
    m_continuousVoiceModeAction->setCheckable(true);
    m_continuousVoiceModeAction->setChecked(false);
    m_scriptedEntranceAction = m_menu->addAction(QStringLiteral("启用脚本式登场"));
    m_scriptedEntranceAction->setCheckable(true);
    m_scriptedEntranceAction->setChecked(false);
    m_fullscreenPauseAction = m_menu->addAction(QStringLiteral("全屏时暂停"));
    m_fullscreenPauseAction->setCheckable(true);
    m_fullscreenPauseAction->setChecked(false);
    m_idleInvasionAction = m_menu->addAction(QStringLiteral("启用空闲入侵"));
    m_idleInvasionAction->setCheckable(true);
    m_idleInvasionAction->setChecked(false);
    QAction *statusAction = m_menu->addAction(QStringLiteral("状态"));
    QAction *resetPositionAction = m_menu->addAction(QStringLiteral("重置位置"));
    QAction *settingsAction = m_menu->addAction(QStringLiteral("设置"));
    QAction *guideAction = m_menu->addAction(QStringLiteral("使用指南"));
    QAction *editScriptsAction = m_menu->addAction(QStringLiteral("编辑台词"));
    QAction *reloadCharactersAction = m_menu->addAction(QStringLiteral("重载角色"));
    QAction *reloadScriptsAction = m_menu->addAction(QStringLiteral("重载台词"));
    QAction *copyLogsAction = m_menu->addAction(QStringLiteral("复制最近日志"));
    QAction *checkUpdatesAction = m_menu->addAction(QStringLiteral("检查更新"));
    QAction *feedbackAction = m_menu->addAction(QStringLiteral("反馈问题"));
    QAction *openConfigAction = m_menu->addAction(QStringLiteral("打开配置文件"));
    QAction *openDataDirAction = m_menu->addAction(QStringLiteral("打开数据目录"));
    QAction *openLogsAction = m_menu->addAction(QStringLiteral("打开日志目录"));
    QAction *aboutAction = m_menu->addAction(QStringLiteral("关于"));
    m_menu->addSeparator();
    QAction *quitAction = m_menu->addAction(QStringLiteral("退出"));

    connect(showAction, &QAction::triggered, this, &TrayController::showRequested);
    connect(hideAction, &QAction::triggered, this, &TrayController::hideRequested);
    connect(toggleAction, &QAction::triggered, this, &TrayController::toggleRequested);
    connect(summonAction, &QAction::triggered, this, &TrayController::summonRequested);
    connect(scriptedTrajectoryAction, &QAction::triggered, this, &TrayController::scriptedTrajectoryRequested);
    connect(sadComfortAction, &QAction::triggered, this, &TrayController::sadComfortDebugRequested);
    connect(noFaceAction, &QAction::triggered, this, &TrayController::noFaceDebugRequested);
    connect(peekAction, &QAction::triggered, this, &TrayController::peekRequested);
    connect(fleeAction, &QAction::triggered, this, &TrayController::fleeRequested);
    connect(demoTrajectoryAction, &QAction::triggered, this, &TrayController::demoTrajectoryRequested);
    connect(commentaryAction, &QAction::triggered, this, &TrayController::commentaryRequested);
    connect(cameraScanDebugAction, &QAction::triggered, this, &TrayController::cameraScanDebugRequested);
    connect(invasionDebugAction, &QAction::triggered, this, &TrayController::invasionDebugRequested);
    connect(m_dndAction, &QAction::toggled, this, [this](bool enabled) {
        m_dndEnabled = enabled;
        refreshToolTip();
        Q_EMIT dndToggled(m_dndEnabled);
    });
    connect(m_offlineAction, &QAction::toggled, this, [this](bool enabled) {
        m_offlineEnabled = enabled;
        Q_EMIT offlineModeToggled(m_offlineEnabled);
    });
    connect(m_residentAction, &QAction::toggled, this, [this](bool enabled) {
        m_residentEnabled = enabled;
        Q_EMIT residentModeToggled(m_residentEnabled);
    });
    connect(m_autoCommentaryAction, &QAction::toggled, this, [this](bool enabled) {
        m_autoCommentaryEnabled = enabled;
        Q_EMIT autoCommentaryToggled(m_autoCommentaryEnabled);
    });
    connect(m_cameraAction, &QAction::toggled, this, [this](bool enabled) {
        m_cameraEnabled = enabled;
        Q_EMIT cameraToggled(m_cameraEnabled);
    });
    connect(m_microphoneAction, &QAction::toggled, this, [this](bool enabled) {
        m_microphoneEnabled = enabled;
        Q_EMIT microphoneToggled(m_microphoneEnabled);
    });
    connect(m_wakeupAction, &QAction::toggled, this, [this](bool enabled) {
        m_wakeupEnabled = enabled;
        Q_EMIT wakeupToggled(m_wakeupEnabled);
    });
    connect(m_eyeTrackingAction, &QAction::toggled, this, [this](bool enabled) {
        m_eyeTrackingEnabled = enabled;
        Q_EMIT eyeTrackingToggled(m_eyeTrackingEnabled);
    });
    connect(m_periodicScanAction, &QAction::toggled, this, [this](bool enabled) {
        m_periodicScanEnabled = enabled;
        Q_EMIT periodicScanToggled(m_periodicScanEnabled);
    });
    connect(m_audioReactiveAction, &QAction::toggled, this, [this](bool enabled) {
        m_audioReactiveEnabled = enabled;
        Q_EMIT audioReactiveToggled(m_audioReactiveEnabled);
    });
    connect(m_continuousVoiceModeAction, &QAction::toggled, this, [this](bool enabled) {
        m_continuousVoiceModeEnabled = enabled;
        Q_EMIT continuousVoiceModeToggled(m_continuousVoiceModeEnabled);
    });
    connect(m_scriptedEntranceAction, &QAction::toggled, this, [this](bool enabled) {
        m_scriptedEntranceEnabled = enabled;
        Q_EMIT scriptedEntranceToggled(m_scriptedEntranceEnabled);
    });
    connect(m_fullscreenPauseAction, &QAction::toggled, this, [this](bool enabled) {
        m_fullscreenPauseEnabled = enabled;
        Q_EMIT fullscreenPauseToggled(m_fullscreenPauseEnabled);
    });
    connect(m_idleInvasionAction, &QAction::toggled, this, [this](bool enabled) {
        m_idleInvasionEnabled = enabled;
        Q_EMIT idleInvasionToggled(m_idleInvasionEnabled);
    });
    connect(statusAction, &QAction::triggered, this, &TrayController::statusRequested);
    connect(resetPositionAction, &QAction::triggered, this, &TrayController::resetPositionRequested);
    connect(settingsAction, &QAction::triggered, this, &TrayController::settingsRequested);
    connect(guideAction, &QAction::triggered, this, &TrayController::guideRequested);
    connect(editScriptsAction, &QAction::triggered, this, &TrayController::editScriptsRequested);
    connect(reloadCharactersAction, &QAction::triggered, this, &TrayController::reloadCharactersRequested);
    connect(reloadScriptsAction, &QAction::triggered, this, &TrayController::reloadScriptsRequested);
    connect(copyLogsAction, &QAction::triggered, this, &TrayController::copyRecentLogsRequested);
    connect(checkUpdatesAction, &QAction::triggered, this, &TrayController::checkUpdatesRequested);
    connect(feedbackAction, &QAction::triggered, this, &TrayController::feedbackRequested);
    connect(openConfigAction, &QAction::triggered, this, &TrayController::openConfigRequested);
    connect(openDataDirAction, &QAction::triggered, this, &TrayController::openDataDirRequested);
    connect(openLogsAction, &QAction::triggered, this, &TrayController::openLogsRequested);
    connect(aboutAction, &QAction::triggered, this, &TrayController::aboutRequested);
    connect(quitAction, &QAction::triggered, this, &TrayController::quitRequested);

    connect(m_trayIcon.get(), &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            Q_EMIT toggleRequested();
        }
    });

    m_trayIcon->setContextMenu(m_menu.get());
}

TrayController::~TrayController() = default;

void TrayController::updateCharacters(const QVector<CharacterManifest> &manifests, const QString &activeCharacterId)
{
    if (!m_characterMenu) {
        return;
    }

    m_characterMenu->clear();
    for (const CharacterManifest &manifest : manifests) {
        if (!manifest.isValid()) {
            continue;
        }
        QAction *action = m_characterMenu->addAction(manifest.name.isEmpty() ? manifest.id : manifest.name);
        action->setCheckable(true);
        action->setChecked(manifest.id.compare(activeCharacterId.trimmed(), Qt::CaseInsensitive) == 0);
        connect(action, &QAction::triggered, this, [this, characterId = manifest.id]() {
            Q_EMIT characterSwitchRequested(characterId);
        });
    }

    if (m_characterMenu->actions().isEmpty()) {
        QAction *fallback = m_characterMenu->addAction(QStringLiteral("无可用角色"));
        fallback->setEnabled(false);
    }
}

void TrayController::show()
{
    m_trayIcon->show();
}

void TrayController::hide()
{
    m_trayIcon->hide();
}

void TrayController::setDndChecked(bool enabled)
{
    const bool target = enabled;
    m_dndEnabled = target;
    refreshToolTip();
    if (!m_dndAction) {
        return;
    }
    if (m_dndAction->isChecked() != target) {
        const QSignalBlocker blocker(m_dndAction);
        m_dndAction->setChecked(target);
    }
}

bool TrayController::isDndChecked() const
{
    return m_dndEnabled;
}

void TrayController::refreshToolTip()
{
    if (!m_trayIcon) {
        return;
    }
    m_trayIcon->setToolTip(
        m_dndEnabled
            ? QStringLiteral("CyberCompanionCpp (请勿打扰)")
            : QStringLiteral("CyberCompanionCpp"));
}

void TrayController::setOfflineChecked(bool enabled)
{
    const bool target = enabled;
    m_offlineEnabled = target;
    if (!m_offlineAction) {
        return;
    }
    if (m_offlineAction->isChecked() != target) {
        const QSignalBlocker blocker(m_offlineAction);
        m_offlineAction->setChecked(target);
    }
}

bool TrayController::isOfflineChecked() const
{
    return m_offlineEnabled;
}

void TrayController::setResidentChecked(bool enabled)
{
    const bool target = enabled;
    m_residentEnabled = target;
    if (!m_residentAction) {
        return;
    }
    if (m_residentAction->isChecked() != target) {
        const QSignalBlocker blocker(m_residentAction);
        m_residentAction->setChecked(target);
    }
}

void TrayController::setAutoCommentaryChecked(bool enabled)
{
    const bool target = enabled;
    m_autoCommentaryEnabled = target;
    if (!m_autoCommentaryAction) {
        return;
    }
    if (m_autoCommentaryAction->isChecked() != target) {
        const QSignalBlocker blocker(m_autoCommentaryAction);
        m_autoCommentaryAction->setChecked(target);
    }
}

bool TrayController::isResidentChecked() const
{
    return m_residentEnabled;
}

bool TrayController::isAutoCommentaryChecked() const
{
    return m_autoCommentaryEnabled;
}

void TrayController::setCameraChecked(bool enabled)
{
    const bool target = enabled;
    m_cameraEnabled = target;
    if (!m_cameraAction) {
        return;
    }
    if (m_cameraAction->isChecked() != target) {
        const QSignalBlocker blocker(m_cameraAction);
        m_cameraAction->setChecked(target);
    }
}

void TrayController::setMicrophoneChecked(bool enabled)
{
    const bool target = enabled;
    m_microphoneEnabled = target;
    if (!m_microphoneAction) {
        return;
    }
    if (m_microphoneAction->isChecked() != target) {
        const QSignalBlocker blocker(m_microphoneAction);
        m_microphoneAction->setChecked(target);
    }
}

bool TrayController::isCameraChecked() const
{
    return m_cameraEnabled;
}

bool TrayController::isMicrophoneChecked() const
{
    return m_microphoneEnabled;
}

void TrayController::setWakeupChecked(bool enabled)
{
    const bool target = enabled;
    m_wakeupEnabled = target;
    if (!m_wakeupAction) {
        return;
    }
    if (m_wakeupAction->isChecked() != target) {
        const QSignalBlocker blocker(m_wakeupAction);
        m_wakeupAction->setChecked(target);
    }
}

void TrayController::setEyeTrackingChecked(bool enabled)
{
    const bool target = enabled;
    m_eyeTrackingEnabled = target;
    if (!m_eyeTrackingAction) {
        return;
    }
    if (m_eyeTrackingAction->isChecked() != target) {
        const QSignalBlocker blocker(m_eyeTrackingAction);
        m_eyeTrackingAction->setChecked(target);
    }
}

bool TrayController::isWakeupChecked() const
{
    return m_wakeupEnabled;
}

bool TrayController::isEyeTrackingChecked() const
{
    return m_eyeTrackingEnabled;
}

void TrayController::setPeriodicScanChecked(bool enabled)
{
    const bool target = enabled;
    m_periodicScanEnabled = target;
    if (!m_periodicScanAction) {
        return;
    }
    if (m_periodicScanAction->isChecked() != target) {
        const QSignalBlocker blocker(m_periodicScanAction);
        m_periodicScanAction->setChecked(target);
    }
}

void TrayController::setAudioReactiveChecked(bool enabled)
{
    const bool target = enabled;
    m_audioReactiveEnabled = target;
    if (!m_audioReactiveAction) {
        return;
    }
    if (m_audioReactiveAction->isChecked() != target) {
        const QSignalBlocker blocker(m_audioReactiveAction);
        m_audioReactiveAction->setChecked(target);
    }
}

bool TrayController::isPeriodicScanChecked() const
{
    return m_periodicScanEnabled;
}

bool TrayController::isAudioReactiveChecked() const
{
    return m_audioReactiveEnabled;
}

void TrayController::setContinuousVoiceModeChecked(bool enabled)
{
    const bool target = enabled;
    m_continuousVoiceModeEnabled = target;
    if (!m_continuousVoiceModeAction) {
        return;
    }
    if (m_continuousVoiceModeAction->isChecked() != target) {
        const QSignalBlocker blocker(m_continuousVoiceModeAction);
        m_continuousVoiceModeAction->setChecked(target);
    }
}

bool TrayController::isContinuousVoiceModeChecked() const
{
    return m_continuousVoiceModeEnabled;
}

void TrayController::setScriptedEntranceChecked(bool enabled)
{
    const bool target = enabled;
    m_scriptedEntranceEnabled = target;
    if (!m_scriptedEntranceAction) {
        return;
    }
    if (m_scriptedEntranceAction->isChecked() != target) {
        const QSignalBlocker blocker(m_scriptedEntranceAction);
        m_scriptedEntranceAction->setChecked(target);
    }
}

void TrayController::setFullscreenPauseChecked(bool enabled)
{
    const bool target = enabled;
    m_fullscreenPauseEnabled = target;
    if (!m_fullscreenPauseAction) {
        return;
    }
    if (m_fullscreenPauseAction->isChecked() != target) {
        const QSignalBlocker blocker(m_fullscreenPauseAction);
        m_fullscreenPauseAction->setChecked(target);
    }
}

bool TrayController::isScriptedEntranceChecked() const
{
    return m_scriptedEntranceEnabled;
}

bool TrayController::isFullscreenPauseChecked() const
{
    return m_fullscreenPauseEnabled;
}

void TrayController::setIdleInvasionChecked(bool enabled)
{
    const bool target = enabled;
    m_idleInvasionEnabled = target;
    if (!m_idleInvasionAction) {
        return;
    }
    if (m_idleInvasionAction->isChecked() != target) {
        const QSignalBlocker blocker(m_idleInvasionAction);
        m_idleInvasionAction->setChecked(target);
    }
}

bool TrayController::isIdleInvasionChecked() const
{
    return m_idleInvasionEnabled;
}

void TrayController::showStartupMessage(const QString &title, const QString &message)
{
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 5000);
}

void TrayController::popupAt(const QPoint &globalPos)
{
    if (!m_menu) {
        return;
    }
    m_menu->popup(globalPos);
}
