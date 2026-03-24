#include <deque>
#include <memory>

#include <QMetaObject>
#include <QSignalSpy>
#include <QtTest>

#include "services/audio_output_monitor_service.h"

namespace {

struct FakeBackendState
{
    std::deque<AudioOutputPollResult> queuedResults;
    int pollCount = 0;
    bool available = true;
};

class FakeAudioOutputPollBackend final : public AudioOutputPollBackend
{
public:
    explicit FakeAudioOutputPollBackend(std::shared_ptr<FakeBackendState> state)
        : m_state(std::move(state))
    {
    }

    bool isAvailable() const override
    {
        return m_state->available;
    }

    QString backendName() const override
    {
        return QStringLiteral("Fake");
    }

    AudioOutputPollResult poll(
        const AudioOutputMonitorOptions &,
        std::optional<quint32>) override
    {
        ++m_state->pollCount;
        if (m_state->queuedResults.empty()) {
            return {};
        }
        const AudioOutputPollResult result = m_state->queuedResults.front();
        m_state->queuedResults.pop_front();
        return result;
    }

private:
    std::shared_ptr<FakeBackendState> m_state;
};

AudioOutputMonitorOptions testOptions()
{
    AudioOutputMonitorOptions options;
    options.pollIntervalMs = 60000;
    options.useWorkerThread = false;
    options.silenceDebounceCount = 5;
    return options;
}

std::unique_ptr<WindowsAudioOutputMonitorService> createService(std::shared_ptr<FakeBackendState> state)
{
    return std::make_unique<WindowsAudioOutputMonitorService>(
        [state]() {
            return std::static_pointer_cast<AudioOutputPollBackend>(
                std::make_shared<FakeAudioOutputPollBackend>(state));
        },
        testOptions());
}

}

class AudioOutputMonitorServiceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stopEmitsStoppedWhenServiceWasPlaying();
    void silenceDebounceRequiresConsecutiveSilentPolls();
    void nonMediaAudioDoesNotFlipPlayingState();
    void unavailableBackendSoftFailsOnStart();
};

void AudioOutputMonitorServiceTest::stopEmitsStoppedWhenServiceWasPlaying()
{
    auto state = std::make_shared<FakeBackendState>();
    state->queuedResults.push_back(AudioOutputPollResult{
        true,
        QStringLiteral("spotify(pid=100):0.420"),
        true,
        QStringLiteral("spotify(pid=100):0.420"),
    });

    const auto service = createService(state);
    QSignalSpy startedSpy(service.get(), &AudioOutputMonitorService::audioOutputStarted);
    QSignalSpy stoppedSpy(service.get(), &AudioOutputMonitorService::audioOutputStopped);
    QSignalSpy stateSpy(service.get(), &AudioOutputMonitorService::audioStateChanged);

    service->start();
    QVERIFY(QMetaObject::invokeMethod(service.get(), "pollAsync", Qt::DirectConnection));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(stoppedSpy.count(), 0);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy.at(0).at(0).toBool(), true);
    QVERIFY(service->isPlaying());

    service->stop();

    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(1).at(0).toBool(), false);
    QVERIFY(!service->isPlaying());

    service->stop();
    QCOMPARE(stoppedSpy.count(), 1);
}

void AudioOutputMonitorServiceTest::silenceDebounceRequiresConsecutiveSilentPolls()
{
    auto state = std::make_shared<FakeBackendState>();
    state->queuedResults.push_back(AudioOutputPollResult{
        true,
        QStringLiteral("vlc(pid=222):0.510"),
        true,
        QStringLiteral("vlc(pid=222):0.510"),
    });
    for (int index = 0; index < 5; ++index) {
        state->queuedResults.push_back(AudioOutputPollResult{
            false,
            QString(),
            false,
            QString(),
        });
    }

    const auto service = createService(state);
    QSignalSpy startedSpy(service.get(), &AudioOutputMonitorService::audioOutputStarted);
    QSignalSpy stoppedSpy(service.get(), &AudioOutputMonitorService::audioOutputStopped);

    service->start();
    QVERIFY(QMetaObject::invokeMethod(service.get(), "pollAsync", Qt::DirectConnection));
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(service->isPlaying());

    for (int index = 0; index < 4; ++index) {
        QVERIFY(QMetaObject::invokeMethod(service.get(), "pollAsync", Qt::DirectConnection));
        QCOMPARE(stoppedSpy.count(), 0);
        QVERIFY(service->isPlaying());
    }

    QVERIFY(QMetaObject::invokeMethod(service.get(), "pollAsync", Qt::DirectConnection));
    QCOMPARE(stoppedSpy.count(), 1);
    QVERIFY(!service->isPlaying());
}

void AudioOutputMonitorServiceTest::nonMediaAudioDoesNotFlipPlayingState()
{
    auto state = std::make_shared<FakeBackendState>();
    state->queuedResults.push_back(AudioOutputPollResult{
        false,
        QString(),
        true,
        QStringLiteral("python(pid=333):0.150"),
    });

    const auto service = createService(state);
    QSignalSpy startedSpy(service.get(), &AudioOutputMonitorService::audioOutputStarted);

    service->start();
    QVERIFY(QMetaObject::invokeMethod(service.get(), "pollAsync", Qt::DirectConnection));

    QCOMPARE(startedSpy.count(), 0);
    QVERIFY(!service->isPlaying());
}

void AudioOutputMonitorServiceTest::unavailableBackendSoftFailsOnStart()
{
    auto state = std::make_shared<FakeBackendState>();
    state->available = false;

    const auto service = createService(state);
    QSignalSpy startedSpy(service.get(), &AudioOutputMonitorService::audioOutputStarted);

    QVERIFY(!service->isAvailable());
    service->start();
    QVERIFY(QMetaObject::invokeMethod(service.get(), "pollAsync", Qt::DirectConnection));

    QCOMPARE(startedSpy.count(), 0);
    QCOMPARE(state->pollCount, 0);
    QVERIFY(!service->isPlaying());
}

QTEST_MAIN(AudioOutputMonitorServiceTest)

#include "test_audio_output_monitor_service.moc"
