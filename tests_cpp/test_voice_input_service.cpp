#include <QtTest>

#include "services/voice_input_service.h"

class VoiceInputServiceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void normalizesRecognitionProviderAliases();
    void normalizesWhisperLanguage();
    void defaultModelFollowsProvider();
    void defaultBaseUrlFollowsProvider();
    void buildsZhipuEndpointFromBase();
    void buildsOpenAiEndpointFromBase();
    void extractsTranscriptionText();
    void buildsProviderAwareHttpStatusHint();
    void buildsProviderAwareFailureMessage();
    void legacyGoogleProviderFallsBackToNativeProvider();
    void pushToTalkFailsFastForMissingApiKey();
    void pushToTalkFailsFastWhenOfflineModeEnabled();
};

void VoiceInputServiceTest::normalizesRecognitionProviderAliases()
{
    QCOMPARE(OpenAiVoiceInputService::normalizeRecognitionProvider(QStringLiteral("openai")), QStringLiteral("openai_whisper"));
    QCOMPARE(OpenAiVoiceInputService::normalizeRecognitionProvider(QStringLiteral("xai")), QStringLiteral("xai_realtime"));
    QCOMPARE(OpenAiVoiceInputService::normalizeRecognitionProvider(QStringLiteral("zhipu")), QStringLiteral("zhipu_asr"));
    QCOMPARE(OpenAiVoiceInputService::normalizeRecognitionProvider(QStringLiteral("google")), QStringLiteral("zhipu_asr"));
    QCOMPARE(OpenAiVoiceInputService::normalizeRecognitionProvider(QStringLiteral("google_webspeech")), QStringLiteral("zhipu_asr"));
    QCOMPARE(OpenAiVoiceInputService::normalizeRecognitionProvider(QStringLiteral("unknown")), QStringLiteral("zhipu_asr"));
}

void VoiceInputServiceTest::normalizesWhisperLanguage()
{
    QCOMPARE(OpenAiVoiceInputService::normalizeWhisperLanguage(QStringLiteral("zh-CN")), QStringLiteral("zh"));
    QCOMPARE(OpenAiVoiceInputService::normalizeWhisperLanguage(QStringLiteral("en")), QStringLiteral("en"));
    QCOMPARE(OpenAiVoiceInputService::normalizeWhisperLanguage(QStringLiteral("invalid-language")), QString());
}

void VoiceInputServiceTest::defaultModelFollowsProvider()
{
    QCOMPARE(OpenAiVoiceInputService::defaultModelForProvider(QStringLiteral("zhipu_asr")), QStringLiteral("glm-asr-2512"));
    QCOMPARE(OpenAiVoiceInputService::defaultModelForProvider(QStringLiteral("xai_realtime")), QStringLiteral("grok-2-mini-transcribe"));
    QCOMPARE(OpenAiVoiceInputService::defaultModelForProvider(QStringLiteral("openai_whisper")), QStringLiteral("whisper-1"));
}

