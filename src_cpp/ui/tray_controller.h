#pragma once

#include <memory>

#include <QObject>

#include "runtime/character_manifest_catalog.h"

class QMenu;
class QSystemTrayIcon;
class QIcon;
class QPoint;
class QAction;

class TrayController : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(const QIcon &icon, QObject *parent = nullptr);
    ~TrayController() override;

    void show();
    void hide();
    void setDndChecked(bool enabled);
    void setOfflineChecked(bool enabled);
    void setResidentChecked(bool enabled);
    void setAutoCommentaryChecked(bool enabled);
    void setCameraChecked(bool enabled);
    void setMicrophoneChecked(bool enabled);
    void setWakeupChecked(bool enabled);
    void setEyeTrackingChecked(bool enabled);
    void setPeriodicScanChecked(bool enabled);
    void setAudioReactiveChecked(bool enabled);
    void setContinuousVoiceModeChecked(bool enabled);
    void setScriptedEntranceChecked(bool enabled);
    void setFullscreenPauseChecked(bool enabled);
    void setIdleInvasionChecked(bool enabled);
    bool isDndChecked() const;
    bool isOfflineChecked() const;
    bool isResidentChecked() const;
    bool isAutoCommentaryChecked() const;
    bool isCameraChecked() const;
    bool isMicrophoneChecked() const;
    bool isWakeupChecked() const;
    bool isEyeTrackingChecked() const;
    bool isPeriodicScanChecked() const;
    bool isAudioReactiveChecked() const;
    bool isContinuousVoiceModeChecked() const;
    bool isScriptedEntranceChecked() const;
    bool isFullscreenPauseChecked() const;
    bool isIdleInvasionChecked() const;
    void updateCharacters(const QVector<CharacterManifest> &manifests, const QString &activeCharacterId = QString());
    void showStartupMessage(const QString &title, const QString &message);
    void popupAt(const QPoint &globalPos);

Q_SIGNALS:
    void toggleRequested();
    void showRequested();
    void hideRequested();
    void summonRequested();
    void scriptedTrajectoryRequested();
    void peekRequested();
    void fleeRequested();
    void demoTrajectoryRequested();
    void commentaryRequested();
    void cameraScanDebugRequested();
    void invasionDebugRequested();
    void sadComfortDebugRequested();
    void noFaceDebugRequested();
    void dndToggled(bool enabled);
    void offlineModeToggled(bool enabled);
    void residentModeToggled(bool enabled);
    void autoCommentaryToggled(bool enabled);
    void cameraToggled(bool enabled);
    void microphoneToggled(bool enabled);
    void wakeupToggled(bool enabled);
    void eyeTrackingToggled(bool enabled);
    void periodicScanToggled(bool enabled);
    void audioReactiveToggled(bool enabled);
    void continuousVoiceModeToggled(bool enabled);
    void scriptedEntranceToggled(bool enabled);
    void fullscreenPauseToggled(bool enabled);
    void idleInvasionToggled(bool enabled);
    void statusRequested();
    void resetPositionRequested();
    void settingsRequested();
    void guideRequested();
    void editScriptsRequested();
    void reloadCharactersRequested();
    void reloadScriptsRequested();
    void copyRecentLogsRequested();
    void checkUpdatesRequested();
    void feedbackRequested();
    void characterSwitchRequested(const QString &characterId);
    void openConfigRequested();
    void openDataDirRequested();
    void openLogsRequested();
    void aboutRequested();
    void quitRequested();

private:
    void refreshToolTip();

    std::unique_ptr<QSystemTrayIcon> m_trayIcon;
    std::unique_ptr<QMenu> m_menu;
    QMenu *m_characterMenu = nullptr;
    QAction *m_dndAction = nullptr;
    QAction *m_offlineAction = nullptr;
    QAction *m_residentAction = nullptr;
    QAction *m_autoCommentaryAction = nullptr;
    QAction *m_cameraAction = nullptr;
    QAction *m_microphoneAction = nullptr;
    QAction *m_wakeupAction = nullptr;
    QAction *m_eyeTrackingAction = nullptr;
    QAction *m_periodicScanAction = nullptr;
    QAction *m_audioReactiveAction = nullptr;
    QAction *m_continuousVoiceModeAction = nullptr;
    QAction *m_scriptedEntranceAction = nullptr;
    QAction *m_fullscreenPauseAction = nullptr;
    QAction *m_idleInvasionAction = nullptr;
    bool m_dndEnabled = false;
    bool m_offlineEnabled = false;
    bool m_residentEnabled = false;
    bool m_autoCommentaryEnabled = false;
    bool m_cameraEnabled = false;
    bool m_microphoneEnabled = false;
    bool m_wakeupEnabled = false;
    bool m_eyeTrackingEnabled = false;
    bool m_periodicScanEnabled = false;
    bool m_audioReactiveEnabled = false;
    bool m_continuousVoiceModeEnabled = false;
    bool m_scriptedEntranceEnabled = false;
    bool m_fullscreenPauseEnabled = false;
    bool m_idleInvasionEnabled = false;
};
