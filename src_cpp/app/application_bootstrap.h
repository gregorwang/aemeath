#pragma once

#include <memory>

#include <QObject>
#include <QSet>

#include "runtime/character_manifest_catalog.h"
#include "runtime/config_repository.h"

class QApplication;
class EntityWidget;
class RuntimeDirector;
class IdleInvasionController;
class QTimer;
class QNetworkAccessManager;
class SettingsDialog;
class TrayController;
class TrajectoryPlayer;
class SingleInstanceGuard;
class AudioService;
class AudioOutputMonitorService;
class FullscreenMonitorService;
class HotkeyService;
class IdleMonitorService;
class ScreenCommentaryService;
class VisionService;
class VoiceInputService;
class AutoStartManager;

class ApplicationBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit ApplicationBootstrap(QApplication &app);
    ~ApplicationBootstrap() override;

    bool initialize();

private Q_SLOTS:
    void toggleEntityVisibility();
    void showEntity();
    void hideEntity();
    void resetEntityPosition();
    void setDndModeEnabled(bool enabled);
    void setOfflineModeEnabled(bool enabled);
    void setResidentModeEnabled(bool enabled);
    void setAutoCommentaryEnabled(bool enabled);
    void setCameraEnabled(bool enabled);
    void setMicrophoneEnabled(bool enabled);
    void setWakeupEnabled(bool enabled);
    void setEyeTrackingEnabled(bool enabled);
    void setPeriodicScanEnabled(bool enabled);
    void setAudioReactiveEnabled(bool enabled);
    void setContinuousVoiceModeEnabled(bool enabled);
    void setScriptedEntranceEnabled(bool enabled);
    void setFullscreenPauseEnabled(bool enabled);
    void setIdleInvasionEnabled(bool enabled);
    void showSettingsDialog();
    void openQuickStartGuide();
    void copyRecentLogs();
    void checkForUpdates();
    void openFeedbackIssue();
    void openConfigFile();
    void openDataDirectory();
    void openLogsDirectory();
    void showAboutDialog();
    void handleQuitRequested();

private:
    bool isAutoStartLaunch() const;
    bool isForcedStartMinimizedLaunch() const;
    void setupApplicationMetadata() const;
    void restoreRuntimeState();
    void persistRuntimeState();
    bool saveConfigWithNotification(const QString &context);
    void showRuntimeErrorNotification(const QString &feature, const QString &message);
    void showRuntimeErrorNotificationOnce(
        const QString &dedupeKey,
        const QString &feature,
        const QString &message);
    void syncAutoStartSetting();
    void wireSignals();
    void stopScriptedTrajectoryWatchdog();
    void rebuildAudioOutputMonitorService();
    void rebuildScreenCommentaryService();
    void rebuildVisionService();
    void rebuildVoiceInputService();
    bool visionRuntimeRequested() const;
    void startVisionServiceIfNeeded(bool force = false);
    int nextIdleThresholdMs() const;
    void rearmIdleMonitorThreshold();
    void requestCameraDebugScan();
    void showEntityContextMenu(const QPoint &globalPos);
    void handleVoiceTranscript(const QString &text, const QString &source);
    void maybeWarnLegacyAsrProviderMigration();
    void maybeWarnLegacyTtsProviderMigration();
    QString buildStatusSummary() const;
    QString buildStartupSummary() const;
    QString activeScriptsPath() const;
    void showStatusSummary();
    void editScriptsFile();
    void reloadCharacters();
    void reloadScripts();
    void switchCharacter(const QString &characterId);
    bool applyCharacterManifest(const CharacterManifest &manifest, bool persistConfig, bool notifyUser);

    QApplication &m_app;
    AppConfig m_config;
    std::unique_ptr<ConfigRepository> m_configRepository;
    std::unique_ptr<CharacterManifestCatalog> m_characterManifestCatalog;
    std::unique_ptr<AutoStartManager> m_autoStartManager;
    std::unique_ptr<SingleInstanceGuard> m_singleInstanceGuard;
    std::unique_ptr<EntityWidget> m_entityWidget;
    std::unique_ptr<RuntimeDirector> m_runtimeDirector;
    std::unique_ptr<IdleInvasionController> m_idleInvasionController;
    std::unique_ptr<TrajectoryPlayer> m_trajectoryPlayer;
    std::unique_ptr<QNetworkAccessManager> m_updateNetworkAccessManager;
    std::unique_ptr<AudioService> m_audioService;
    std::unique_ptr<AudioOutputMonitorService> m_audioOutputMonitorService;
    std::unique_ptr<FullscreenMonitorService> m_fullscreenMonitorService;
    std::unique_ptr<HotkeyService> m_hotkeyService;
    std::unique_ptr<IdleMonitorService> m_idleMonitorService;
    std::unique_ptr<ScreenCommentaryService> m_screenCommentaryService;
    std::unique_ptr<VisionService> m_visionService;
    std::unique_ptr<VoiceInputService> m_voiceInputService;
    std::unique_ptr<TrayController> m_trayController;
    QTimer *m_scriptedTrajectoryWatchdog = nullptr;
    QSet<QString> m_notifiedRuntimeErrorKeys;
    CharacterManifest m_activeCharacterManifest;
    bool m_pendingCameraDebugScan = false;
    bool m_updateCheckInFlight = false;
    bool m_fullscreenActive = false;
    bool m_presenceTrackingVisible = false;
    bool m_initialized = false;
};
