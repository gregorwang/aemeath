#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>

#include "runtime/runtime_director.h"

class RuntimeDirectorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void summonNowEmitsEngagedState();
    void triggerFleeTransitionsBackToHidden();
    void summonNowUsesScriptedTrajectoryWhenAvailable();
    void summonNowUsesConfiguredTrajectoryDirectoryWhenEnvironmentUnset();
    void scriptedTrajectorySetsSummoningBehaviorMode();
    void summonNowEmitsIdleSpeechRequest();
    void summonNowPropagatesIdleScriptAudioPath();
    void summonNowEmitsIdleScriptVisualMetadata();
    void autoDismissTimeoutHidesWithoutFleeing();
    void configurableAutoDismissDurationIsApplied();
    void audioOutputStartedSetsMediaPlayingWhenEngaged();
    void audioOutputStartedSetsMediaPlayingWhenHidden();
    void audioOutputStartedIsIgnoredDuringCommentary();
    void audioOutputStartedIsIgnoredDuringSummoning();
    void selfPlaybackStartSuppressesMediaPlayingBehavior();
    void selfPlaybackFinishRestoresMediaPlayingWhenExternalAudioStillActive();
    void disabledAudioOutputReactiveIgnoresMediaSignals();
    void disablingAudioOutputReactiveDropsMediaPlayingMode();
    void idleInvasionEnabledSkipsLegacyIdlePeek();
    void idleInvasionEnabledStillSkipsLegacyIdleBeforeInvasionStartDelay();
    void legacyIdleTriggerSummonsDirectlyWhenEnabled();
    void dndModeBlocksAutoIdlePeek();
    void fullscreenPauseBlocksAutoIdlePeek();
    void residentModeSummonsWhenStartedHidden();
    void residentModeHidesWhenFullscreenActive();
    void userActivityKeepsResidentVisibleWhenResidentMode();
    void audioOutputStoppedRestoresIdleWhenMediaPlaying();
    void audioOutputStoppedRestoresBusyWhenHidden();
    void audioOutputStoppedDoesNotBreakSummoning();
    void triggerScriptedTrajectoryDebugStartsPlaybackWhenEngaged();
    void triggerScriptedTrajectoryDebugReturnsFalseWhenFleeing();
    void triggerSadComfortDebugEmitsSpeechRequest();
    void triggerNoFaceDebugEmitsCriticalSpeechRequest();
    void triggerFleeEmitsCriticalPanicSpeechRequest();
    void scriptedTrajectoryCanBeCanceledWithoutLateEngagedState();
    void requestScreenCommentaryFromHiddenUsesScriptedTrajectoryWhenAvailable();
    void scriptedTrajectoryCompletionIsIgnoredAfterCommentaryTakesOver();
    void requestScreenCommentaryFromHiddenStagesSummon();
    void duplicateCommentaryRequestsAreIgnored();
    void canceledCommentaryResultIsIgnored();
    void commentaryFailureUsesHighPrioritySpeechRequest();
    void commentaryStartEmitsPreambleSpeechWhenConfigured();
    void commentaryReadySplitsSpeechWhenStreamingEnabled();
    void requestScreenCommentarySkipsWhenOfflineModeEnabled();
    void autoScreenCommentaryTimeoutRequestsCommentaryWhenEnabled();
    void autoScreenCommentaryTimeoutSkipsWhenDndEnabled();
    void autoScreenCommentaryTimeoutSkipsWhenOfflineModeEnabled();
    void periodicCameraDebugReturnsFalseWhenCameraUnavailable();
    void periodicCameraTimerRequestsVisionRuntimeWhenCameraStopped();
    void deferredPeriodicCameraScanStartsAfterCameraBecomesReady();
    void periodicCameraDebugCollectsSamplesAndPromotesPresence();
    void cameraStateChangeEmitsStatusWhenCameraEnabled();
    void passivePresenceSummonsSilentCompanionWhenHidden();
    void passivePresenceTimeoutHidesWhenNotResident();
    void absentPresenceEntersDeepSleep();
    void activePresenceCancelsPassiveCompanionTimeout();
    void prolongedIdlePulseEmitsWhenHidden();
    void prolongedIdlePulseIsSuppressedWhenIdleInvasionEnabled();
    void gazeUpdateEmitsGazeFollowWhenEnabled();
    void gazeUpdateDoesNotEmitGazeFollowWhenEyeTrackingDisabled();
    void stableHappyExpressionEmitsVisualOverrideWhenIdle();
    void expressionTrackingIsSuppressedOutsideIdleMode();
    void happyExpressionRaisesMoodWhenStabilized();
    void sadEmotionAutoTriggersComfortSpeech();
    void sadEmotionAutoTriggerRespectsCooldown();
    void noFaceReturnGreetsUserAfterQualifiedAbsence();
    void noFaceReturnSkipsGreetingWhenAwayTooShort();
    void noFaceReturnGreetingRespectsCooldown();
};

void RuntimeDirectorTest::summonNowEmitsEngagedState()
{
    RuntimeDirector director;
    QSignalSpy spy(&director, &RuntimeDirector::stateChanged);

    director.summonNow();

    QCOMPARE(spy.count(), 1);
    const QList<QVariant> first = spy.takeFirst();
    QCOMPARE(first.at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Engaged);
}

