#include <QAudioOutput>
#include <QSignalSpy>
#include <QtTest>

#include "services/audio_service.h"

class AudioServiceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fallbackPlaybackEmitsLifecycleSignals();
    void structuredSpeechRequestEmitsLifecycleSignals();
    void configureVoiceKeepsFallbackPlaybackWorking();
    void configureProviderKeepsFallbackPlaybackWorking();
    void setVolumeUpdatesAudioOutput();
    void disablingCacheKeepsFallbackPlaybackWorking();
    void queuedFallbackPlaybackKeepsSignalPairsBalanced();
    void interruptClearsQueuedFallbackPlayback();
    void shutdownIsIdempotent();
    void missingPreferredAudioEmitsWarning();
    void noAudioOutputFallbackEmitsWarning();
};

void AudioServiceTest::fallbackPlaybackEmitsLifecycleSignals()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    QSignalSpy startedSpy(&service, &AudioService::playbackStarted);
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.speak(QStringLiteral("hello"));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QStringLiteral("hello"));
}

void AudioServiceTest::structuredSpeechRequestEmitsLifecycleSignals()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    QSignalSpy startedSpy(&service, &AudioService::playbackStarted);
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.speakRequest(AudioPlaybackRequest{
        QStringLiteral("priority hello"),
        static_cast<int>(AudioPlaybackPriority::High),
        false,
        QString(),
    });

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QStringLiteral("priority hello"));
}

void AudioServiceTest::configureVoiceKeepsFallbackPlaybackWorking()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    service.configureVoice(QStringLiteral("zh-CN-YunxiNeural"), QStringLiteral("+15%"));

    QSignalSpy startedSpy(&service, &AudioService::playbackStarted);
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.speak(QStringLiteral("voice configured"));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QStringLiteral("voice configured"));
}

void AudioServiceTest::configureProviderKeepsFallbackPlaybackWorking()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    service.setTtsProvider(QStringLiteral("openai"));

    QSignalSpy startedSpy(&service, &AudioService::playbackStarted);
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.speak(QStringLiteral("provider configured"));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QStringLiteral("provider configured"));
}

void AudioServiceTest::setVolumeUpdatesAudioOutput()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    auto *audioOutput = service.findChild<QAudioOutput *>();
    QVERIFY(audioOutput != nullptr);

    service.setVolume(0.25);

    QCOMPARE(audioOutput->volume(), 0.25f);
}

void AudioServiceTest::disablingCacheKeepsFallbackPlaybackWorking()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    QSignalSpy startedSpy(&service, &AudioService::playbackStarted);
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.setCacheEnabled(false);
    service.speak(QStringLiteral("cache disabled"));

    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QStringLiteral("cache disabled"));
}

void AudioServiceTest::queuedFallbackPlaybackKeepsSignalPairsBalanced()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    QSignalSpy startedSpy(&service, &AudioService::playbackStarted);
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.speak(QStringLiteral("one"));
    service.speak(QStringLiteral("two"));

    QCOMPARE(startedSpy.count(), 2);
    QCOMPARE(finishedSpy.count(), 2);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QStringLiteral("one"));
    QCOMPARE(startedSpy.at(1).at(0).toString(), QStringLiteral("two"));
}

void AudioServiceTest::interruptClearsQueuedFallbackPlayback()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    QSignalSpy startedSpy(&service, &AudioService::playbackStarted);
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.speak(QStringLiteral("one"));
    QCOMPARE(startedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);

    service.interrupt();
    QCOMPARE(finishedSpy.count(), 1);

    service.speak(QStringLiteral("two"));

    QCOMPARE(startedSpy.count(), 2);
    QCOMPARE(finishedSpy.count(), 2);
    QCOMPARE(startedSpy.at(0).at(0).toString(), QStringLiteral("one"));
    QCOMPARE(startedSpy.at(1).at(0).toString(), QStringLiteral("two"));
}

void AudioServiceTest::shutdownIsIdempotent()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    QSignalSpy finishedSpy(&service, &AudioService::playbackFinished);

    service.speak(QStringLiteral("one"));
    QCOMPARE(finishedSpy.count(), 1);

    service.shutdown();
    service.shutdown();

    QCOMPARE(finishedSpy.count(), 1);
}

void AudioServiceTest::missingPreferredAudioEmitsWarning()
{
    QtAudioService service(QStringLiteral("Z:/nonexistent/aemeath-notification.wav"));
    QSignalSpy warningSpy(&service, &AudioService::playbackWarning);

    service.speakRequest(AudioPlaybackRequest{
        QStringLiteral("missing cached audio"),
        static_cast<int>(AudioPlaybackPriority::Normal),
        false,
        QStringLiteral("Z:/nonexistent/missing-script-audio.mp3"),
    });

    QVERIFY(!warningSpy.isEmpty());
    QCOMPARE(
        warningSpy.at(0).at(0).toString(),
        QStringLiteral("脚本缓存语音不存在，将回退到 TTS 或提示音。"));
}

void AudioServiceTest::noAudioOutputFallbackEmitsWarning()
{
    QtAudioService service;
    QSignalSpy warningSpy(&service, &AudioService::playbackWarning);

    service.speak(QStringLiteral("silent fallback"));

    QVERIFY(!warningSpy.isEmpty());
    QCOMPARE(
        warningSpy.at(warningSpy.count() - 1).at(0).toString(),
        QStringLiteral("没有可用的语音音频输出，当前文本只会记录日志。"));
}

QTEST_MAIN(AudioServiceTest)

#include "test_audio_service.moc"
