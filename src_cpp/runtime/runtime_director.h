#pragma once

#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>

#include "runtime/mood_system.h"
#include "runtime/presence_detector.h"
#include "services/service_contracts.h"
#include "runtime/voice_script_catalog.h"

class QTimer;

class RuntimeDirector : public QObject
{
    Q_OBJECT

public:
    enum class EntityState {
        Hidden,
        Peeking,
        Engaged,
        Fleeing
    };
    Q_ENUM(EntityState)

    enum class BehaviorMode {
        Idle,
        Busy,
        MediaPlaying,
        Summoning,
        Commentary
    };
    Q_ENUM(BehaviorMode)

    explicit RuntimeDirector(QObject *parent = nullptr);

    void start();
    EntityState currentState() const;
    BehaviorMode currentBehaviorMode() const;
    bool commentaryInFlight() const;
    void setScriptedEntranceEnabled(bool enabled);
    void setScriptedTrajectoryPath(const QString &path);
    void setVoiceScriptsPath(const QString &path);
    void setAudioOutputReactive(bool enabled);
    void setAutoDismissSeconds(int seconds);
    void setIdleInvasionEnabled(bool enabled);
    void setIdleInvasionStartDelayMs(int startDelayMs);
    void setOfflineMode(bool enabled);
    void setScreenCommentaryAutoEnabled(bool enabled);
    void setScreenCommentaryAutoIntervalMinutes(int minutes);
    void setScreenCommentarySpeechOptions(
        bool streamingEnabled,
        int streamChunkChars,
        int maxResponseChars,
        const QString &preambleText);
    void setCameraEnabled(bool enabled);
    void setPresenceTargetFps(int targetFps);
    void setEyeTrackingEnabled(bool enabled);
    void setPeriodicScanEnabled(bool enabled);
    void setPeriodicScanIntervalMinutes(int minutes);
    void setFullScreenPauseEnabled(bool enabled);
    void setResidentModeEnabled(bool enabled);
    void setDndMode(bool enabled);
    void setVisionFeedbackTimingsForTest(
        int noFaceMinAbsenceMs,
        int userReturnMinSpeakMs,
        int noFaceCooldownMs,
        int userReturnMediumThresholdMs,
        int userReturnLongThresholdMs);
    bool isDndMode() const;
    bool isResidentModeEnabled() const;
    bool isAudioOutputActive() const;
    int autoDismissMs() const;
    PresenceState currentPresenceState() const;
    double currentMoodValue() const;
    QString currentMoodLabel() const;
    bool triggerScriptedTrajectoryDebug();
    bool triggerSadComfortDebug();
    bool triggerNoFaceTestDebug();
    bool triggerPeriodicCameraCheckDebug();

public Q_SLOTS:
    void summonNow();
    void triggerPeek();
    void triggerFlee();
    void hideNow();
    void playDemoTrajectory();
    void onScriptedTrajectoryFinished();
    void onUserIdleDetected();
    void onIdleTimeUpdated(qint64 idleMs);
    void onUserActivityDetected();
    void requestScreenCommentary();
    void onCommentaryReady(const QString &text);
    void onCommentaryFailed(const QString &error);
    void onAudioOutputStarted();
    void onAudioOutputStopped();
    void onSelfPlaybackStarted(const QString &text);
    void onSelfPlaybackFinished();
    void onFullscreenStateChanged(bool fullscreenActive);
    void onCameraStateChanged(bool running);
    void onGazeUpdated(const GazeSample &sample);
    void onCameraError(const QString &message);

Q_SIGNALS:
    void stateChanged(RuntimeDirector::EntityState state);
    void behaviorModeChanged(RuntimeDirector::BehaviorMode mode);
    void statusTextChanged(const QString &title, const QString &detail);
    void visionRuntimeRequested(bool debugMode);
    void trajectoryRequested(const QVector<QPoint> &points, int durationMs);
    void scriptedTrajectoryRequested(const QString &filePath);
    void speechRequestRequested(const AudioPlaybackRequest &request);
    void voiceVisualRequested(const QString &spritePath, const QString &animSpeed);
    void entityStateOverrideRequested(const QString &stateName);
    void screenCommentaryRequested();
    void gazeFollowRequested(double faceX, double faceY, bool faceDetected, double confidence);
    void periodicCameraScanCompleted(bool debugMode);
    void prolongedIdlePulseRequested();

private Q_SLOTS:
    void onIdlePeekTimeout();
    void onPeekToEngagedTimeout();
    void onAutoDismissTimeout();
    void onFleeHideTimeout();
    void onCommentaryStartTimeout();
    void onAutoScreenCommentaryTimeout();
    void onPeriodicCameraScanTimeout();
    void onPeriodicCameraScanCollectTimeout();
    void onPassivePresenceTimeout();
    void onProlongedIdleTimeout();

private:
    void stopTimers();
    void armAutoDismiss(int durationMs);
    void completeCommentarySession();
    void syncAutoScreenCommentaryTimer();
    void syncPeriodicScanTimer();
    void syncProlongedIdleTimer();
    void beginPeriodicCameraScan(const QString &source, bool debugMode);
    bool startPeriodicCameraScan(const QString &source, bool debugMode);
    void applyPeriodicScanVisual(bool facePresent, const QString &emotionLabel);
    void refreshPresenceState(const GazeSample *sample);
    void trackExpressionState(const GazeSample &sample);
    void resetExpressionTracking();
    void maybeTriggerSadComfort(const GazeSample &sample);
    void emitSadComfortSpeech();
    QStringList splitCommentarySpeech(const QString &text) const;
    void resetNoFaceTracker();
    void maybeTriggerNoFaceReturn(const GazeSample &sample);
    void maybeGreetUserReturn();
    void emitUserReturnSpeech(qint64 awayMs);
    void applyPresenceState(PresenceState state);
    void enterPassiveCompanion();
    void enterDeepSleep();
    void onPresenceBackActive();
    QString resolveScriptedTrajectoryPath() const;
    void emitIdleSpeechRequest();
    void emitPanicSpeechRequest();
    void syncResidentVisibility();
    bool residentActive() const;
    void setBehaviorMode(BehaviorMode mode);
    void transitionTo(EntityState state, const QString &title, const QString &detail);
    int nextIdleIntervalMs() const;