void RuntimeDirectorTest::triggerFleeTransitionsBackToHidden()
{
    RuntimeDirector director;
    QSignalSpy spy(&director, &RuntimeDirector::stateChanged);

    director.summonNow();
    director.triggerFlee();
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 3, 1500);

    QCOMPARE(spy.at(1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Fleeing);
    QCOMPARE(spy.at(spy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::summonNowUsesScriptedTrajectoryWhenAvailable()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":10,"y":10,"state":6}]})");
    file.flush();

    qputenv("CYBERCOMPANION_TRAJECTORY_PATH", file.fileName().toUtf8());

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy scriptedSpy(&director, &RuntimeDirector::scriptedTrajectoryRequested);

    director.summonNow();

    QCOMPARE(scriptedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Peeking);

    director.onScriptedTrajectoryFinished();
    QTRY_VERIFY_WITH_TIMEOUT(stateSpy.count() >= 2, 100);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Hidden);

    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");
}

void RuntimeDirectorTest::summonNowUsesConfiguredTrajectoryDirectoryWhenEnvironmentUnset()
{
    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile older(tempDir.filePath(QStringLiteral("trajectory_1770800738_qt_animation.json")));
    QVERIFY(older.open(QIODevice::WriteOnly | QIODevice::Text));
    older.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":10,"x":10,"y":10,"state":5}]})");
    older.close();

    QFile newer(tempDir.filePath(QStringLiteral("trajectory_1771029879_qt_animation.json")));
    QVERIFY(newer.open(QIODevice::WriteOnly | QIODevice::Text));
    newer.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":12,"x":12,"y":12,"state":6}]})");
    newer.close();

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    director.setScriptedTrajectoryPath(tempDir.path());

    QSignalSpy scriptedSpy(&director, &RuntimeDirector::scriptedTrajectoryRequested);
    director.summonNow();

    QCOMPARE(scriptedSpy.count(), 1);
    QCOMPARE(scriptedSpy.at(0).at(0).toString(), QFileInfo(newer).absoluteFilePath());
}

void RuntimeDirectorTest::scriptedTrajectorySetsSummoningBehaviorMode()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":10,"y":10,"state":6}]})");
    file.flush();

    qputenv("CYBERCOMPANION_TRAJECTORY_PATH", file.fileName().toUtf8());

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    QSignalSpy modeSpy(&director, &RuntimeDirector::behaviorModeChanged);

    director.summonNow();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Summoning);
    QVERIFY(modeSpy.count() >= 1);
    QCOMPARE(modeSpy.at(modeSpy.count() - 1).at(0).value<RuntimeDirector::BehaviorMode>(), RuntimeDirector::BehaviorMode::Summoning);

    director.onScriptedTrajectoryFinished();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);

    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");
}

void RuntimeDirectorTest::summonNowEmitsIdleSpeechRequest()
{
    RuntimeDirector director;
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    director.summonNow();

    QCOMPARE(speechSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QVERIFY(!request.text.trimmed().isEmpty());
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::High));
    QCOMPARE(request.interrupt, false);
}

void RuntimeDirectorTest::summonNowPropagatesIdleScriptAudioPath()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(QDir(tempDir.path()).mkpath(QStringLiteral("voice_cache")));

    QFile audioFile(tempDir.filePath(QStringLiteral("voice_cache/idle.mp3")));
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("stub");
    audioFile.close();

    QFile scriptsFile(tempDir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"({
        "idle_events": [{
            "id": "idle_cached",
            "text": "cached hello",
            "audio_cache": "voice_cache/idle.mp3",
            "priority": 1,
            "time_range": "default"
        }],
        "panic_events": []
    })");
    scriptsFile.close();

    RuntimeDirector director;
    director.setVoiceScriptsPath(scriptsFile.fileName());
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    director.summonNow();

    QCOMPARE(speechSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QCOMPARE(request.text, QStringLiteral("cached hello"));
    QCOMPARE(QFileInfo(request.audioFilePath).absoluteFilePath(), QFileInfo(audioFile).absoluteFilePath());
}

void RuntimeDirectorTest::summonNowEmitsIdleScriptVisualMetadata()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QVERIFY(QDir(tempDir.path()).mkpath(QStringLiteral("assets/sprites")));

    QFile spriteFile(tempDir.filePath(QStringLiteral("assets/sprites/idle.gif")));
    QVERIFY(spriteFile.open(QIODevice::WriteOnly));
    spriteFile.write("gif");
    spriteFile.close();

    QFile scriptsFile(tempDir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"({
        "idle_events": [{
            "id": "idle_visual",
            "text": "visual hello",
            "sprite": "assets/sprites/idle.gif",
            "anim_speed": "slow",
            "priority": 1,
            "time_range": "default"
        }],
        "panic_events": []
    })");
    scriptsFile.close();

    RuntimeDirector director;
    director.setVoiceScriptsPath(scriptsFile.fileName());
    QSignalSpy visualSpy(&director, &RuntimeDirector::voiceVisualRequested);

    director.summonNow();

    QCOMPARE(visualSpy.count(), 1);
    QCOMPARE(QFileInfo(visualSpy.at(0).at(0).toString()).absoluteFilePath(), QFileInfo(spriteFile).absoluteFilePath());
    QCOMPARE(visualSpy.at(0).at(1).toString(), QStringLiteral("slow"));
}

