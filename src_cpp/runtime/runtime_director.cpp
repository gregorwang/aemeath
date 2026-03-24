#include "runtime/runtime_director.h"

#include <algorithm>
#include <array>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QRandomGenerator>
#include <QTimer>

#include "runtime/scripted_trajectory_catalog.h"

namespace {

constexpr int kPeekAdvanceMs = 1400;
constexpr int kFleeHideMs = 900;
constexpr int kCommentarySummonDelayMs = 800;
constexpr int kPassiveCompanionDurationMs = 30000;
constexpr int kProlongedIdlePulseMs = 10 * 60 * 1000;
constexpr int kSadComfortCooldownMs = 5 * 60 * 1000;
constexpr int kExpressionVisualIntervalMs = 800;
constexpr int kMoodDecayIntervalMs = 60 * 60 * 1000;
constexpr int kMinimumAutoCommentaryIntervalMs = 60 * 1000;
constexpr int kPeriodicCameraScanDurationMs = 8000;
constexpr double kPeriodicCameraMinFaceRatio = 0.25;
constexpr double kPeriodicCameraMinPresenceRatio = 0.35;
constexpr double kSadComfortMinScore = 0.6;
constexpr auto kUserReturnShortTexts = std::array<const char *, 2>{
    "回来啦~",
    "欸你回来了。",
};
constexpr auto kUserReturnMediumTexts = std::array<const char *, 2>{
    "你去了{minutes}分钟哦，我一直在等你呢。",
    "去了{minutes}分钟，还以为你把我忘了。",
};
constexpr auto kUserReturnLongTexts = std::array<const char *, 2>{
    "你离开了好久！去做什么了？",
    "终于回来了！我都快睡着了。",
};
constexpr auto kDefaultTrajectoryFile = "recorded_paths/trajectory_1771029879_qt_animation.json";
constexpr auto kDefaultTrajectoryDirectory = "recorded_paths";

QString expressionStateForLabel(const QString &label)
{
    const QString normalized = label.trimmed().toLower();
    if (normalized == QStringLiteral("happy")) {
        return QStringLiteral("state6");
    }
    if (normalized == QStringLiteral("sad")) {
        return QStringLiteral("state5");
    }
    if (normalized == QStringLiteral("angry")) {
        return QStringLiteral("state4");
    }
    return QStringLiteral("state1");
}

}

RuntimeDirector::RuntimeDirector(QObject *parent)
    : QObject(parent)
    , m_idlePeekTimer(new QTimer(this))
    , m_peekAdvanceTimer(new QTimer(this))
    , m_autoDismissTimer(new QTimer(this))
    , m_fleeHideTimer(new QTimer(this))
    , m_commentaryStartTimer(new QTimer(this))
    , m_autoScreenCommentaryTimer(new QTimer(this))
    , m_periodicScanTimer(new QTimer(this))
    , m_periodicScanCollectTimer(new QTimer(this))
    , m_passivePresenceTimer(new QTimer(this))
    , m_moodDecayTimer(new QTimer(this))
    , m_prolongedIdleTimer(new QTimer(this))
{
    m_idlePeekTimer->setSingleShot(true);
    m_peekAdvanceTimer->setSingleShot(true);
    m_autoDismissTimer->setSingleShot(true);
    m_fleeHideTimer->setSingleShot(true);
    m_commentaryStartTimer->setSingleShot(true);
    m_autoScreenCommentaryTimer->setSingleShot(true);
    m_periodicScanTimer->setSingleShot(true);
    m_periodicScanCollectTimer->setSingleShot(true);
    m_passivePresenceTimer->setSingleShot(true);
    m_moodDecayTimer->setInterval(kMoodDecayIntervalMs);
    m_prolongedIdleTimer->setSingleShot(true);

    connect(m_idlePeekTimer, &QTimer::timeout, this, &RuntimeDirector::onIdlePeekTimeout);
    connect(m_peekAdvanceTimer, &QTimer::timeout, this, &RuntimeDirector::onPeekToEngagedTimeout);
    connect(m_autoDismissTimer, &QTimer::timeout, this, &RuntimeDirector::onAutoDismissTimeout);
    connect(m_fleeHideTimer, &QTimer::timeout, this, &RuntimeDirector::onFleeHideTimeout);
    connect(m_commentaryStartTimer, &QTimer::timeout, this, &RuntimeDirector::onCommentaryStartTimeout);
    connect(m_autoScreenCommentaryTimer, &QTimer::timeout, this, &RuntimeDirector::onAutoScreenCommentaryTimeout);
    connect(m_periodicScanTimer, &QTimer::timeout, this, &RuntimeDirector::onPeriodicCameraScanTimeout);
    connect(m_periodicScanCollectTimer, &QTimer::timeout, this, &RuntimeDirector::onPeriodicCameraScanCollectTimeout);
    connect(m_passivePresenceTimer, &QTimer::timeout, this, &RuntimeDirector::onPassivePresenceTimeout);
    connect(m_prolongedIdleTimer, &QTimer::timeout, this, &RuntimeDirector::onProlongedIdleTimeout);
    connect(m_moodDecayTimer, &QTimer::timeout, this, [this]() {
        m_moodSystem.naturalDecay();
    });

    qRegisterMetaType<RuntimeDirector::EntityState>("RuntimeDirector::EntityState");
    qRegisterMetaType<RuntimeDirector::BehaviorMode>("RuntimeDirector::BehaviorMode");
    qRegisterMetaType<AudioPlaybackRequest>("AudioPlaybackRequest");
    qRegisterMetaType<GazeSample>("GazeSample");

    m_expressionVotes.insert(QStringLiteral("happy"), 0);
    m_expressionVotes.insert(QStringLiteral("neutral"), 0);
    m_expressionVotes.insert(QStringLiteral("angry"), 0);
    m_expressionVotes.insert(QStringLiteral("sad"), 0);
    m_moodDecayTimer->start();
}

void RuntimeDirector::start()
{
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Hidden, QStringLiteral("待机中"), QStringLiteral("等待下一次出现时机"));
    syncResidentVisibility();
    m_idlePeekTimer->stop();
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

RuntimeDirector::EntityState RuntimeDirector::currentState() const
{
    return m_state;
}

RuntimeDirector::BehaviorMode RuntimeDirector::currentBehaviorMode() const
{
    return m_behaviorMode;
}

bool RuntimeDirector::commentaryInFlight() const
{
    return m_commentaryPending || m_commentaryActive;
}

void RuntimeDirector::setScriptedEntranceEnabled(bool enabled)
{
    m_scriptedEntranceEnabled = enabled;
}

void RuntimeDirector::setScriptedTrajectoryPath(const QString &path)
{
    m_scriptedTrajectoryPath = path.trimmed();
}

void RuntimeDirector::setVoiceScriptsPath(const QString &path)
{
    m_voiceScriptCatalog.setScriptsPath(path);
}

void RuntimeDirector::setAudioOutputReactive(bool enabled)
{
    m_audioOutputReactive = enabled;
    if (enabled) {
        return;
    }

    m_audioOutputActive = false;
    if (m_behaviorMode != BehaviorMode::MediaPlaying) {
        return;
    }

    if (m_state == EntityState::Hidden) {
        setBehaviorMode(BehaviorMode::Busy);
    } else {
        setBehaviorMode(BehaviorMode::Idle);
    }
}

