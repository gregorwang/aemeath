#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

enum class AudioPlaybackPriority {
    Critical = 0,
    High = 1,
    Normal = 2,
    Low = 3,
};

struct AudioPlaybackRequest
{
    QString text;
    int priority = static_cast<int>(AudioPlaybackPriority::Normal);
    bool interrupt = false;
    QString audioFilePath;
};

Q_DECLARE_METATYPE(AudioPlaybackRequest)

struct GazeSample
{
    bool faceDetected = false;
    double faceX = 0.0;
    double faceY = 0.0;
    double confidence = 0.0;
    QString emotionLabel = QStringLiteral("unknown");
    double emotionScore = 0.0;
    double brightness = 0.0;
    double motionScore = 0.0;
};

Q_DECLARE_METATYPE(GazeSample)

struct VoiceInputConfig
{
    bool microphoneEnabled = false;
    bool offlineMode = false;
    QString voiceInputMode = QStringLiteral("push_to_talk");
    QString asrProvider = QStringLiteral("zhipu_asr");
    QString asrApiKey;
    QString asrModel = QStringLiteral("glm-asr-2512");
    QString asrBaseUrl = QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions");
    double asrTemperature = 0.0;
    QString asrPrompt;
    bool wakeupEnabled = false;
    QStringList wakeupPhrases;
    QString wakeupLanguage = QStringLiteral("zh-CN");
    QString llmApiKey;
    QString llmBaseUrl = QStringLiteral("https://api.openai.com/v1");
};

Q_DECLARE_METATYPE(VoiceInputConfig)

class AudioService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~AudioService() override = default;

public Q_SLOTS:
    virtual void speak(const QString &text) = 0;
    virtual void speakRequest(const AudioPlaybackRequest &request) = 0;
    virtual void setTtsProvider(const QString &provider) = 0;
    virtual void configureVoice(const QString &voice, const QString &rate) = 0;
    virtual void setVolume(double volume) = 0;
    virtual void setCacheEnabled(bool enabled) = 0;
    virtual void interrupt() = 0;
    virtual void shutdown() = 0;

Q_SIGNALS:
    void playbackStarted(const QString &text);
    void playbackFinished();
    void playbackWarning(const QString &message);
};

class VisionService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~VisionService() override = default;

public Q_SLOTS:
    virtual void configureCamera(int cameraIndex, int targetFps) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void gazeUpdated(const GazeSample &sample);
    void cameraError(const QString &message);
    void cameraStateChanged(bool running);

    void faceDetected(bool detected);
};

class AudioOutputMonitorService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~AudioOutputMonitorService() override = default;

    virtual bool isPlaying() const = 0;
    virtual bool isAvailable() const = 0;

public Q_SLOTS:
    virtual void start() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void audioOutputStarted();
    void audioOutputStopped();
    void audioStateChanged(bool playing);
};

class HotkeyService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~HotkeyService() override = default;

    virtual bool isSummonRegistered() const = 0;
    virtual bool isPushToTalkRegistered() const = 0;

public Q_SLOTS:
    virtual void start() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void summonRequested();
    void pushToTalkRequested();
};

class IdleMonitorService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~IdleMonitorService() override = default;

public Q_SLOTS:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void setThresholdMs(int thresholdMs) = 0;
    virtual void resetToStandby() = 0;

Q_SIGNALS:
    void idleDetected();
    void activityDetected();
    void idleTimeUpdated(qint64 idleMs);
};

class ScreenCommentaryService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~ScreenCommentaryService() override = default;

public Q_SLOTS:
    virtual void requestCommentary() = 0;
    virtual void cancel() = 0;

Q_SIGNALS:
    void commentaryReady(const QString &text);
    void commentaryFailed(const QString &error);
};

class VoiceInputService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~VoiceInputService() override = default;

public Q_SLOTS:
    virtual void configure(const VoiceInputConfig &config) = 0;
    virtual void startContinuousListening() = 0;
    virtual void startPushToTalkOnce() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void transcriptReady(const QString &text, const QString &source);
    void listenerError(const QString &message);
    void listenerWarning(const QString &message);
    void continuousListeningDegraded(const QString &message);
    void listenerStateChanged(bool running);
    void captureStateChanged(bool active, const QString &source);
};

class FullscreenMonitorService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~FullscreenMonitorService() override = default;

    virtual bool isFullscreenActive() const = 0;

public Q_SLOTS:
    virtual void start() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void fullscreenChanged(bool fullscreenActive);
};