void VoiceInputServiceTest::defaultBaseUrlFollowsProvider()
{
    QCOMPARE(
        OpenAiVoiceInputService::defaultBaseUrlForProvider(QStringLiteral("zhipu_asr"), QStringLiteral("https://api.openai.com/v1")),
        QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"));
    QCOMPARE(
        OpenAiVoiceInputService::defaultBaseUrlForProvider(QStringLiteral("xai_realtime"), QString()),
        QStringLiteral("https://api.x.ai/v1"));
    QCOMPARE(
        OpenAiVoiceInputService::defaultBaseUrlForProvider(QStringLiteral("openai_whisper"), QString()),
        QStringLiteral("https://api.openai.com/v1"));
}

void VoiceInputServiceTest::buildsZhipuEndpointFromBase()
{
    QCOMPARE(
        OpenAiVoiceInputService::buildTranscriptionEndpoint(
            QStringLiteral("zhipu_asr"),
            QStringLiteral("https://open.bigmodel.cn/api/paas/v4"),
            QString()),
        QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"));
}

void VoiceInputServiceTest::buildsOpenAiEndpointFromBase()
{
    QCOMPARE(
        OpenAiVoiceInputService::buildTranscriptionEndpoint(
            QStringLiteral("openai_whisper"),
            QStringLiteral("https://api.openai.com/v1/audio/transcriptions"),
            QString()),
        QStringLiteral("https://api.openai.com/v1/audio/transcriptions"));
}

void VoiceInputServiceTest::extractsTranscriptionText()
{
    QCOMPARE(
        OpenAiVoiceInputService::extractTranscriptionText(R"({"text":"hello"})"),
        QStringLiteral("hello"));
    QCOMPARE(
        OpenAiVoiceInputService::extractTranscriptionText(R"({"data":{"text":"nested"}})"),
        QStringLiteral("nested"));
}

void VoiceInputServiceTest::buildsProviderAwareHttpStatusHint()
{
    const QString zhipuHint = OpenAiVoiceInputService::buildHttpStatusHint(
        404,
        QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"),
        QStringLiteral("{\"error\":\"not found\"}"),
        QStringLiteral("zhipu_asr"));
    QVERIFY(zhipuHint.contains(QStringLiteral("/api/paas/v4/audio/transcriptions")));
    QVERIFY(zhipuHint.contains(QStringLiteral("智谱")));

    const QString authHint = OpenAiVoiceInputService::buildHttpStatusHint(
        401,
        QStringLiteral("https://api.x.ai/v1/audio/transcriptions"),
        QString(),
        QStringLiteral("xai_realtime"));
    QVERIFY(authHint.contains(QStringLiteral("API Key")));
    QVERIFY(authHint.contains(QStringLiteral("xAI")));
}

void VoiceInputServiceTest::buildsProviderAwareFailureMessage()
{
    const QString message = OpenAiVoiceInputService::buildFailureMessage(
        429,
        QStringLiteral("Too Many Requests"),
        QByteArray("{\"error\":\"rate limit\"}"),
        QStringLiteral("openai_whisper"),
        QStringLiteral("https://api.openai.com/v1/audio/transcriptions"));
    QVERIFY(message.contains(QStringLiteral("HTTP 429")));
    QVERIFY(message.contains(QStringLiteral("请求过快或额度不足")));
    QVERIFY(message.contains(QStringLiteral("rate limit")));
}

void VoiceInputServiceTest::legacyGoogleProviderFallsBackToNativeProvider()
{
    OpenAiVoiceInputService service;
    VoiceInputConfig config;
    config.microphoneEnabled = true;
    config.voiceInputMode = QStringLiteral("continuous");
    config.wakeupEnabled = true;
    config.asrProvider = QStringLiteral("google");

    QSignalSpy degradedSpy(&service, &VoiceInputService::continuousListeningDegraded);
    QSignalSpy warningSpy(&service, &VoiceInputService::listenerWarning);

    service.configure(config);
    service.startContinuousListening();

    QCOMPARE(degradedSpy.count(), 1);
    QCOMPARE(warningSpy.count(), 1);
    QVERIFY(degradedSpy.constFirst().constFirst().toString().contains(QStringLiteral("API Key")));
    QVERIFY(warningSpy.constFirst().constFirst().toString().contains(QStringLiteral("API Key")));
}

void VoiceInputServiceTest::pushToTalkFailsFastForMissingApiKey()
{
    OpenAiVoiceInputService service;
    VoiceInputConfig config;
    config.microphoneEnabled = true;
    config.voiceInputMode = QStringLiteral("push_to_talk");
    config.asrProvider = QStringLiteral("openai_whisper");
    config.asrApiKey.clear();
    config.llmApiKey.clear();

    QSignalSpy errorSpy(&service, &VoiceInputService::listenerError);
    QSignalSpy captureSpy(&service, &VoiceInputService::captureStateChanged);

    service.configure(config);
    service.startPushToTalkOnce();

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(captureSpy.count(), 0);
    QVERIFY(errorSpy.constFirst().constFirst().toString().contains(QStringLiteral("API Key")));
}

void VoiceInputServiceTest::pushToTalkFailsFastWhenOfflineModeEnabled()
{
    OpenAiVoiceInputService service;
    VoiceInputConfig config;
    config.microphoneEnabled = true;
    config.offlineMode = true;
    config.voiceInputMode = QStringLiteral("push_to_talk");

    QSignalSpy errorSpy(&service, &VoiceInputService::listenerError);
    QSignalSpy captureSpy(&service, &VoiceInputService::captureStateChanged);

    service.configure(config);
    service.startPushToTalkOnce();

    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(captureSpy.count(), 0);
    QVERIFY(errorSpy.constFirst().constFirst().toString().contains(QStringLiteral("离线模式")));
}

QTEST_MAIN(VoiceInputServiceTest)

#include "test_voice_input_service.moc"
