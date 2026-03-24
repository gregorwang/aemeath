#include "services/audio_output_monitor_service.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QStringList>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <wrl/client.h>
#endif

namespace {

#if defined(_MSC_VER)
constexpr bool kHasAudioMeterInformation = true;
#else
constexpr bool kHasAudioMeterInformation = false;
#endif

using QStringListLiterals = std::initializer_list<const char *>;

constexpr double kSessionPeakThreshold = 0.001;
constexpr double kMasterPeakThreshold = 0.005;
constexpr int kMinimumPollIntervalMs = 100;
constexpr qint64 kRejectedLogIntervalMs = 5000;

const QStringListLiterals kMediaProcessKeywords = {
    "chrome",
    "msedge",
    "firefox",
    "vlc",
    "potplayer",
    "mpv",
    "spotify",
    "qqmusic",
    "cloudmusic",
    "music",
    "wmplayer",
    "kodi",
    "bilibili",
    "youku",
};

const QStringListLiterals kNonMediaProcessKeywords = {
    "cybercompanion",
    "cybercompanioncpp",
    "python",
    "pythonw",
    "ffmpeg",
    "audiodg",
    "svchost",
};

struct ActiveAudioSession
{
    std::optional<quint32> pid;
    QString processName;
    QString displayName;
    double peak = 0.0;
};

QString buildSessionSummary(const std::vector<ActiveAudioSession> &sessions, int limit = 3)
{
    if (sessions.empty()) {
        return QStringLiteral("-");
    }

    QStringList parts;
    const int clampedLimit = std::max(1, limit);
    for (int index = 0; index < static_cast<int>(sessions.size()) && index < clampedLimit; ++index) {
        const ActiveAudioSession &item = sessions.at(index);
        QString label = !item.processName.isEmpty() ? item.processName : item.displayName;
        if (label.isEmpty()) {
            label = QStringLiteral("unknown");
        }
        if (item.pid.has_value()) {
            label += QStringLiteral("(pid=%1)").arg(*item.pid);
        }
        parts << QStringLiteral("%1:%2").arg(label, QString::number(item.peak, 'f', 3));
    }
    return parts.join(QStringLiteral(", "));
}

bool looksLikeMediaSession(const ActiveAudioSession &session)
{
    const QString blob = QStringLiteral("%1 %2")
                             .arg(session.processName, session.displayName)
                             .trimmed()
                             .toLower();
    if (blob.isEmpty()) {
        return false;
    }
    for (const char *keyword : kNonMediaProcessKeywords) {
        if (blob.contains(QLatin1String(keyword))) {
            return false;
        }
    }
    for (const char *keyword : kMediaProcessKeywords) {
        if (blob.contains(QLatin1String(keyword))) {
            return true;
        }
    }
    return false;
}

#ifdef Q_OS_WIN
using Microsoft::WRL::ComPtr;

class ScopedComInitialization
{
public:
    ScopedComInitialization()
        : m_hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
    }

    ~ScopedComInitialization()
    {
        if (m_hr == S_OK || m_hr == S_FALSE) {
            CoUninitialize();
        }
    }