void RuntimeDirectorTest::autoDismissTimeoutHidesWithoutFleeing()
{
    RuntimeDirector director;
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    director.summonNow();
    stateSpy.clear();
    speechSpy.clear();

    QVERIFY(QMetaObject::invokeMethod(&director, "onAutoDismissTimeout", Qt::DirectConnection));

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Hidden);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Hidden);
    QCOMPARE(speechSpy.count(), 0);
}

void RuntimeDirectorTest::configurableAutoDismissDurationIsApplied()
{
    RuntimeDirector director;
    director.setAutoDismissSeconds(18);

    QCOMPARE(director.autoDismissMs(), 18000);
}

void RuntimeDirectorTest::audioOutputStartedSetsMediaPlayingWhenEngaged()
{
    RuntimeDirector director;
    director.summonNow();

    director.onAudioOutputStarted();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::MediaPlaying);
}

void RuntimeDirectorTest::audioOutputStartedSetsMediaPlayingWhenHidden()
{
    RuntimeDirector director;

    director.onAudioOutputStarted();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::MediaPlaying);
}

void RuntimeDirectorTest::audioOutputStartedIsIgnoredDuringCommentary()
{
    RuntimeDirector director;
    director.requestScreenCommentary();

    director.onAudioOutputStarted();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Commentary);
}

void RuntimeDirectorTest::audioOutputStartedIsIgnoredDuringSummoning()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":10,"y":10,"state":6}]})");
    file.flush();

    qputenv("CYBERCOMPANION_TRAJECTORY_PATH", file.fileName().toUtf8());

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    director.summonNow();

    director.onAudioOutputStarted();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Summoning);

    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");
}

void RuntimeDirectorTest::selfPlaybackStartSuppressesMediaPlayingBehavior()
{
    RuntimeDirector director;
    director.summonNow();
    director.onAudioOutputStarted();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::MediaPlaying);

    director.onSelfPlaybackStarted(QStringLiteral("tts"));

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);

    director.onAudioOutputStopped();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);
}

void RuntimeDirectorTest::selfPlaybackFinishRestoresMediaPlayingWhenExternalAudioStillActive()
{
    RuntimeDirector director;
    director.summonNow();
    director.onAudioOutputStarted();
    director.onSelfPlaybackStarted(QStringLiteral("tts"));

    director.onSelfPlaybackFinished();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::MediaPlaying);
}

void RuntimeDirectorTest::disabledAudioOutputReactiveIgnoresMediaSignals()
{
    RuntimeDirector director;
    director.setAudioOutputReactive(false);
    director.summonNow();

    director.onAudioOutputStarted();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);

    director.onAudioOutputStopped();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);
}

void RuntimeDirectorTest::disablingAudioOutputReactiveDropsMediaPlayingMode()
{
    RuntimeDirector director;
    director.summonNow();
    director.onAudioOutputStarted();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::MediaPlaying);

    director.setAudioOutputReactive(false);

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);
}

void RuntimeDirectorTest::idleInvasionEnabledSkipsLegacyIdlePeek()
{
    RuntimeDirector director;
    director.setIdleInvasionEnabled(true);
    director.setIdleInvasionStartDelayMs(30'000);
    director.onIdleTimeUpdated(35'000);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.start();
    stateSpy.clear();
    director.onUserIdleDetected();

    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::idleInvasionEnabledStillSkipsLegacyIdleBeforeInvasionStartDelay()
{
    RuntimeDirector director;
    director.setIdleInvasionEnabled(true);
    director.setIdleInvasionStartDelayMs(300'000);
    director.onIdleTimeUpdated(15'000);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.start();
    stateSpy.clear();
    director.onUserIdleDetected();

    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::legacyIdleTriggerSummonsDirectlyWhenEnabled()
{
    RuntimeDirector director;
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    director.start();
    stateSpy.clear();
    speechSpy.clear();
    director.onUserIdleDetected();

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);
    QCOMPARE(speechSpy.count(), 1);
}

void RuntimeDirectorTest::dndModeBlocksAutoIdlePeek()
{
    RuntimeDirector director;
    director.setDndMode(true);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.start();
    stateSpy.clear();
    director.onUserIdleDetected();

    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::fullscreenPauseBlocksAutoIdlePeek()
{
    RuntimeDirector director;
    director.setFullScreenPauseEnabled(true);
    director.onFullscreenStateChanged(true);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.start();
    stateSpy.clear();
    director.onUserIdleDetected();

    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::residentModeSummonsWhenStartedHidden()
{
    RuntimeDirector director;
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy modeSpy(&director, &RuntimeDirector::behaviorModeChanged);

    director.setResidentModeEnabled(true);
    director.start();

    QVERIFY(stateSpy.count() >= 2);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);
    QVERIFY(modeSpy.count() >= 1);
}

void RuntimeDirectorTest::residentModeHidesWhenFullscreenActive()
{
    RuntimeDirector director;
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.setResidentModeEnabled(true);
    director.start();
    stateSpy.clear();

    director.onFullscreenStateChanged(true);

    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Hidden);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::userActivityKeepsResidentVisibleWhenResidentMode()
{
    RuntimeDirector director;
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.setResidentModeEnabled(true);
    director.start();
    stateSpy.clear();

    director.onUserActivityDetected();

    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Engaged);
}

void RuntimeDirectorTest::audioOutputStoppedRestoresIdleWhenMediaPlaying()
{
    RuntimeDirector director;
    director.summonNow();
    director.onAudioOutputStarted();

    director.onAudioOutputStopped();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);
}

void RuntimeDirectorTest::audioOutputStoppedRestoresBusyWhenHidden()
{
    RuntimeDirector director;
    director.onAudioOutputStarted();

    director.onAudioOutputStopped();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);
}

void RuntimeDirectorTest::audioOutputStoppedDoesNotBreakSummoning()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":10,"y":10,"state":6}]})");
    file.flush();

    qputenv("CYBERCOMPANION_TRAJECTORY_PATH", file.fileName().toUtf8());

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    director.onAudioOutputStarted();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::MediaPlaying);

    director.summonNow();
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Summoning);

    director.onAudioOutputStopped();

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Summoning);

    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");
}