    EntityState m_state = EntityState::Hidden;
    BehaviorMode m_behaviorMode = BehaviorMode::Busy;
    QTimer *m_idlePeekTimer = nullptr;
    QTimer *m_peekAdvanceTimer = nullptr;
    QTimer *m_autoDismissTimer = nullptr;
    QTimer *m_fleeHideTimer = nullptr;
    QTimer *m_commentaryStartTimer = nullptr;
    QTimer *m_autoScreenCommentaryTimer = nullptr;
    QTimer *m_periodicScanTimer = nullptr;
    QTimer *m_periodicScanCollectTimer = nullptr;
    QTimer *m_passivePresenceTimer = nullptr;
    QTimer *m_moodDecayTimer = nullptr;
    QTimer *m_prolongedIdleTimer = nullptr;
    VoiceScriptCatalog m_voiceScriptCatalog;
    MoodSystem m_moodSystem;
    PresenceDetector m_presenceDetector;
    GazeSample m_latestGazeSample;
    QVector<GazeSample> m_periodicScanSamples;
    QString m_scriptedTrajectoryPath;
    bool m_scriptedEntranceEnabled = false;
    bool m_audioOutputReactive = true;
    int m_autoDismissMs = 10000;
    bool m_idleInvasionEnabled = false;
    int m_idleInvasionStartDelayMs = 300000;
    bool m_offlineMode = false;
    bool m_screenCommentaryAutoEnabled = false;
    int m_screenCommentaryAutoIntervalMs = 60 * 60 * 1000;
    bool m_commentaryStreamingEnabled = true;
    int m_commentaryStreamChunkChars = 22;
    int m_commentaryMaxResponseChars = 90;
    QString m_commentaryPreambleText = QStringLiteral("正在看你的屏幕内容，让我看看你在做什么。");
    bool m_cameraEnabled = false;
    bool m_eyeTrackingEnabled = true;
    bool m_periodicScanEnabled = true;
    int m_periodicScanIntervalMinutes = 30;
    bool m_periodicScanActive = false;
    bool m_periodicScanDebugMode = false;
    bool m_periodicScanPendingStart = false;
    bool m_periodicScanPendingDebugMode = false;
    bool m_cameraRunning = false;
    bool m_passivePresenceActive = false;
    bool m_deepSleepActive = false;
    bool m_fullScreenPauseEnabled = true;
    bool m_residentModeEnabled = false;
    bool m_dndMode = false;
    bool m_fullscreenActive = false;
    bool m_audioOutputActive = false;
    bool m_selfPlaybackActive = false;
    bool m_scriptedEntranceActive = false;
    bool m_commentaryPending = false;
    bool m_commentaryActive = false;
    QHash<QString, int> m_expressionVotes;
    QString m_stableExpression = QStringLiteral("neutral");
    qint64 m_lastExpressionVisualAtMs = 0;
    qint64 m_lastSadComfortAtMs = 0;
    qint64 m_noFaceAbsentSinceMs = -1;
    qint64 m_userLeftAtMs = -1;
    qint64 m_lastNoFaceGreetingAtMs = 0;
    qint64 m_latestIdleTimeMs = 0;
    int m_noFaceMinAbsenceMs = 3000;
    int m_userReturnMinSpeakMs = 30000;
    int m_userReturnMediumThresholdMs = 5 * 60 * 1000;
    int m_userReturnLongThresholdMs = 30 * 60 * 1000;
    int m_noFaceCooldownMs = 20000;
    bool m_noFaceStreakTriggered = false;
    PresenceState m_presenceState = PresenceState::Unknown;
};

Q_DECLARE_METATYPE(RuntimeDirector::EntityState)
Q_DECLARE_METATYPE(RuntimeDirector::BehaviorMode)
