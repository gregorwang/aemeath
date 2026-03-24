#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QObject>
#include <QString>
#include <QTimer>

#include "services/service_contracts.h"

struct AudioOutputMonitorOptions
{
    int pollIntervalMs = 500;
    bool ignoreCurrentProcessAudio = true;
    bool includeMasterPeakFallback = false;
    bool preferMediaSessions = true;
    int silenceDebounceCount = 5;
    bool useWorkerThread = true;
};

struct AudioOutputPollResult
{
    bool currentlyPlaying = false;
    QString matchedSummary;
    bool hasAllActive = false;
    QString allSummary;
};

Q_DECLARE_METATYPE(AudioOutputPollResult)

class AudioOutputPollBackend
{
public:
    virtual ~AudioOutputPollBackend() = default;

    virtual bool isAvailable() const = 0;
    virtual QString backendName() const = 0;
    virtual AudioOutputPollResult poll(
        const AudioOutputMonitorOptions &options,
        std::optional<quint32> ignorePid) = 0;
};

using AudioOutputPollBackendFactory = std::function<std::shared_ptr<AudioOutputPollBackend>()>;

class AudioOutputPollWorker final : public QObject
{
    Q_OBJECT

public:
    AudioOutputPollWorker(
        std::shared_ptr<AudioOutputPollBackend> backend,
        AudioOutputMonitorOptions options,
        QObject *parent = nullptr);

public Q_SLOTS:
    void poll(quint32 ignorePid, bool hasIgnorePid);

Q_SIGNALS:
    void pollFinished(const AudioOutputPollResult &result);

private:
    std::shared_ptr<AudioOutputPollBackend> m_backend;
    AudioOutputMonitorOptions m_options;
};

class QThread;

class WindowsAudioOutputMonitorService final : public AudioOutputMonitorService
{
    Q_OBJECT

public:
    explicit WindowsAudioOutputMonitorService(QObject *parent = nullptr);
    explicit WindowsAudioOutputMonitorService(
        AudioOutputMonitorOptions options,
        QObject *parent = nullptr);
    WindowsAudioOutputMonitorService(
        AudioOutputPollBackendFactory backendFactory,
        AudioOutputMonitorOptions options,
        QObject *parent = nullptr);
    ~WindowsAudioOutputMonitorService() override;

    bool isPlaying() const override;
    bool isAvailable() const override;

public Q_SLOTS:
    void start() override;
    void stop() override;

private Q_SLOTS:
    void pollAsync();
    void onPollFinished(const AudioOutputPollResult &result);

Q_SIGNALS:
    void pollRequested(quint32 ignorePid, bool hasIgnorePid);

private:
    void ensureWorker();
    void shutdownWorker();
    void pollSync();
    void applyPollResult(const AudioOutputPollResult &result);

    AudioOutputMonitorOptions m_options;
    AudioOutputPollBackendFactory m_backendFactory;
    std::shared_ptr<AudioOutputPollBackend> m_syncBackend;
    QTimer m_timer;
    QThread *m_workerThread = nullptr;
    AudioOutputPollWorker *m_worker = nullptr;
    std::optional<quint32> m_currentPid;
    QString m_lastSessionSummary = QStringLiteral("-");
    qint64 m_lastRejectedLogMs = 0;
    bool m_running = false;
    bool m_isPlaying = false;
    bool m_pollInflight = false;
    int m_silenceCounter = 0;
    bool m_loggedUnavailable = false;
};