void RuntimeDirectorTest::triggerScriptedTrajectoryDebugStartsPlaybackWhenEngaged()
{
    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile trajectory(tempDir.filePath(QStringLiteral("trajectory_1771029879_qt_animation.json")));
    QVERIFY(trajectory.open(QIODevice::WriteOnly | QIODevice::Text));
    trajectory.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":12,"x":12,"y":12,"state":6}]})");
    trajectory.close();

    RuntimeDirector director;
    director.summonNow();
    director.setScriptedEntranceEnabled(true);
    director.setScriptedTrajectoryPath(tempDir.path());

    QSignalSpy scriptedSpy(&director, &RuntimeDirector::scriptedTrajectoryRequested);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    QVERIFY(director.triggerScriptedTrajectoryDebug());
    QCOMPARE(scriptedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Peeking);

    director.onScriptedTrajectoryFinished();
    QTRY_VERIFY_WITH_TIMEOUT(stateSpy.count() >= 2, 100);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::triggerScriptedTrajectoryDebugReturnsFalseWhenFleeing()
{
    RuntimeDirector director;
    director.summonNow();
    director.triggerFlee();

    QSignalSpy scriptedSpy(&director, &RuntimeDirector::scriptedTrajectoryRequested);

    QVERIFY(!director.triggerScriptedTrajectoryDebug());
    QCOMPARE(scriptedSpy.count(), 0);
}

void RuntimeDirectorTest::triggerSadComfortDebugEmitsSpeechRequest()
{
    RuntimeDirector director;
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    QVERIFY(director.triggerSadComfortDebug());

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(speechSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::High));
    QCOMPARE(request.interrupt, false);
    QVERIFY(request.text.contains(QStringLiteral("陪")));
}

void RuntimeDirectorTest::triggerNoFaceDebugEmitsCriticalSpeechRequest()
{
    RuntimeDirector director;
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    QVERIFY(director.triggerNoFaceTestDebug());

    QCOMPARE(speechSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QVERIFY(!request.text.trimmed().isEmpty());
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::Critical));
    QCOMPARE(request.interrupt, true);
}

void RuntimeDirectorTest::triggerFleeEmitsCriticalPanicSpeechRequest()
{
    RuntimeDirector director;
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    director.summonNow();
    speechSpy.clear();

    director.triggerFlee();

    QCOMPARE(speechSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QVERIFY(!request.text.trimmed().isEmpty());
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::Critical));
    QCOMPARE(request.interrupt, true);
}

void RuntimeDirectorTest::scriptedTrajectoryCanBeCanceledWithoutLateEngagedState()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":10,"y":10,"state":6}]})");
    file.flush();

    qputenv("CYBERCOMPANION_TRAJECTORY_PATH", file.fileName().toUtf8());

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.summonNow();
    director.triggerFlee();
    director.onScriptedTrajectoryFinished();

    QTRY_VERIFY_WITH_TIMEOUT(stateSpy.count() >= 3, 1500);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Peeking);
    QCOMPARE(stateSpy.at(1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Fleeing);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Hidden);

    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");
}

void RuntimeDirectorTest::requestScreenCommentaryFromHiddenUsesScriptedTrajectoryWhenAvailable()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":10,"y":10,"state":6}]})");
    file.flush();

    qputenv("CYBERCOMPANION_TRAJECTORY_PATH", file.fileName().toUtf8());

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    QSignalSpy scriptedSpy(&director, &RuntimeDirector::scriptedTrajectoryRequested);
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    director.requestScreenCommentary();

    QCOMPARE(scriptedSpy.count(), 1);
    QCOMPARE(commentarySpy.count(), 0);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Summoning);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Peeking);

    director.onScriptedTrajectoryFinished();

    QTRY_COMPARE_WITH_TIMEOUT(commentarySpy.count(), 1, 100);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Commentary);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Engaged);
    QVERIFY(director.commentaryInFlight());

    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");
}

void RuntimeDirectorTest::scriptedTrajectoryCompletionIsIgnoredAfterCommentaryTakesOver()
{
    QTemporaryFile file;
    QVERIFY(file.open());
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":10,"y":10,"state":6}]})");
    file.flush();

    qputenv("CYBERCOMPANION_TRAJECTORY_PATH", file.fileName().toUtf8());

    RuntimeDirector director;
    director.setScriptedEntranceEnabled(true);
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);

    director.summonNow();
    director.requestScreenCommentary();
    QTRY_COMPARE_WITH_TIMEOUT(commentarySpy.count(), 1, 100);

    director.onScriptedTrajectoryFinished();

    QVERIFY(director.commentaryInFlight());
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Commentary);
    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Engaged);

    qunsetenv("CYBERCOMPANION_TRAJECTORY_PATH");
}