void RuntimeDirector::setAutoDismissSeconds(int seconds)
{
    m_autoDismissMs = qMax(1000, seconds * 1000);
}

void RuntimeDirector::setIdleInvasionEnabled(bool enabled)
{
    m_idleInvasionEnabled = enabled;
    m_idlePeekTimer->stop();
}

void RuntimeDirector::setIdleInvasionStartDelayMs(int startDelayMs)
{
    m_idleInvasionStartDelayMs = qMax(1000, startDelayMs);
}

void RuntimeDirector::setScreenCommentaryAutoEnabled(bool enabled)
{
    m_screenCommentaryAutoEnabled = enabled;
    syncAutoScreenCommentaryTimer();
}

void RuntimeDirector::setOfflineMode(bool enabled)
{
    m_offlineMode = enabled;
    syncAutoScreenCommentaryTimer();
}

void RuntimeDirector::setScreenCommentaryAutoIntervalMinutes(int minutes)
{
    m_screenCommentaryAutoIntervalMs = qMax(kMinimumAutoCommentaryIntervalMs, minutes * 60 * 1000);
    syncAutoScreenCommentaryTimer();
}

void RuntimeDirector::setScreenCommentarySpeechOptions(
    bool streamingEnabled,
    int streamChunkChars,
    int maxResponseChars,
    const QString &preambleText)
{
    m_commentaryStreamingEnabled = streamingEnabled;
    m_commentaryStreamChunkChars = qBound(8, streamChunkChars, 80);
    m_commentaryMaxResponseChars = qBound(20, maxResponseChars, 300);
    m_commentaryPreambleText = preambleText.trimmed();
}

void RuntimeDirector::setFullScreenPauseEnabled(bool enabled)
{
    m_fullScreenPauseEnabled = enabled;
    syncResidentVisibility();
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

void RuntimeDirector::setResidentModeEnabled(bool enabled)
{
    m_residentModeEnabled = enabled;
    syncResidentVisibility();
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

void RuntimeDirector::setDndMode(bool enabled)
{
    m_dndMode = enabled;
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
    if (m_dndMode) {
        stopTimers();
        if (m_state == EntityState::Peeking || m_state == EntityState::Engaged) {
            transitionTo(EntityState::Hidden, QStringLiteral("请勿打扰"), QStringLiteral("已暂停自动出场与空闲行为"));
        } else {
            Q_EMIT statusTextChanged(QStringLiteral("请勿打扰"), QStringLiteral("已暂停自动出场与空闲行为"));
        }
        return;
    }

    if (m_state == EntityState::Hidden && !commentaryInFlight() && !m_scriptedEntranceActive && !m_idleInvasionEnabled) {
        m_idlePeekTimer->stop();
    }
    syncPeriodicScanTimer();
}

void RuntimeDirector::setVisionFeedbackTimingsForTest(
    int noFaceMinAbsenceMs,
    int userReturnMinSpeakMs,
    int noFaceCooldownMs,
    int userReturnMediumThresholdMs,
    int userReturnLongThresholdMs)
{
    m_noFaceMinAbsenceMs = qMax(0, noFaceMinAbsenceMs);
    m_userReturnMinSpeakMs = qMax(0, userReturnMinSpeakMs);
    m_noFaceCooldownMs = qMax(0, noFaceCooldownMs);
    m_userReturnMediumThresholdMs = qMax(m_userReturnMinSpeakMs, userReturnMediumThresholdMs);
    m_userReturnLongThresholdMs = qMax(m_userReturnMediumThresholdMs + 1, userReturnLongThresholdMs);
}

bool RuntimeDirector::isDndMode() const
{
    return m_dndMode;
}

bool RuntimeDirector::isResidentModeEnabled() const
{
    return m_residentModeEnabled;
}

bool RuntimeDirector::isAudioOutputActive() const
{
    return m_audioOutputActive;
}

int RuntimeDirector::autoDismissMs() const
{
    return m_autoDismissMs;
}

PresenceState RuntimeDirector::currentPresenceState() const
{
    return m_presenceState;
}

double RuntimeDirector::currentMoodValue() const
{
    return m_moodSystem.mood();
}

QString RuntimeDirector::currentMoodLabel() const
{
    return m_moodSystem.moodLabel();
}

bool RuntimeDirector::triggerScriptedTrajectoryDebug()
{
    if (m_scriptedEntranceActive) {
        Q_EMIT statusTextChanged(QStringLiteral("轨迹登场调试"), QStringLiteral("已跳过：脚本轨迹正在播放"));
        return false;
    }
    if (m_state == EntityState::Fleeing) {
        Q_EMIT statusTextChanged(QStringLiteral("轨迹登场调试"), QStringLiteral("已跳过：当前处于撤退状态"));
        return false;
    }

    const QString scriptedPath = resolveScriptedTrajectoryPath();
    if (scriptedPath.isEmpty()) {
        Q_EMIT statusTextChanged(QStringLiteral("轨迹登场调试"), QStringLiteral("未找到可用的脚本轨迹文件"));
        return false;
    }

    stopTimers();
    completeCommentarySession();
    m_scriptedEntranceActive = true;
    setBehaviorMode(BehaviorMode::Summoning);
    transitionTo(EntityState::Peeking, QStringLiteral("轨迹登场调试"), QStringLiteral("正在播放脚本式轨迹登场"));
    Q_EMIT scriptedTrajectoryRequested(scriptedPath);
    return true;
}

bool RuntimeDirector::triggerSadComfortDebug()
{
    if (m_scriptedEntranceActive || commentaryInFlight() || m_state == EntityState::Fleeing) {
        Q_EMIT statusTextChanged(QStringLiteral("悲伤安慰调试"), QStringLiteral("已跳过：当前状态不适合触发安慰语音"));
        return false;
    }

    stopTimers();
    m_passivePresenceActive = false;
    m_deepSleepActive = false;
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Engaged, QStringLiteral("悲伤安慰调试"), QStringLiteral("正在触发安慰语音"));
    emitSadComfortSpeech();
    armAutoDismiss(m_autoDismissMs);
    return true;
}

bool RuntimeDirector::triggerNoFaceTestDebug()
{
    if (m_scriptedEntranceActive || commentaryInFlight() || m_state == EntityState::Fleeing) {
        Q_EMIT statusTextChanged(QStringLiteral("无人脸提醒调试"), QStringLiteral("已跳过：当前状态不适合触发无人脸提醒"));
        return false;
    }

    stopTimers();
    m_passivePresenceActive = false;
    m_deepSleepActive = false;
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Engaged, QStringLiteral("无人脸提醒调试"), QStringLiteral("正在触发无人脸提醒语音"));
    Q_EMIT entityStateOverrideRequested(QStringLiteral("state5"));
    emitPanicSpeechRequest();
    armAutoDismiss(m_autoDismissMs);
    return true;
}

bool RuntimeDirector::triggerPeriodicCameraCheckDebug()
{
    return startPeriodicCameraScan(QStringLiteral("manual"), true);
}