    bool isUsable() const
    {
        return SUCCEEDED(m_hr) || m_hr == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT m_hr;
};

QString lowerFileNameFromPath(const QString &path)
{
    return QFileInfo(path).fileName().trimmed().toLower();
}

QString processNameForPid(quint32 pid)
{
    if (pid == 0) {
        return {};
    }

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (processHandle == nullptr) {
        return {};
    }

    wchar_t buffer[MAX_PATH];
    DWORD bufferSize = MAX_PATH;
    QString processName;
    if (QueryFullProcessImageNameW(processHandle, 0, buffer, &bufferSize)) {
        processName = lowerFileNameFromPath(QString::fromWCharArray(buffer, static_cast<int>(bufferSize)));
    }
    CloseHandle(processHandle);
    return processName;
}

QString sessionDisplayName(IAudioSessionControl *control)
{
    if (control == nullptr) {
        return {};
    }

    LPWSTR displayName = nullptr;
    const HRESULT hr = control->GetDisplayName(&displayName);
    if (FAILED(hr) || displayName == nullptr) {
        return {};
    }

    const QString result = QString::fromWCharArray(displayName).trimmed().toLower();
    CoTaskMemFree(displayName);
    return result;
}

std::optional<quint32> sessionProcessId(IAudioSessionControl2 *control2)
{
    if (control2 == nullptr) {
        return std::nullopt;
    }

    DWORD rawPid = 0;
    if (FAILED(control2->GetProcessId(&rawPid)) || rawPid == 0) {
        return std::nullopt;
    }
    return static_cast<quint32>(rawPid);
}

std::vector<ActiveAudioSession> collectActiveSessions(
    IMMDevice *device,
    std::optional<quint32> ignorePid)
{
    std::vector<ActiveAudioSession> activeSessions;
#if !defined(_MSC_VER)
    Q_UNUSED(device);
    Q_UNUSED(ignorePid);
    return activeSessions;
#else
    if (device == nullptr) {
        return activeSessions;
    }

    ComPtr<IAudioSessionManager2> sessionManager;
    if (FAILED(device->Activate(
            __uuidof(IAudioSessionManager2),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void **>(sessionManager.GetAddressOf())))) {
        return activeSessions;
    }

    ComPtr<IAudioSessionEnumerator> enumerator;
    if (FAILED(sessionManager->GetSessionEnumerator(enumerator.GetAddressOf())) || !enumerator) {
        return activeSessions;
    }

    int count = 0;
    if (FAILED(enumerator->GetCount(&count)) || count <= 0) {
        return activeSessions;
    }

    for (int index = 0; index < count; ++index) {
        ComPtr<IAudioSessionControl> sessionControl;
        if (FAILED(enumerator->GetSession(index, sessionControl.GetAddressOf())) || !sessionControl) {
            continue;
        }

        ComPtr<IAudioSessionControl2> sessionControl2;
        sessionControl.As(&sessionControl2);

        const std::optional<quint32> pid = sessionProcessId(sessionControl2.Get());
        if (ignorePid.has_value() && pid.has_value() && *pid == *ignorePid) {
            continue;
        }

        ComPtr<IAudioMeterInformation> meterInformation;
        if (FAILED(sessionControl.As(&meterInformation)) || !meterInformation) {
            continue;
        }

        float peak = 0.0f;
        if (FAILED(meterInformation->GetPeakValue(&peak)) || peak <= kSessionPeakThreshold) {
            continue;
        }

        ActiveAudioSession session;
        session.pid = pid;
        session.displayName = sessionDisplayName(sessionControl.Get());
        session.processName = pid.has_value() ? processNameForPid(*pid) : QString();
        session.peak = static_cast<double>(peak);
        activeSessions.push_back(std::move(session));
    }

    return activeSessions;
#endif
}

bool masterPeakExceedsThreshold(IMMDevice *device)
{
#if !defined(_MSC_VER)
    Q_UNUSED(device);
    return false;
#else
    if (device == nullptr) {
        return false;
    }

    ComPtr<IAudioMeterInformation> meterInformation;
    if (FAILED(device->Activate(
            __uuidof(IAudioMeterInformation),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void **>(meterInformation.GetAddressOf())))
        || !meterInformation) {
        return false;
    }

    float peak = 0.0f;
    return SUCCEEDED(meterInformation->GetPeakValue(&peak)) && peak > kMasterPeakThreshold;
#endif
}

class WasapiAudioOutputPollBackend final : public AudioOutputPollBackend
{
public:
    bool isAvailable() const override
    {
        return true;
    }

    QString backendName() const override
    {
        return QStringLiteral("WASAPI");
    }

    AudioOutputPollResult poll(
        const AudioOutputMonitorOptions &options,
        std::optional<quint32> ignorePid) override
    {
        AudioOutputPollResult result;

        ScopedComInitialization com;
        if (!com.isUsable()) {
            return result;
        }

        ComPtr<IMMDeviceEnumerator> enumerator;
        if (FAILED(CoCreateInstance(
                __uuidof(MMDeviceEnumerator),
                nullptr,
                CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator),
                reinterpret_cast<void **>(enumerator.GetAddressOf())))
            || !enumerator) {
            return result;
        }