void RuntimeDirectorTest::requestScreenCommentaryFromHiddenStagesSummon()
{
    RuntimeDirector director;
    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);

    director.requestScreenCommentary();

    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Peeking);
    QCOMPARE(commentarySpy.count(), 0);

    QTRY_COMPARE_WITH_TIMEOUT(commentarySpy.count(), 1, 1500);
    QTRY_VERIFY_WITH_TIMEOUT(stateSpy.count() >= 2, 1500);

    QCOMPARE(stateSpy.at(stateSpy.count() - 1).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Engaged);
    QVERIFY(director.commentaryInFlight());
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Commentary);
}

void RuntimeDirectorTest::duplicateCommentaryRequestsAreIgnored()
{
    RuntimeDirector director;
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);

    director.summonNow();
    director.requestScreenCommentary();
    director.requestScreenCommentary();

    QCOMPARE(commentarySpy.count(), 1);
    QVERIFY(director.commentaryInFlight());
}

void RuntimeDirectorTest::canceledCommentaryResultIsIgnored()
{
    RuntimeDirector director;
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    director.summonNow();
    director.requestScreenCommentary();
    director.triggerFlee();
    speechSpy.clear();
    director.onCommentaryReady(QStringLiteral("late result"));

    QCOMPARE(speechSpy.count(), 0);
}

void RuntimeDirectorTest::commentaryFailureUsesHighPrioritySpeechRequest()
{
    RuntimeDirector director;
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    director.summonNow();
    director.requestScreenCommentary();
    speechSpy.clear();
    director.onCommentaryFailed(QStringLiteral("network failure"));

    QCOMPARE(speechSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QCOMPARE(request.text, QStringLiteral("network failure"));
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::High));
    QCOMPARE(request.interrupt, false);
}

void RuntimeDirectorTest::commentaryStartEmitsPreambleSpeechWhenConfigured()
{
    RuntimeDirector director;
    director.setScreenCommentarySpeechOptions(true, 12, 90, QStringLiteral("让我先看看。"));
    director.summonNow();

    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);
    speechSpy.clear();

    director.requestScreenCommentary();

    QVERIFY(speechSpy.count() >= 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QCOMPARE(request.text, QStringLiteral("让我先看看。"));
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::High));
    QCOMPARE(request.interrupt, true);
}

void RuntimeDirectorTest::commentaryReadySplitsSpeechWhenStreamingEnabled()
{
    RuntimeDirector director;
    director.setScreenCommentarySpeechOptions(true, 8, 40, QStringLiteral("前导语"));
    director.summonNow();

    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);
    director.requestScreenCommentary();
    speechSpy.clear();

    director.onCommentaryReady(QStringLiteral("这是第一句。这里是第二句。然后是第三句。"));

    QVERIFY(speechSpy.count() >= 2);
    QCOMPARE(speechSpy.at(0).at(0).value<AudioPlaybackRequest>().text, QStringLiteral("这是第一句。"));
    QCOMPARE(speechSpy.at(1).at(0).value<AudioPlaybackRequest>().text, QStringLiteral("这里是第二句。"));
}

void RuntimeDirectorTest::requestScreenCommentarySkipsWhenOfflineModeEnabled()
{
    RuntimeDirector director;
    director.setOfflineMode(true);
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);
    QSignalSpy statusSpy(&director, &RuntimeDirector::statusTextChanged);

    director.requestScreenCommentary();

    QCOMPARE(commentarySpy.count(), 0);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);
    QVERIFY(statusSpy.count() >= 1);
    QCOMPARE(statusSpy.at(statusSpy.count() - 1).at(0).toString(), QStringLiteral("离线模式"));
}

void RuntimeDirectorTest::autoScreenCommentaryTimeoutRequestsCommentaryWhenEnabled()
{
    RuntimeDirector director;
    director.setScreenCommentaryAutoEnabled(true);
    director.setScreenCommentaryAutoIntervalMinutes(1);
    director.summonNow();
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);

    QVERIFY(QMetaObject::invokeMethod(&director, "onAutoScreenCommentaryTimeout", Qt::DirectConnection));

    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Commentary);
    QCOMPARE(commentarySpy.count(), 1);
}

void RuntimeDirectorTest::autoScreenCommentaryTimeoutSkipsWhenDndEnabled()
{
    RuntimeDirector director;
    director.setScreenCommentaryAutoEnabled(true);
    director.setScreenCommentaryAutoIntervalMinutes(1);
    director.setDndMode(true);
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);

    QVERIFY(QMetaObject::invokeMethod(&director, "onAutoScreenCommentaryTimeout", Qt::DirectConnection));

    QCOMPARE(commentarySpy.count(), 0);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);
}