void RuntimeDirector::summonNow()
{
    stopTimers();
    completeCommentarySession();
    m_scriptedEntranceActive = false;
    setBehaviorMode(BehaviorMode::Busy);

    if (m_state == EntityState::Hidden && m_scriptedEntranceEnabled) {
        const QString scriptedPath = resolveScriptedTrajectoryPath();
        if (!scriptedPath.isEmpty()) {
            m_scriptedEntranceActive = true;
            setBehaviorMode(BehaviorMode::Summoning);
            transitionTo(EntityState::Peeking, QStringLiteral("脚本式登场"), QStringLiteral("正在播放录制轨迹"));
            Q_EMIT scriptedTrajectoryRequested(scriptedPath);
            return;
        }
    }

    transitionTo(EntityState::Engaged, QStringLiteral("已召唤"), QStringLiteral("角色已进入交互状态"));
    m_moodSystem.onInteracted();
    emitIdleSpeechRequest();
    armAutoDismiss(m_autoDismissMs);
}

void RuntimeDirector::triggerPeek()
{
    if (m_state != EntityState::Hidden || commentaryInFlight()) {
        return;
    }
    m_idlePeekTimer->stop();
    setBehaviorMode(BehaviorMode::Idle);
    transitionTo(EntityState::Peeking, QStringLiteral("探头观察"), QStringLiteral("角色正在试探性出现"));
    m_peekAdvanceTimer->start(kPeekAdvanceMs);
}

void RuntimeDirector::triggerFlee()
{
    if (m_state == EntityState::Hidden) {
        return;
    }
    stopTimers();
    completeCommentarySession();
    resetNoFaceTracker();
    m_scriptedEntranceActive = false;
    if (m_state == EntityState::Peeking || m_state == EntityState::Engaged) {
        m_moodSystem.onDismissed();
    }
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Fleeing, QStringLiteral("撤退中"), QStringLiteral("角色准备离开屏幕"));
    emitPanicSpeechRequest();
    m_fleeHideTimer->start(kFleeHideMs);
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
}

void RuntimeDirector::hideNow()
{
    stopTimers();
    completeCommentarySession();
    resetNoFaceTracker();
    m_scriptedEntranceActive = false;
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Hidden, QStringLiteral("已隐藏"), QStringLiteral("角色暂时离开"));
    m_idlePeekTimer->stop();
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
}

void RuntimeDirector::playDemoTrajectory()
{
    QVector<QPoint> points;
    points << QPoint(0, 0)
           << QPoint(-70, -24)
           << QPoint(-130, 18)
           << QPoint(-90, 48)
           << QPoint(0, 0);
    m_scriptedEntranceActive = false;
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Engaged, QStringLiteral("轨迹演示"), QStringLiteral("正在播放原生 QWidget 轨迹动画"));
    Q_EMIT trajectoryRequested(points, 2400);
    armAutoDismiss(m_autoDismissMs);
}

void RuntimeDirector::onScriptedTrajectoryFinished()
{
    if (!m_scriptedEntranceActive) {
        return;
    }

    m_scriptedEntranceActive = false;
    if (m_commentaryPending) {
        setBehaviorMode(BehaviorMode::Commentary);
        transitionTo(EntityState::Engaged, QStringLiteral("屏幕评论中"), QStringLiteral("脚本式登场完成，开始请求屏幕评论"));
        onCommentaryStartTimeout();
        return;
    }
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Hidden, QStringLiteral("轨迹登场完成"), QStringLiteral("脚本式轨迹已播放完毕并已隐藏"));
    if (!m_idleInvasionEnabled) {
        m_idlePeekTimer->start(nextIdleIntervalMs());
    }
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
}

void RuntimeDirector::onUserIdleDetected()
{
    if (m_scriptedEntranceActive) {
        return;
    }
    if (residentActive()) {
        syncResidentVisibility();
        return;
    }
    if (m_state != EntityState::Hidden) {
        return;
    }
    if (m_dndMode) {
        return;
    }
    if (m_fullScreenPauseEnabled && m_fullscreenActive) {
        return;
    }
    if (m_idleInvasionEnabled) {
        return;
    }
    if (commentaryInFlight()) {
        return;
    }
    setBehaviorMode(BehaviorMode::Idle);
    transitionTo(EntityState::Engaged, QStringLiteral("空闲召唤"), QStringLiteral("检测到用户空闲，角色已进入交互状态"));
    m_moodSystem.onInteracted();
    emitIdleSpeechRequest();
    armAutoDismiss(m_autoDismissMs);
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
}

void RuntimeDirector::onIdleTimeUpdated(qint64 idleMs)
{
    m_latestIdleTimeMs = qMax<qint64>(0, idleMs);
    if (m_cameraEnabled) {
        refreshPresenceState(&m_latestGazeSample);
    }
}

void RuntimeDirector::onUserActivityDetected()
{
    if (commentaryInFlight()) {
        syncAutoScreenCommentaryTimer();
        return;
    }
    if (residentActive()) {
        stopTimers();
        setBehaviorMode(BehaviorMode::Idle);
        Q_EMIT statusTextChanged(QStringLiteral("常驻模式"), QStringLiteral("保持可见，不执行自动撤退"));
        syncAutoScreenCommentaryTimer();
        return;
    }
    if (m_state == EntityState::Peeking || m_state == EntityState::Engaged) {
        triggerFlee();
    }
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
}

void RuntimeDirector::requestScreenCommentary()
{
    if (m_offlineMode) {
        Q_EMIT statusTextChanged(QStringLiteral("离线模式"), QStringLiteral("已启用离线模式，屏幕评论不可用"));
        syncAutoScreenCommentaryTimer();
        return;
    }
    if (commentaryInFlight()) {
        Q_EMIT statusTextChanged(QStringLiteral("屏幕评论中"), QStringLiteral("已有评论请求在进行，已忽略重复触发"));
        syncAutoScreenCommentaryTimer();
        return;
    }

    stopTimers();
    m_scriptedEntranceActive = false;
    m_commentaryPending = true;
    m_commentaryActive = false;
    setBehaviorMode(BehaviorMode::Commentary);
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();

    if (m_state == EntityState::Hidden) {
        if (!m_scriptedEntranceEnabled) {
            transitionTo(EntityState::Peeking, QStringLiteral("准备评论"), QStringLiteral("角色正在接近屏幕"));
            m_commentaryStartTimer->start(kCommentarySummonDelayMs);
            return;
        }

        const QString scriptedPath = resolveScriptedTrajectoryPath();
        if (!scriptedPath.isEmpty()) {
            m_scriptedEntranceActive = true;
            setBehaviorMode(BehaviorMode::Summoning);
            transitionTo(EntityState::Peeking, QStringLiteral("准备评论"), QStringLiteral("先执行脚本式登场，再开始屏幕评论"));
            Q_EMIT scriptedTrajectoryRequested(scriptedPath);
            return;
        }

        transitionTo(EntityState::Peeking, QStringLiteral("准备评论"), QStringLiteral("角色正在接近屏幕"));
        m_commentaryStartTimer->start(kCommentarySummonDelayMs);
        return;
    }

    if (m_state == EntityState::Fleeing) {
        transitionTo(EntityState::Engaged, QStringLiteral("准备评论"), QStringLiteral("已打断撤退并恢复交互"));
    } else {
        transitionTo(EntityState::Engaged, QStringLiteral("屏幕评论中"), QStringLiteral("准备请求屏幕评论服务"));
    }
    onCommentaryStartTimeout();
}