        ComPtr<IMMDevice> defaultDevice;
        if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, defaultDevice.GetAddressOf()))
            || !defaultDevice) {
            return result;
        }

        const std::vector<ActiveAudioSession> activeSessions = collectActiveSessions(defaultDevice.Get(), ignorePid);
        std::vector<ActiveAudioSession> matchedSessions;
        matchedSessions.reserve(activeSessions.size());
        for (const ActiveAudioSession &session : activeSessions) {
            if (!options.preferMediaSessions || looksLikeMediaSession(session)) {
                matchedSessions.push_back(session);
            }
        }

        result.matchedSummary = matchedSessions.empty() ? QString() : buildSessionSummary(matchedSessions);
        result.hasAllActive = !activeSessions.empty();
        result.allSummary = activeSessions.empty() ? QString() : buildSessionSummary(activeSessions);
        if (!matchedSessions.empty()) {
            result.currentlyPlaying = true;
            return result;
        }

        if (options.includeMasterPeakFallback && masterPeakExceedsThreshold(defaultDevice.Get())) {
            result.currentlyPlaying = true;
        }
        return result;
    }
};
#else
class WasapiAudioOutputPollBackend final : public AudioOutputPollBackend
{
public:
    bool isAvailable() const override
    {
        return false;
    }

    QString backendName() const override
    {
        return QStringLiteral("Unavailable");
    }

    AudioOutputPollResult poll(
        const AudioOutputMonitorOptions &,
        std::optional<quint32>) override
    {
        return {};
    }
};
#endif

AudioOutputPollBackendFactory defaultBackendFactory()
{
    return []() {
        return std::static_pointer_cast<AudioOutputPollBackend>(
            std::make_shared<WasapiAudioOutputPollBackend>());
    };
}

}

AudioOutputPollWorker::AudioOutputPollWorker(
    std::shared_ptr<AudioOutputPollBackend> backend,
    AudioOutputMonitorOptions options,
    QObject *parent)
    : QObject(parent)
    , m_backend(std::move(backend))
    , m_options(options)
{
}

void AudioOutputPollWorker::poll(quint32 ignorePid, bool hasIgnorePid)
{
    const std::optional<quint32> normalizedPid = hasIgnorePid
        ? std::optional<quint32>(ignorePid)
        : std::nullopt;
    if (!m_backend) {
        Q_EMIT pollFinished(AudioOutputPollResult{});
        return;
    }
    Q_EMIT pollFinished(m_backend->poll(m_options, normalizedPid));
}

WindowsAudioOutputMonitorService::WindowsAudioOutputMonitorService(QObject *parent)
    : WindowsAudioOutputMonitorService(defaultBackendFactory(), AudioOutputMonitorOptions{}, parent)
{
}

WindowsAudioOutputMonitorService::WindowsAudioOutputMonitorService(
    AudioOutputMonitorOptions options,
    QObject *parent)
    : WindowsAudioOutputMonitorService(defaultBackendFactory(), options, parent)
{
}

WindowsAudioOutputMonitorService::WindowsAudioOutputMonitorService(
    AudioOutputPollBackendFactory backendFactory,
    AudioOutputMonitorOptions options,
    QObject *parent)
    : AudioOutputMonitorService(parent)
    , m_options(options)
    , m_backendFactory(std::move(backendFactory))
    , m_syncBackend(m_backendFactory ? m_backendFactory() : nullptr)
{
    m_options.pollIntervalMs = std::max(kMinimumPollIntervalMs, m_options.pollIntervalMs);
    m_options.silenceDebounceCount = std::max(1, m_options.silenceDebounceCount);
    if (m_options.ignoreCurrentProcessAudio) {
        m_currentPid = static_cast<quint32>(QCoreApplication::applicationPid());
    }

    qRegisterMetaType<AudioOutputPollResult>("AudioOutputPollResult");

    m_timer.setParent(this);
    m_timer.setInterval(m_options.pollIntervalMs);
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &WindowsAudioOutputMonitorService::pollAsync);
}

WindowsAudioOutputMonitorService::~WindowsAudioOutputMonitorService()
{
    shutdownWorker();
}

bool WindowsAudioOutputMonitorService::isPlaying() const
{
    return m_isPlaying;
}

bool WindowsAudioOutputMonitorService::isAvailable() const
{
    return m_syncBackend && m_syncBackend->isAvailable();
}

void WindowsAudioOutputMonitorService::start()
{
    if (m_running) {
        return;
    }
    if (!isAvailable()) {
        if (!m_loggedUnavailable) {
            m_loggedUnavailable = true;
            qWarning().noquote() << "[AudioOutputMonitorService] backend unavailable, monitor not started";
        }
        return;
    }

    m_running = true;
    m_pollInflight = false;
    m_silenceCounter = 0;
    ensureWorker();
    m_timer.start();
    qInfo().noquote() << "[AudioOutputMonitorService] started"
                      << "(" << (m_syncBackend ? m_syncBackend->backendName() : QStringLiteral("unknown")) << ")";
}