void RuntimeDirectorTest::autoScreenCommentaryTimeoutSkipsWhenOfflineModeEnabled()
{
    RuntimeDirector director;
    director.setScreenCommentaryAutoEnabled(true);
    director.setScreenCommentaryAutoIntervalMinutes(1);
    director.setOfflineMode(true);
    QSignalSpy commentarySpy(&director, &RuntimeDirector::screenCommentaryRequested);
    QSignalSpy statusSpy(&director, &RuntimeDirector::statusTextChanged);

    QVERIFY(QMetaObject::invokeMethod(&director, "onAutoScreenCommentaryTimeout", Qt::DirectConnection));

    QCOMPARE(commentarySpy.count(), 0);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);
    QVERIFY(statusSpy.count() >= 1);
    QCOMPARE(statusSpy.at(statusSpy.count() - 1).at(0).toString(), QStringLiteral("离线模式"));
}

void RuntimeDirectorTest::periodicCameraDebugReturnsFalseWhenCameraUnavailable()
{
    RuntimeDirector director;
    director.setCameraEnabled(false);
    QSignalSpy stateSpy(&director, &RuntimeDirector::statusTextChanged);

    QVERIFY(!director.triggerPeriodicCameraCheckDebug());
    QVERIFY(stateSpy.count() >= 1);
    QVERIFY(stateSpy.at(stateSpy.count() - 1).at(1).toString().contains(QStringLiteral("摄像头未启用或尚未运行")));
}

void RuntimeDirectorTest::periodicCameraTimerRequestsVisionRuntimeWhenCameraStopped()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setPeriodicScanEnabled(true);

    QSignalSpy runtimeSpy(&director, &RuntimeDirector::visionRuntimeRequested);

    QVERIFY(QMetaObject::invokeMethod(&director, "onPeriodicCameraScanTimeout", Qt::DirectConnection));

    QCOMPARE(runtimeSpy.count(), 1);
    QCOMPARE(runtimeSpy.at(0).at(0).toBool(), false);
}

void RuntimeDirectorTest::deferredPeriodicCameraScanStartsAfterCameraBecomesReady()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setPeriodicScanEnabled(true);

    QSignalSpy runtimeSpy(&director, &RuntimeDirector::visionRuntimeRequested);
    QSignalSpy completedSpy(&director, &RuntimeDirector::periodicCameraScanCompleted);

    QVERIFY(QMetaObject::invokeMethod(&director, "onPeriodicCameraScanTimeout", Qt::DirectConnection));
    QCOMPARE(runtimeSpy.count(), 1);

    director.onCameraStateChanged(true);

    GazeSample sample;
    sample.faceDetected = true;
    sample.brightness = 90.0;
    sample.motionScore = 5.0;
    sample.emotionLabel = QStringLiteral("happy");
    sample.emotionScore = 0.8;
    for (int i = 0; i < 6; ++i) {
        director.onGazeUpdated(sample);
    }

    QVERIFY(QMetaObject::invokeMethod(&director, "onPeriodicCameraScanCollectTimeout", Qt::DirectConnection));

    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), false);
}

void RuntimeDirectorTest::periodicCameraDebugCollectsSamplesAndPromotesPresence()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setPeriodicScanEnabled(true);
    director.onCameraStateChanged(true);

    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy overrideSpy(&director, &RuntimeDirector::entityStateOverrideRequested);

    director.setPeriodicScanIntervalMinutes(5);
    QVERIFY(QMetaObject::invokeMethod(&director, "onPeriodicCameraScanTimeout", Qt::DirectConnection));

    GazeSample sample;
    sample.faceDetected = true;
    sample.brightness = 90.0;
    sample.motionScore = 5.0;
    sample.emotionLabel = QStringLiteral("happy");
    sample.emotionScore = 0.8;
    for (int i = 0; i < 6; ++i) {
        director.onGazeUpdated(sample);
    }

    QVERIFY(QMetaObject::invokeMethod(&director, "onPeriodicCameraScanCollectTimeout", Qt::DirectConnection));

    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Engaged);
    QVERIFY(stateSpy.count() >= 1);
    QVERIFY(overrideSpy.count() >= 1);
    QCOMPARE(overrideSpy.at(overrideSpy.count() - 1).at(0).toString(), QStringLiteral("state6"));
}

void RuntimeDirectorTest::cameraStateChangeEmitsStatusWhenCameraEnabled()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    QSignalSpy statusSpy(&director, &RuntimeDirector::statusTextChanged);

    director.onCameraStateChanged(true);
    QVERIFY(statusSpy.count() >= 1);
    QCOMPARE(statusSpy.at(0).at(0).toString(), QStringLiteral("摄像头已连接"));

    director.onCameraStateChanged(false);
    QVERIFY(statusSpy.count() >= 2);
    QCOMPARE(statusSpy.at(statusSpy.count() - 1).at(0).toString(), QStringLiteral("摄像头已停止"));
}

void RuntimeDirectorTest::passivePresenceSummonsSilentCompanionWhenHidden()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setPresenceTargetFps(15);
    director.onIdleTimeUpdated(300000);

    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QSignalSpy overrideSpy(&director, &RuntimeDirector::entityStateOverrideRequested);
    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);

    GazeSample sample;
    sample.faceDetected = true;
    sample.brightness = 90.0;
    sample.motionScore = 1.0;

    director.onGazeUpdated(sample);

    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);
    QVERIFY(stateSpy.count() >= 1);
    QVERIFY(overrideSpy.count() >= 1);
    QCOMPARE(overrideSpy.at(overrideSpy.count() - 1).at(0).toString(), QStringLiteral("state5"));
    QCOMPARE(speechSpy.count(), 0);
}