void RuntimeDirector::onCommentaryReady(const QString &text)
{
    if (!m_commentaryActive) {
        return;
    }
    QString cleaned = text.simplified().trimmed();
    if (cleaned.size() > m_commentaryMaxResponseChars) {
        cleaned = cleaned.left(m_commentaryMaxResponseChars).trimmed();
    }
    if (cleaned.isEmpty()) {
        onCommentaryFailed(QStringLiteral("评论服务返回空内容"));
        return;
    }
    completeCommentarySession();
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Engaged, QStringLiteral("评论已返回"), cleaned);
    const QStringList chunks = splitCommentarySpeech(cleaned);
    for (int index = 0; index < chunks.size(); ++index) {
        Q_EMIT speechRequestRequested(AudioPlaybackRequest{
            chunks.at(index),
            static_cast<int>(AudioPlaybackPriority::Normal),
            false,
            QString(),
        });
    }
    armAutoDismiss(m_autoDismissMs);
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
}

void RuntimeDirector::onCommentaryFailed(const QString &error)
{
    if (!m_commentaryActive) {
        return;
    }
    const QString detail = error.trimmed().isEmpty()
        ? QStringLiteral("评论服务暂不可用")
        : error.trimmed();
    completeCommentarySession();
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Engaged, QStringLiteral("评论失败"), detail);
    Q_EMIT speechRequestRequested(AudioPlaybackRequest{
        detail,
        static_cast<int>(AudioPlaybackPriority::High),
        false,
        QString(),
    });
    armAutoDismiss(m_autoDismissMs);
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
}

void RuntimeDirector::onAudioOutputStarted()
{
    if (!m_audioOutputReactive) {
        return;
    }
    m_audioOutputActive = true;
    if (m_selfPlaybackActive || m_scriptedEntranceActive || commentaryInFlight()) {
        return;
    }
    setBehaviorMode(BehaviorMode::MediaPlaying);
    Q_EMIT statusTextChanged(QStringLiteral("媒体播放中"), QStringLiteral("检测到外部音频输出活动"));
}

void RuntimeDirector::onAudioOutputStopped()
{
    if (!m_audioOutputReactive) {
        return;
    }
    if (!m_audioOutputActive) {
        return;
    }
    m_audioOutputActive = false;
    if (m_selfPlaybackActive) {
        return;
    }
    if (m_behaviorMode == BehaviorMode::Summoning) {
        return;
    }
    if (m_state == EntityState::Hidden) {
        setBehaviorMode(BehaviorMode::Busy);
        return;
    }
    if (m_state == EntityState::Peeking || m_state == EntityState::Engaged || m_state == EntityState::Fleeing) {
        setBehaviorMode(BehaviorMode::Idle);
    }
}

void RuntimeDirector::onSelfPlaybackStarted(const QString &)
{
    m_selfPlaybackActive = true;
    if (!m_audioOutputActive) {
        return;
    }
    if (m_behaviorMode == BehaviorMode::MediaPlaying) {
        if (m_state == EntityState::Hidden) {
            setBehaviorMode(BehaviorMode::Busy);
        } else {
            setBehaviorMode(BehaviorMode::Idle);
        }
    }
}

void RuntimeDirector::onSelfPlaybackFinished()
{
    m_selfPlaybackActive = false;
    if (m_audioOutputReactive && m_audioOutputActive) {
        onAudioOutputStarted();
    }
}