void WindowsAudioOutputMonitorService::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    m_timer.stop();
    shutdownWorker();
    m_pollInflight = false;
    m_silenceCounter = 0;
    if (m_isPlaying) {
        m_isPlaying = false;
        Q_EMIT audioOutputStopped();
        Q_EMIT audioStateChanged(false);
    }
    qInfo().noquote() << "[AudioOutputMonitorService] stopped";
}

void WindowsAudioOutputMonitorService::pollAsync()
{
    if (!m_running) {
        return;
    }
    if (!m_options.useWorkerThread || m_workerThread == nullptr || m_worker == nullptr || !m_workerThread->isRunning()) {
        pollSync();
        return;
    }
    if (m_pollInflight) {
        return;
    }

    m_pollInflight = true;
    Q_EMIT pollRequested(m_currentPid.value_or(0u), m_currentPid.has_value());
}

void WindowsAudioOutputMonitorService::onPollFinished(const AudioOutputPollResult &result)
{
    m_pollInflight = false;
    if (!m_running) {
        return;
    }
    applyPollResult(result);
}

void WindowsAudioOutputMonitorService::ensureWorker()
{
    if (!m_options.useWorkerThread || !m_backendFactory) {
        return;
    }
    if (m_workerThread != nullptr && m_worker != nullptr && m_workerThread->isRunning()) {
        return;
    }

    std::shared_ptr<AudioOutputPollBackend> workerBackend = m_backendFactory();
    if (!workerBackend || !workerBackend->isAvailable()) {
        return;
    }

    m_workerThread = new QThread(this);
    m_worker = new AudioOutputPollWorker(workerBackend, m_options);
    m_worker->moveToThread(m_workerThread);
    connect(this, &WindowsAudioOutputMonitorService::pollRequested, m_worker, &AudioOutputPollWorker::poll);
    connect(m_worker, &AudioOutputPollWorker::pollFinished, this, &WindowsAudioOutputMonitorService::onPollFinished);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread->start();
}

void WindowsAudioOutputMonitorService::shutdownWorker()
{
    if (m_worker != nullptr) {
        disconnect(this, &WindowsAudioOutputMonitorService::pollRequested, m_worker, &AudioOutputPollWorker::poll);
        disconnect(m_worker, &AudioOutputPollWorker::pollFinished, this, &WindowsAudioOutputMonitorService::onPollFinished);
    }

    QThread *thread = m_workerThread;
    m_workerThread = nullptr;
    m_worker = nullptr;
    if (thread != nullptr) {
        thread->quit();
        thread->wait(1000);
    }
}

void WindowsAudioOutputMonitorService::pollSync()
{
    if (m_pollInflight || !m_syncBackend) {
        return;
    }
    m_pollInflight = true;
    const AudioOutputPollResult result = m_syncBackend->poll(m_options, m_currentPid);
    m_pollInflight = false;
    if (!m_running) {
        return;
    }
    applyPollResult(result);
}

void WindowsAudioOutputMonitorService::applyPollResult(const AudioOutputPollResult &result)
{
    if (!result.matchedSummary.isEmpty()) {
        m_lastSessionSummary = result.matchedSummary;
    } else if (!result.allSummary.isEmpty()) {
        m_lastSessionSummary = result.allSummary;
    }

    if (m_options.preferMediaSessions && !result.currentlyPlaying && result.hasAllActive) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastRejectedLogMs >= kRejectedLogIntervalMs) {
            m_lastRejectedLogMs = nowMs;
            qInfo().noquote() << "[AudioOutputMonitorService] active non-media audio ignored:" << result.allSummary;
        }
    }

    if (result.currentlyPlaying) {
        m_silenceCounter = 0;
        if (!m_isPlaying) {
            m_isPlaying = true;
            Q_EMIT audioOutputStarted();
            Q_EMIT audioStateChanged(true);
            qInfo().noquote() << "[AudioOutputMonitorService] audio detected:" << m_lastSessionSummary;
        }
        return;
    }

    if (!m_isPlaying) {
        return;
    }

    ++m_silenceCounter;
    if (m_silenceCounter < m_options.silenceDebounceCount) {
        return;
    }

    m_isPlaying = false;
    m_silenceCounter = 0;
    Q_EMIT audioOutputStopped();
    Q_EMIT audioStateChanged(false);
    qInfo().noquote() << "[AudioOutputMonitorService] audio stopped (last=" << m_lastSessionSummary << ")";
}