void RuntimeDirectorTest::passivePresenceTimeoutHidesWhenNotResident()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setPresenceTargetFps(15);
    director.onIdleTimeUpdated(300000);

    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);

    GazeSample sample;
    sample.faceDetected = true;
    sample.brightness = 90.0;
    sample.motionScore = 1.0;
    director.onGazeUpdated(sample);
    stateSpy.clear();

    QVERIFY(QMetaObject::invokeMethod(&director, "onPassivePresenceTimeout", Qt::DirectConnection));

    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Hidden);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).value<RuntimeDirector::EntityState>(), RuntimeDirector::EntityState::Hidden);
}

void RuntimeDirectorTest::absentPresenceEntersDeepSleep()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setPresenceTargetFps(15);
    director.summonNow();
    director.onIdleTimeUpdated(300000);

    QSignalSpy overrideSpy(&director, &RuntimeDirector::entityStateOverrideRequested);

    GazeSample absent;
    absent.faceDetected = false;
    absent.brightness = 10.0;
    absent.motionScore = 0.1;
    for (int i = 0; i < 40; ++i) {
        director.onGazeUpdated(absent);
    }

    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Busy);
    QVERIFY(overrideSpy.count() >= 1);
    QCOMPARE(overrideSpy.at(overrideSpy.count() - 1).at(0).toString(), QStringLiteral("state5"));
}

void RuntimeDirectorTest::activePresenceCancelsPassiveCompanionTimeout()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setPresenceTargetFps(15);
    director.onIdleTimeUpdated(15000);
    director.onIdleTimeUpdated(300000);

    GazeSample passive;
    passive.faceDetected = true;
    passive.brightness = 90.0;
    passive.motionScore = 1.0;
    director.onGazeUpdated(passive);

    director.onIdleTimeUpdated(15000);
    GazeSample active = passive;
    active.motionScore = 8.0;
    director.onGazeUpdated(active);

    QSignalSpy stateSpy(&director, &RuntimeDirector::stateChanged);
    QVERIFY(QMetaObject::invokeMethod(&director, "onPassivePresenceTimeout", Qt::DirectConnection));

    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(director.currentState(), RuntimeDirector::EntityState::Engaged);
    QCOMPARE(director.currentBehaviorMode(), RuntimeDirector::BehaviorMode::Idle);
}

void RuntimeDirectorTest::prolongedIdlePulseEmitsWhenHidden()
{
    RuntimeDirector director;
    QSignalSpy pulseSpy(&director, &RuntimeDirector::prolongedIdlePulseRequested);

    director.start();
    pulseSpy.clear();

    QVERIFY(QMetaObject::invokeMethod(&director, "onProlongedIdleTimeout", Qt::DirectConnection));

    QCOMPARE(pulseSpy.count(), 1);
}

void RuntimeDirectorTest::prolongedIdlePulseIsSuppressedWhenIdleInvasionEnabled()
{
    RuntimeDirector director;
    director.setIdleInvasionEnabled(true);
    QSignalSpy pulseSpy(&director, &RuntimeDirector::prolongedIdlePulseRequested);

    director.start();
    pulseSpy.clear();

    QVERIFY(QMetaObject::invokeMethod(&director, "onProlongedIdleTimeout", Qt::DirectConnection));

    QCOMPARE(pulseSpy.count(), 0);
}

void RuntimeDirectorTest::gazeUpdateEmitsGazeFollowWhenEnabled()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setEyeTrackingEnabled(true);
    director.summonNow();
    QSignalSpy gazeSpy(&director, &RuntimeDirector::gazeFollowRequested);

    GazeSample sample;
    sample.faceDetected = true;
    sample.faceX = 0.25;
    sample.faceY = -0.1;
    sample.confidence = 0.85;
    sample.motionScore = 1.0;
    sample.brightness = 90.0;

    director.onGazeUpdated(sample);

    QCOMPARE(gazeSpy.count(), 1);
    QCOMPARE(gazeSpy.at(0).at(0).toDouble(), 0.25);
    QCOMPARE(gazeSpy.at(0).at(1).toDouble(), -0.1);
    QCOMPARE(gazeSpy.at(0).at(2).toBool(), true);
    QCOMPARE(gazeSpy.at(0).at(3).toDouble(), 0.85);
}

void RuntimeDirectorTest::gazeUpdateDoesNotEmitGazeFollowWhenEyeTrackingDisabled()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setEyeTrackingEnabled(false);
    director.summonNow();
    QSignalSpy gazeSpy(&director, &RuntimeDirector::gazeFollowRequested);

    GazeSample sample;
    sample.faceDetected = true;
    sample.faceX = 0.4;
    sample.faceY = 0.0;
    sample.confidence = 0.8;

    director.onGazeUpdated(sample);

    QCOMPARE(gazeSpy.count(), 0);
}

void RuntimeDirectorTest::stableHappyExpressionEmitsVisualOverrideWhenIdle()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setResidentModeEnabled(true);
    director.start();

    QSignalSpy overrideSpy(&director, &RuntimeDirector::entityStateOverrideRequested);
    overrideSpy.clear();

    GazeSample sample;
    sample.faceDetected = true;
    sample.emotionLabel = QStringLiteral("happy");
    sample.emotionScore = 0.8;

    director.onGazeUpdated(sample);
    director.onGazeUpdated(sample);

    QCOMPARE(overrideSpy.count(), 1);
    QCOMPARE(overrideSpy.at(0).at(0).toString(), QStringLiteral("state6"));
}