void RuntimeDirector::onFullscreenStateChanged(bool fullscreenActive)
{
    m_fullscreenActive = fullscreenActive;
    syncResidentVisibility();
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

void RuntimeDirector::onCameraStateChanged(bool running)
{
    const bool previousRunning = m_cameraRunning;
    m_cameraRunning = running;
    if (!m_cameraRunning && m_periodicScanActive) {
        m_periodicScanCollectTimer->stop();
        m_periodicScanActive = false;
        m_periodicScanDebugMode = false;
        m_periodicScanSamples.clear();
    }
    if (!m_cameraRunning) {
        m_periodicScanPendingStart = false;
        m_periodicScanPendingDebugMode = false;
    }
    if (m_cameraEnabled) {
        if (m_cameraRunning && !previousRunning) {
            Q_EMIT statusTextChanged(QStringLiteral("摄像头已连接"), QStringLiteral("视觉巡检与视线跟随已可用"));
            if (m_periodicScanPendingStart) {
                const bool debugMode = m_periodicScanPendingDebugMode;
                m_periodicScanPendingStart = false;
                m_periodicScanPendingDebugMode = false;
                beginPeriodicCameraScan(QStringLiteral("camera_ready"), debugMode);
            }
        } else if (!m_cameraRunning && previousRunning) {
            Q_EMIT statusTextChanged(QStringLiteral("摄像头已停止"), QStringLiteral("视觉服务当前不可用"));
        }
    }
    syncPeriodicScanTimer();
}

void RuntimeDirector::onGazeUpdated(const GazeSample &sample)
{
    m_latestGazeSample = sample;

    if (m_eyeTrackingEnabled
        && (m_state == EntityState::Peeking || m_state == EntityState::Engaged)) {
        Q_EMIT gazeFollowRequested(
            sample.faceX,
            sample.faceY,
            sample.faceDetected,
            sample.confidence);
    }

    if (m_periodicScanActive) {
        m_periodicScanSamples.push_back(sample);
        if (m_periodicScanSamples.size() > 300) {
            m_periodicScanSamples.remove(0, m_periodicScanSamples.size() - 300);
        }
    }

    if (!m_cameraEnabled) {
        return;
    }
    refreshPresenceState(&m_latestGazeSample);
    if (m_periodicScanActive && m_periodicScanDebugMode) {
        return;
    }
    maybeTriggerNoFaceReturn(sample);
    trackExpressionState(sample);
    maybeTriggerSadComfort(sample);
}

void RuntimeDirector::onCameraError(const QString &message)
{
    m_cameraRunning = false;
    m_periodicScanTimer->stop();
    m_periodicScanCollectTimer->stop();
    m_periodicScanActive = false;
    m_periodicScanDebugMode = false;
    m_periodicScanSamples.clear();
    m_passivePresenceTimer->stop();
    m_passivePresenceActive = false;
    m_deepSleepActive = false;
    resetExpressionTracking();
    resetNoFaceTracker();
    m_presenceState = PresenceState::Unknown;
    Q_EMIT statusTextChanged(
        QStringLiteral("摄像头错误"),
        message.trimmed().isEmpty() ? QStringLiteral("视觉服务暂不可用") : message.trimmed());
}

void RuntimeDirector::onIdlePeekTimeout()
{
    triggerPeek();
}

void RuntimeDirector::onPeekToEngagedTimeout()
{
    if (m_state != EntityState::Peeking) {
        return;
    }
    setBehaviorMode(BehaviorMode::Busy);
    transitionTo(EntityState::Engaged, QStringLiteral("已接近"), QStringLiteral("角色已完成探头并停留"));
    m_moodSystem.onInteracted();
    emitIdleSpeechRequest();
    armAutoDismiss(m_autoDismissMs);
}

void RuntimeDirector::onAutoDismissTimeout()
{
    if (m_state == EntityState::Engaged) {
        m_moodSystem.onDismissed();
        hideNow();
    }
}

void RuntimeDirector::onFleeHideTimeout()
{
    hideNow();
}

void RuntimeDirector::onCommentaryStartTimeout()
{
    if (!m_commentaryPending) {
        syncAutoScreenCommentaryTimer();
        return;
    }
    m_commentaryPending = false;
    m_commentaryActive = true;
    m_moodSystem.onEngaged();
    transitionTo(EntityState::Engaged, QStringLiteral("屏幕评论中"), QStringLiteral("准备请求屏幕评论服务"));
    if (!m_commentaryPreambleText.isEmpty()) {
        Q_EMIT speechRequestRequested(AudioPlaybackRequest{
            m_commentaryPreambleText,
            static_cast<int>(AudioPlaybackPriority::High),
            true,
            QString(),
        });
    }
    Q_EMIT screenCommentaryRequested();
    syncAutoScreenCommentaryTimer();
}

void RuntimeDirector::onAutoScreenCommentaryTimeout()
{
    syncAutoScreenCommentaryTimer();
    if (!m_screenCommentaryAutoEnabled || commentaryInFlight()) {
        return;
    }
    if (m_dndMode) {
        Q_EMIT statusTextChanged(QStringLiteral("自动屏幕评论"), QStringLiteral("请勿打扰已开启，已跳过本轮自动评论"));
        return;
    }
    if (m_fullScreenPauseEnabled && m_fullscreenActive) {
        Q_EMIT statusTextChanged(QStringLiteral("自动屏幕评论"), QStringLiteral("检测到全屏应用，已跳过本轮自动评论"));
        return;
    }
    requestScreenCommentary();
}

void RuntimeDirector::onPeriodicCameraScanTimeout()
{
    startPeriodicCameraScan(QStringLiteral("timer"), false);
}

void RuntimeDirector::onPeriodicCameraScanCollectTimeout()
{
    if (!m_periodicScanActive) {
        syncPeriodicScanTimer();
        return;
    }

    const QVector<GazeSample> samples = m_periodicScanSamples;
    const bool debugMode = m_periodicScanDebugMode;
    m_periodicScanActive = false;
    m_periodicScanDebugMode = false;
    m_periodicScanSamples.clear();

    if (samples.isEmpty()) {
        Q_EMIT statusTextChanged(
            debugMode ? QStringLiteral("调试摄像头巡检") : QStringLiteral("摄像头巡检"),
            QStringLiteral("本轮未采集到有效样本"));
        Q_EMIT periodicCameraScanCompleted(debugMode);
        syncPeriodicScanTimer();
        return;
    }

    int faceCount = 0;
    int presentCount = 0;
    QHash<QString, double> emotionVotes;
    for (const GazeSample &sample : samples) {
        if (sample.faceDetected) {
            ++faceCount;
            const QString label = (sample.emotionLabel.trimmed().isEmpty()
                    ? QStringLiteral("neutral")
                    : sample.emotionLabel.trimmed().toLower());
            emotionVotes[label] += qMax(0.1, sample.emotionScore);
        }

        const PresenceState presence = m_presenceDetector.determinePresence(m_latestIdleTimeMs, sample);
        if (presence == PresenceState::PresentActive || presence == PresenceState::PresentPassive) {
            ++presentCount;
        }
    }

    const double sampleCount = static_cast<double>(samples.size());
    const double faceRatio = sampleCount > 0.0 ? static_cast<double>(faceCount) / sampleCount : 0.0;
    const double presenceRatio = sampleCount > 0.0 ? static_cast<double>(presentCount) / sampleCount : 0.0;
    const bool facePresent = faceRatio >= kPeriodicCameraMinFaceRatio || presenceRatio >= kPeriodicCameraMinPresenceRatio;
    QString dominantEmotion = QStringLiteral("neutral");
    double dominantEmotionScore = -1.0;
    for (auto it = emotionVotes.cbegin(); it != emotionVotes.cend(); ++it) {
        if (it.value() > dominantEmotionScore) {
            dominantEmotionScore = it.value();
            dominantEmotion = it.key();
        }
    }

    Q_EMIT statusTextChanged(
        debugMode ? QStringLiteral("调试摄像头巡检") : QStringLiteral("摄像头巡检"),
        facePresent
            ? QStringLiteral("检测到用户存在，emotion=%1 face_ratio=%2 presence_ratio=%3")
                  .arg(dominantEmotion)
                  .arg(faceRatio, 0, 'f', 2)
                  .arg(presenceRatio, 0, 'f', 2)
            : QStringLiteral("未检测到稳定存在，face_ratio=%1 presence_ratio=%2")
                  .arg(faceRatio, 0, 'f', 2)
                  .arg(presenceRatio, 0, 'f', 2));

    if (!debugMode) {
        applyPeriodicScanVisual(facePresent, dominantEmotion);
    }

    Q_EMIT periodicCameraScanCompleted(debugMode);
    syncPeriodicScanTimer();
}

void RuntimeDirector::stopTimers()
{
    m_idlePeekTimer->stop();
    m_peekAdvanceTimer->stop();
    m_autoDismissTimer->stop();
    m_fleeHideTimer->stop();
    m_commentaryStartTimer->stop();
    m_periodicScanCollectTimer->stop();
    m_prolongedIdleTimer->stop();
}

void RuntimeDirector::armAutoDismiss(int durationMs)
{
    if (residentActive()) {
        m_autoDismissTimer->stop();
        return;
    }
    m_autoDismissTimer->start(durationMs);
}

void RuntimeDirector::completeCommentarySession()
{
    m_commentaryPending = false;
    m_commentaryActive = false;
    syncAutoScreenCommentaryTimer();
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

void RuntimeDirector::setCameraEnabled(bool enabled)
{
    m_cameraEnabled = enabled;
    if (!m_cameraEnabled) {
        resetExpressionTracking();
        resetNoFaceTracker();
        m_passivePresenceTimer->stop();
        m_passivePresenceActive = false;
        m_deepSleepActive = false;
        m_presenceState = PresenceState::Unknown;
        m_cameraRunning = false;
        m_periodicScanTimer->stop();
        m_periodicScanCollectTimer->stop();
        m_periodicScanActive = false;
        m_periodicScanDebugMode = false;
        m_periodicScanSamples.clear();
    }
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

void RuntimeDirector::setPresenceTargetFps(int targetFps)
{
    m_presenceDetector.setTargetFps(targetFps);
}

void RuntimeDirector::setEyeTrackingEnabled(bool enabled)
{
    m_eyeTrackingEnabled = enabled;
}

void RuntimeDirector::setPeriodicScanEnabled(bool enabled)
{
    m_periodicScanEnabled = enabled;
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

void RuntimeDirector::setPeriodicScanIntervalMinutes(int minutes)
{
    m_periodicScanIntervalMinutes = qBound(5, minutes, 240);
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

void RuntimeDirector::emitIdleSpeechRequest()
{
    if (m_behaviorMode == BehaviorMode::Summoning || commentaryInFlight()) {
        return;
    }

    const VoiceScriptEntry script = m_voiceScriptCatalog.selectIdleScript(QDateTime::currentDateTime());
    if (script.text.trimmed().isEmpty()) {
        return;
    }

    Q_EMIT voiceVisualRequested(script.spritePath.trimmed(), script.animSpeed.trimmed());
    Q_EMIT speechRequestRequested(AudioPlaybackRequest{
        script.text.trimmed(),
        static_cast<int>(AudioPlaybackPriority::High),
        false,
        script.audioPath.trimmed(),
    });
}

void RuntimeDirector::emitPanicSpeechRequest()
{
    const VoiceScriptEntry script = m_voiceScriptCatalog.selectPanicScript(QDateTime::currentDateTime());
    if (script.text.trimmed().isEmpty()) {
        return;
    }

    Q_EMIT voiceVisualRequested(script.spritePath.trimmed(), script.animSpeed.trimmed());
    Q_EMIT speechRequestRequested(AudioPlaybackRequest{
        script.text.trimmed(),
        static_cast<int>(AudioPlaybackPriority::Critical),
        true,
        script.audioPath.trimmed(),
    });
}

void RuntimeDirector::syncResidentVisibility()
{
    if (!m_residentModeEnabled || m_dndMode || m_scriptedEntranceActive || commentaryInFlight()) {
        return;
    }

    const bool fullscreenBlocking = m_fullScreenPauseEnabled && m_fullscreenActive;
    if (fullscreenBlocking) {
        stopTimers();
        if (m_state == EntityState::Peeking || m_state == EntityState::Engaged) {
            setBehaviorMode(BehaviorMode::Busy);
            transitionTo(EntityState::Hidden, QStringLiteral("全屏暂停"), QStringLiteral("检测到全屏应用，角色已隐藏"));
        }
        return;
    }

    if (m_state == EntityState::Hidden) {
        stopTimers();
        setBehaviorMode(BehaviorMode::Idle);
        transitionTo(EntityState::Engaged, QStringLiteral("常驻模式"), QStringLiteral("角色保持常驻显示"));
        return;
    }

    if (m_state == EntityState::Peeking || m_state == EntityState::Engaged) {
        stopTimers();
        setBehaviorMode(BehaviorMode::Idle);
        Q_EMIT statusTextChanged(QStringLiteral("常驻模式"), QStringLiteral("角色保持可见"));
        return;
    }

    if (m_state == EntityState::Fleeing) {
        stopTimers();
        setBehaviorMode(BehaviorMode::Idle);
        transitionTo(EntityState::Engaged, QStringLiteral("常驻模式"), QStringLiteral("已取消撤退，角色继续常驻显示"));
    }
}

void RuntimeDirector::syncAutoScreenCommentaryTimer()
{
    m_autoScreenCommentaryTimer->stop();
    if (!m_screenCommentaryAutoEnabled || m_offlineMode) {
        return;
    }
    if (commentaryInFlight()) {
        return;
    }
    m_autoScreenCommentaryTimer->start(m_screenCommentaryAutoIntervalMs);
}

void RuntimeDirector::syncPeriodicScanTimer()
{
    m_periodicScanTimer->stop();
    if (m_periodicScanActive) {
        return;
    }
    if (!m_periodicScanEnabled || !m_cameraEnabled || !m_cameraRunning) {
        return;
    }
    if (commentaryInFlight() || m_scriptedEntranceActive || m_state == EntityState::Fleeing) {
        return;
    }
    m_periodicScanTimer->start(m_periodicScanIntervalMinutes * 60 * 1000);
}

void RuntimeDirector::syncProlongedIdleTimer()
{
    m_prolongedIdleTimer->stop();
    if (m_state != EntityState::Hidden) {
        return;
    }
    if (m_idleInvasionEnabled || m_dndMode) {
        return;
    }
    if (residentActive() || commentaryInFlight() || m_scriptedEntranceActive) {
        return;
    }
    if (m_fullScreenPauseEnabled && m_fullscreenActive) {
        return;
    }
    m_prolongedIdleTimer->start(kProlongedIdlePulseMs);
}

bool RuntimeDirector::startPeriodicCameraScan(const QString &source, bool debugMode)
{
    const QString normalizedSource = source.trimmed().isEmpty() ? QStringLiteral("timer") : source.trimmed().toLower();
    if (m_periodicScanActive) {
        Q_EMIT statusTextChanged(
            debugMode ? QStringLiteral("调试摄像头巡检") : QStringLiteral("摄像头巡检"),
            QStringLiteral("已跳过：巡检已在进行中 source=%1").arg(normalizedSource));
        return false;
    }
    if (!debugMode && !m_periodicScanEnabled) {
        return false;
    }
    if (!m_cameraEnabled) {
        Q_EMIT statusTextChanged(
            debugMode ? QStringLiteral("调试摄像头巡检") : QStringLiteral("摄像头巡检"),
            QStringLiteral("已跳过：摄像头未启用或尚未运行 source=%1").arg(normalizedSource));
        syncPeriodicScanTimer();
        return false;
    }
    if (!m_cameraRunning) {
        m_periodicScanPendingStart = true;
        m_periodicScanPendingDebugMode = debugMode;
        Q_EMIT visionRuntimeRequested(debugMode);
        Q_EMIT statusTextChanged(
            debugMode ? QStringLiteral("调试摄像头巡检") : QStringLiteral("摄像头巡检"),
            QStringLiteral("正在启动摄像头并准备巡检 source=%1").arg(normalizedSource));
        return true;
    }
    if (!debugMode && m_dndMode) {
        Q_EMIT statusTextChanged(QStringLiteral("摄像头巡检"), QStringLiteral("请勿打扰已开启，已跳过本轮巡检"));
        syncPeriodicScanTimer();
        return false;
    }
    if (!debugMode && m_fullScreenPauseEnabled && m_fullscreenActive) {
        Q_EMIT statusTextChanged(QStringLiteral("摄像头巡检"), QStringLiteral("检测到全屏应用，已跳过本轮巡检"));
        syncPeriodicScanTimer();
        return false;
    }
    beginPeriodicCameraScan(normalizedSource, debugMode);
    return true;
}

void RuntimeDirector::beginPeriodicCameraScan(const QString &source, bool debugMode)
{
    const QString normalizedSource = source.trimmed().isEmpty() ? QStringLiteral("timer") : source.trimmed().toLower();
    m_periodicScanTimer->stop();
    m_periodicScanActive = true;
    m_periodicScanDebugMode = debugMode;
    m_periodicScanSamples.clear();
    m_periodicScanCollectTimer->start(kPeriodicCameraScanDurationMs);
    Q_EMIT statusTextChanged(
        debugMode ? QStringLiteral("调试摄像头巡检") : QStringLiteral("摄像头巡检中"),
        QStringLiteral("正在采集视觉样本 source=%1").arg(normalizedSource));
}

void RuntimeDirector::applyPeriodicScanVisual(bool facePresent, const QString &emotionLabel)
{
    if (facePresent && m_state == EntityState::Hidden) {
        stopTimers();
        m_passivePresenceActive = false;
        m_deepSleepActive = false;
        setBehaviorMode(BehaviorMode::Idle);
        transitionTo(EntityState::Engaged, QStringLiteral("巡检发现用户"), QStringLiteral("检测到用户存在，角色已显现"));
        armAutoDismiss(m_autoDismissMs);
    }

    if (m_state != EntityState::Peeking && m_state != EntityState::Engaged) {
        return;
    }

    if (!facePresent) {
        Q_EMIT entityStateOverrideRequested(QStringLiteral("state5"));
        return;
    }

    setBehaviorMode(BehaviorMode::Idle);
    Q_EMIT entityStateOverrideRequested(expressionStateForLabel(emotionLabel));
}

void RuntimeDirector::refreshPresenceState(const GazeSample *sample)
{
    const PresenceState state = m_presenceDetector.determinePresence(m_latestIdleTimeMs, sample);
    if (state == m_presenceState) {
        return;
    }

    m_presenceState = state;
    applyPresenceState(state);
}

void RuntimeDirector::trackExpressionState(const GazeSample &sample)
{
    if (m_state != EntityState::Peeking && m_state != EntityState::Engaged) {
        return;
    }
    if (m_behaviorMode != BehaviorMode::Idle) {
        return;
    }

    QString label = sample.faceDetected ? sample.emotionLabel.trimmed().toLower() : QStringLiteral("neutral");
    if (!m_expressionVotes.contains(label)) {
        label = QStringLiteral("neutral");
    }
    const int weight = sample.emotionScore >= 0.55 ? 2 : 1;

    for (auto it = m_expressionVotes.begin(); it != m_expressionVotes.end(); ++it) {
        it.value() = qMax(0, it.value() - 1);
    }
    m_expressionVotes[label] = qMin(8, m_expressionVotes.value(label) + weight);

    QString winner = QStringLiteral("neutral");
    int winnerScore = -1;
    for (auto it = m_expressionVotes.cbegin(); it != m_expressionVotes.cend(); ++it) {
        if (it.value() > winnerScore) {
            winner = it.key();
            winnerScore = it.value();
        }
    }
    if (winnerScore < 3) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (winner == m_stableExpression && (nowMs - m_lastExpressionVisualAtMs) < kExpressionVisualIntervalMs) {
        return;
    }

    m_stableExpression = winner;
    m_lastExpressionVisualAtMs = nowMs;
    if (winner == QStringLiteral("happy")) {
        m_moodSystem.applyDelta(0.05);
    } else if (winner == QStringLiteral("angry")) {
        m_moodSystem.applyDelta(-0.05);
    } else if (winner == QStringLiteral("sad")) {
        m_moodSystem.applyDelta(-0.03);
    }
    Q_EMIT entityStateOverrideRequested(expressionStateForLabel(winner));
}

void RuntimeDirector::resetExpressionTracking()
{
    for (auto it = m_expressionVotes.begin(); it != m_expressionVotes.end(); ++it) {
        it.value() = 0;
    }
    m_stableExpression = QStringLiteral("neutral");
    m_lastExpressionVisualAtMs = 0;
}

void RuntimeDirector::maybeTriggerSadComfort(const GazeSample &sample)
{
    if (!m_cameraEnabled || !sample.faceDetected) {
        return;
    }
    if (m_state != EntityState::Peeking && m_state != EntityState::Engaged) {
        return;
    }
    if (m_scriptedEntranceActive || commentaryInFlight() || m_state == EntityState::Fleeing) {
        return;
    }
    if (sample.emotionLabel.trimmed().compare(QStringLiteral("sad"), Qt::CaseInsensitive) != 0) {
        return;
    }
    if (sample.emotionScore < kSadComfortMinScore) {
        return;
    }
    if (m_stableExpression != QStringLiteral("sad")) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_lastSadComfortAtMs > 0 && (nowMs - m_lastSadComfortAtMs) < kSadComfortCooldownMs) {
        return;
    }
    m_lastSadComfortAtMs = nowMs;
    Q_EMIT statusTextChanged(QStringLiteral("情绪安慰"), QStringLiteral("检测到 sad 表情，已触发安慰语音"));
    emitSadComfortSpeech();
}

void RuntimeDirector::emitSadComfortSpeech()
{
    Q_EMIT entityStateOverrideRequested(QStringLiteral("state5"));
    Q_EMIT speechRequestRequested(AudioPlaybackRequest{
        QStringLiteral("别太难过，我在这里陪你。"),
        static_cast<int>(AudioPlaybackPriority::High),
        false,
        QString(),
    });
}

QStringList RuntimeDirector::splitCommentarySpeech(const QString &text) const
{
    const QString cleaned = text.simplified().trimmed();
    if (cleaned.isEmpty()) {
        return {};
    }
    if (!m_commentaryStreamingEnabled) {
        return { cleaned };
    }

    const int targetChars = qBound(8, m_commentaryStreamChunkChars, 80);
    const QString punctuation = QStringLiteral("。！？；.!?;,，、\n");
    QStringList chunks;
    QString current;
    current.reserve(targetChars + 8);

    for (const QChar ch : cleaned) {
        current.append(ch);
        const bool punctuationHit = punctuation.contains(ch);
        if (current.size() >= targetChars || (punctuationHit && current.size() >= (targetChars / 2))) {
            const QString chunk = current.trimmed();
            if (!chunk.isEmpty()) {
                chunks.append(chunk);
            }
            current.clear();
        }
    }

    const QString tail = current.trimmed();
    if (!tail.isEmpty()) {
        chunks.append(tail);
    }

    if (chunks.isEmpty()) {
        chunks.append(cleaned);
    }
    return chunks;
}

void RuntimeDirector::resetNoFaceTracker()
{
    m_noFaceAbsentSinceMs = -1;
    m_userLeftAtMs = -1;
    m_noFaceStreakTriggered = false;
}

void RuntimeDirector::maybeTriggerNoFaceReturn(const GazeSample &sample)
{
    if (!m_cameraEnabled) {
        resetNoFaceTracker();
        return;
    }

    if (sample.faceDetected) {
        maybeGreetUserReturn();
        return;
    }

    if (m_state != EntityState::Peeking && m_state != EntityState::Engaged) {
        resetNoFaceTracker();
        return;
    }
    if (m_scriptedEntranceActive || commentaryInFlight() || m_state == EntityState::Fleeing) {
        return;
    }
    if (m_userLeftAtMs >= 0) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_noFaceAbsentSinceMs < 0) {
        m_noFaceAbsentSinceMs = nowMs;
        return;
    }
    if ((nowMs - m_noFaceAbsentSinceMs) < m_noFaceMinAbsenceMs) {
        return;
    }

    m_userLeftAtMs = m_noFaceAbsentSinceMs;
    m_noFaceStreakTriggered = true;
    Q_EMIT statusTextChanged(QStringLiteral("无人脸离开"), QStringLiteral("已标记用户暂时离开"));
}

void RuntimeDirector::maybeGreetUserReturn()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 userLeftAtMs = m_userLeftAtMs;
    const bool hadMarkedAbsence = m_noFaceStreakTriggered;
    resetNoFaceTracker();

    if (!hadMarkedAbsence || userLeftAtMs < 0) {
        return;
    }

    const qint64 awayMs = qMax<qint64>(0, nowMs - userLeftAtMs);
    if (awayMs < m_userReturnMinSpeakMs) {
        return;
    }
    if (m_lastNoFaceGreetingAtMs > 0 && (nowMs - m_lastNoFaceGreetingAtMs) < m_noFaceCooldownMs) {
        return;
    }

    m_lastNoFaceGreetingAtMs = nowMs;
    emitUserReturnSpeech(awayMs);
}

void RuntimeDirector::emitUserReturnSpeech(qint64 awayMs)
{
    QString text;
    if (awayMs < m_userReturnMediumThresholdMs) {
        const int index = QRandomGenerator::global()->bounded(static_cast<int>(kUserReturnShortTexts.size()));
        text = QString::fromUtf8(kUserReturnShortTexts.at(index));
    } else if (awayMs < m_userReturnLongThresholdMs) {
        const int minutes = qMax(1, static_cast<int>(awayMs / 60000));
        const int index = QRandomGenerator::global()->bounded(static_cast<int>(kUserReturnMediumTexts.size()));
        text = QString::fromUtf8(kUserReturnMediumTexts.at(index)).arg(minutes);
    } else {
        const int index = QRandomGenerator::global()->bounded(static_cast<int>(kUserReturnLongTexts.size()));
        text = QString::fromUtf8(kUserReturnLongTexts.at(index));
    }

    Q_EMIT statusTextChanged(QStringLiteral("用户回来了"), text);
    if (m_state == EntityState::Peeking || m_state == EntityState::Engaged) {
        Q_EMIT entityStateOverrideRequested(QStringLiteral("state6"));
    }
    Q_EMIT speechRequestRequested(AudioPlaybackRequest{
        text,
        static_cast<int>(AudioPlaybackPriority::High),
        false,
        QString(),
    });
}

void RuntimeDirector::applyPresenceState(PresenceState state)
{
    if (state == PresenceState::Unknown) {
        return;
    }

    if (state == PresenceState::PresentActive) {
        onPresenceBackActive();
        return;
    }

    if (m_scriptedEntranceActive || commentaryInFlight() || m_state == EntityState::Fleeing) {
        return;
    }

    if (state == PresenceState::PresentPassive) {
        enterPassiveCompanion();
        return;
    }

    if (state == PresenceState::Absent) {
        enterDeepSleep();
    }
}

void RuntimeDirector::enterPassiveCompanion()
{
    EntityState currentState = m_state;
    if (currentState == EntityState::Hidden) {
        stopTimers();
        m_deepSleepActive = false;
        setBehaviorMode(BehaviorMode::Idle);
        transitionTo(EntityState::Engaged, QStringLiteral("被动陪伴"), QStringLiteral("检测到静止用户，角色静默陪伴中"));
        currentState = m_state;
    }

    if (currentState != EntityState::Peeking && currentState != EntityState::Engaged) {
        return;
    }
    if (m_behaviorMode != BehaviorMode::Idle) {
        return;
    }

    m_deepSleepActive = false;
    m_passivePresenceActive = true;
    m_autoDismissTimer->stop();
    Q_EMIT entityStateOverrideRequested(QStringLiteral("state5"));
    m_passivePresenceTimer->start(kPassiveCompanionDurationMs);
}

void RuntimeDirector::enterDeepSleep()
{
    if (m_state != EntityState::Peeking && m_state != EntityState::Engaged) {
        return;
    }

    m_passivePresenceActive = false;
    m_passivePresenceTimer->stop();
    m_deepSleepActive = true;
    m_autoDismissTimer->stop();
    setBehaviorMode(BehaviorMode::Busy);
    Q_EMIT entityStateOverrideRequested(QStringLiteral("state5"));
}

void RuntimeDirector::onPresenceBackActive()
{
    const bool wasDeepSleep = m_deepSleepActive;
    m_deepSleepActive = false;
    if (m_passivePresenceActive) {
        m_passivePresenceActive = false;
        m_passivePresenceTimer->stop();
    }

    if (m_state != EntityState::Peeking && m_state != EntityState::Engaged) {
        return;
    }
    if (m_behaviorMode != BehaviorMode::Idle) {
        return;
    }

    if (wasDeepSleep) {
        Q_EMIT entityStateOverrideRequested(QStringLiteral("state2"));
        if (!residentActive()) {
            armAutoDismiss(m_autoDismissMs);
        }
    }
}

void RuntimeDirector::onPassivePresenceTimeout()
{
    if (!m_passivePresenceActive) {
        return;
    }

    m_passivePresenceActive = false;
    if (residentActive()) {
        syncResidentVisibility();
        return;
    }

    if (m_state == EntityState::Peeking || m_state == EntityState::Engaged) {
        transitionTo(EntityState::Hidden, QStringLiteral("被动陪伴结束"), QStringLiteral("长时间静止已结束，角色已隐藏"));
    }
}

void RuntimeDirector::onProlongedIdleTimeout()
{
    syncProlongedIdleTimer();
    if (m_state != EntityState::Hidden || m_idleInvasionEnabled || m_dndMode) {
        return;
    }
    if (residentActive() || commentaryInFlight() || m_scriptedEntranceActive) {
        return;
    }
    if (m_fullScreenPauseEnabled && m_fullscreenActive) {
        return;
    }

    Q_EMIT statusTextChanged(QStringLiteral("长时间空闲"), QStringLiteral("角色在屏幕边缘轻微探头，维持存在感"));
    Q_EMIT prolongedIdlePulseRequested();
    syncProlongedIdleTimer();
}

bool RuntimeDirector::residentActive() const
{
    return m_residentModeEnabled && !m_dndMode && !(m_fullScreenPauseEnabled && m_fullscreenActive);
}

QString RuntimeDirector::resolveScriptedTrajectoryPath() const
{
    const QString defaultFileName = QFileInfo(QString::fromLatin1(kDefaultTrajectoryFile)).fileName();
    const QString envResolved = ScriptedTrajectoryCatalog::resolvePreferredTrajectory(
        qEnvironmentVariable("CYBERCOMPANION_TRAJECTORY_PATH"),
        defaultFileName);
    if (!envResolved.isEmpty()) {
        return envResolved;
    }

    const QString configuredResolved = ScriptedTrajectoryCatalog::resolvePreferredTrajectory(
        m_scriptedTrajectoryPath,
        defaultFileName);
    if (!configuredResolved.isEmpty()) {
        return configuredResolved;
    }

    const QString scannedDefaultDirectory = ScriptedTrajectoryCatalog::scanDefaultDirectory(
        QString::fromLatin1(kDefaultTrajectoryDirectory),
        defaultFileName);
    if (!scannedDefaultDirectory.isEmpty()) {
        return scannedDefaultDirectory;
    }

    return ScriptedTrajectoryCatalog::resolvePreferredTrajectory(
        QString::fromLatin1(kDefaultTrajectoryFile),
        defaultFileName);
}

void RuntimeDirector::setBehaviorMode(BehaviorMode mode)
{
    if (m_behaviorMode == mode) {
        return;
    }
    m_behaviorMode = mode;
    Q_EMIT behaviorModeChanged(mode);
}

void RuntimeDirector::transitionTo(EntityState state, const QString &title, const QString &detail)
{
    m_state = state;
    Q_EMIT stateChanged(state);
    Q_EMIT statusTextChanged(title, detail);
    syncPeriodicScanTimer();
    syncProlongedIdleTimer();
}

int RuntimeDirector::nextIdleIntervalMs() const
{
    return QRandomGenerator::global()->bounded(7000, 12001);
}