void RuntimeDirectorTest::expressionTrackingIsSuppressedOutsideIdleMode()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.summonNow();

    QSignalSpy overrideSpy(&director, &RuntimeDirector::entityStateOverrideRequested);
    overrideSpy.clear();

    GazeSample sample;
    sample.faceDetected = true;
    sample.emotionLabel = QStringLiteral("happy");
    sample.emotionScore = 0.8;

    director.onGazeUpdated(sample);
    director.onGazeUpdated(sample);

    QCOMPARE(overrideSpy.count(), 0);
}

void RuntimeDirectorTest::happyExpressionRaisesMoodWhenStabilized()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setResidentModeEnabled(true);
    director.start();

    const double initialMood = director.currentMoodValue();

    GazeSample sample;
    sample.faceDetected = true;
    sample.emotionLabel = QStringLiteral("happy");
    sample.emotionScore = 0.8;

    director.onGazeUpdated(sample);
    director.onGazeUpdated(sample);

    QVERIFY(director.currentMoodValue() > initialMood);
    QVERIFY(
        director.currentMoodLabel() == QStringLiteral("平静")
        || director.currentMoodLabel() == QStringLiteral("开心"));
}

void RuntimeDirectorTest::sadEmotionAutoTriggersComfortSpeech()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setResidentModeEnabled(true);
    director.start();

    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);
    speechSpy.clear();

    GazeSample sample;
    sample.faceDetected = true;
    sample.emotionLabel = QStringLiteral("sad");
    sample.emotionScore = 0.9;

    director.onGazeUpdated(sample);
    director.onGazeUpdated(sample);

    QCOMPARE(speechSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::High));
    QVERIFY(request.text.contains(QStringLiteral("别太难过")));
}

void RuntimeDirectorTest::sadEmotionAutoTriggerRespectsCooldown()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setResidentModeEnabled(true);
    director.start();

    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);
    speechSpy.clear();

    GazeSample sample;
    sample.faceDetected = true;
    sample.emotionLabel = QStringLiteral("sad");
    sample.emotionScore = 0.9;

    director.onGazeUpdated(sample);
    director.onGazeUpdated(sample);
    director.onGazeUpdated(sample);

    QCOMPARE(speechSpy.count(), 1);
}

void RuntimeDirectorTest::noFaceReturnGreetsUserAfterQualifiedAbsence()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setVisionFeedbackTimingsForTest(40, 80, 500, 200, 800);
    director.summonNow();

    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);
    QSignalSpy overrideSpy(&director, &RuntimeDirector::entityStateOverrideRequested);
    speechSpy.clear();
    overrideSpy.clear();

    GazeSample absent;
    absent.faceDetected = false;

    director.onGazeUpdated(absent);
    QTest::qWait(50);
    director.onGazeUpdated(absent);
    QTest::qWait(50);

    GazeSample present;
    present.faceDetected = true;
    director.onGazeUpdated(present);

    QCOMPARE(speechSpy.count(), 1);
    QCOMPARE(overrideSpy.count(), 1);
    const AudioPlaybackRequest request = speechSpy.at(0).at(0).value<AudioPlaybackRequest>();
    QCOMPARE(request.priority, static_cast<int>(AudioPlaybackPriority::High));
    QCOMPARE(request.interrupt, false);
    QVERIFY(
        request.text == QStringLiteral("回来啦~")
        || request.text == QStringLiteral("欸你回来了。"));
}

void RuntimeDirectorTest::noFaceReturnSkipsGreetingWhenAwayTooShort()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setVisionFeedbackTimingsForTest(20, 300, 500, 600, 1200);
    director.summonNow();

    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);
    speechSpy.clear();

    GazeSample absent;
    absent.faceDetected = false;
    director.onGazeUpdated(absent);
    QTest::qWait(30);
    director.onGazeUpdated(absent);
    QTest::qWait(30);

    GazeSample present;
    present.faceDetected = true;
    director.onGazeUpdated(present);

    QCOMPARE(speechSpy.count(), 0);
}

void RuntimeDirectorTest::noFaceReturnGreetingRespectsCooldown()
{
    RuntimeDirector director;
    director.setCameraEnabled(true);
    director.setVisionFeedbackTimingsForTest(30, 60, 600, 200, 800);
    director.summonNow();

    QSignalSpy speechSpy(&director, &RuntimeDirector::speechRequestRequested);
    speechSpy.clear();

    GazeSample absent;
    absent.faceDetected = false;
    GazeSample present;
    present.faceDetected = true;

    director.onGazeUpdated(absent);
    QTest::qWait(40);
    director.onGazeUpdated(absent);
    QTest::qWait(40);
    director.onGazeUpdated(present);
    QCOMPARE(speechSpy.count(), 1);

    director.onGazeUpdated(absent);
    QTest::qWait(40);
    director.onGazeUpdated(absent);
    QTest::qWait(40);
    director.onGazeUpdated(present);

    QCOMPARE(speechSpy.count(), 1);
}

QTEST_MAIN(RuntimeDirectorTest)

#include "test_runtime_director.moc"
