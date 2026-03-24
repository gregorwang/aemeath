#include <QtTest>

#include "services/openai_commentary_service.h"

class OpenAiCommentaryServiceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void normalizeBaseUrlAddsV1();
    void normalizeBaseUrlStripsChatCompletions();
    void normalizeBaseUrlStripsResponsesSuffix();
    void normalizeBaseUrlKeepsProviderSpecificPrefixes();
    void resolveApiKeyUsesExplicitValueFirst();
    void resolveApiKeyFallsBackToEnvironmentByProvider();
    void extractResponseTextReadsStringContent();
    void buildChatPayloadFallsBackWithoutImage();
    void buildChatPayloadUsesConfiguredPromptsAndLimits();
    void buildChatPayloadIncludesForegroundContextWhenFallbackEnabled();
    void sanitizeCommentaryTextTrimsToConfiguredLimit();
    void buildFailureMessageHandlesUnauthorized();
    void requestCommentaryFailsFastWhenOfflineModeEnabled();
};

void OpenAiCommentaryServiceTest::normalizeBaseUrlAddsV1()
{
    QCOMPARE(
        OpenAiCommentaryService::normalizeBaseUrl(QStringLiteral("https://api.x.ai")),
        QStringLiteral("https://api.x.ai/v1"));
}

void OpenAiCommentaryServiceTest::normalizeBaseUrlStripsChatCompletions()
{
    QCOMPARE(
        OpenAiCommentaryService::normalizeBaseUrl(QStringLiteral("https://api.openai.com/v1/chat/completions")),
        QStringLiteral("https://api.openai.com/v1"));
}

void OpenAiCommentaryServiceTest::normalizeBaseUrlStripsResponsesSuffix()
{
    QCOMPARE(
        OpenAiCommentaryService::normalizeBaseUrl(QStringLiteral("https://api.x.ai/v1/responses")),
        QStringLiteral("https://api.x.ai/v1"));
}

void OpenAiCommentaryServiceTest::normalizeBaseUrlKeepsProviderSpecificPrefixes()
{
    QCOMPARE(
        OpenAiCommentaryService::normalizeBaseUrl(QStringLiteral("https://open.bigmodel.cn/api/paas/v4")),
        QStringLiteral("https://open.bigmodel.cn/api/paas/v4"));
    QCOMPARE(
        OpenAiCommentaryService::normalizeBaseUrl(QStringLiteral("https://ark.cn-beijing.volces.com/api/v3")),
        QStringLiteral("https://ark.cn-beijing.volces.com/api/v3"));
}

void OpenAiCommentaryServiceTest::resolveApiKeyUsesExplicitValueFirst()
{
    QCOMPARE(
        OpenAiCompatibleClient::resolveApiKey(QStringLiteral("openai"), QStringLiteral("explicit-key")),
        QStringLiteral("explicit-key"));
}

void OpenAiCommentaryServiceTest::resolveApiKeyFallsBackToEnvironmentByProvider()
{
    qputenv("OPENAI_API_KEY", "");
    qputenv("POLOAI_API_KEY", "polo-key");
    qputenv("XAI_API_KEY", "xai-key");
    qputenv("DEEPSEEK_API_KEY", "deepseek-key");
    qputenv("ZHIPU_API_KEY", "zhipu-key");

    QCOMPARE(
        OpenAiCompatibleClient::resolveApiKey(QStringLiteral("openai"), QString()),
        QStringLiteral("polo-key"));
    QCOMPARE(
        OpenAiCompatibleClient::resolveApiKey(QStringLiteral("xai"), QString()),
        QStringLiteral("xai-key"));
    QCOMPARE(
        OpenAiCompatibleClient::resolveApiKey(QStringLiteral("deepseek"), QString()),
        QStringLiteral("deepseek-key"));
    QCOMPARE(
        OpenAiCompatibleClient::resolveApiKey(QStringLiteral("zhipu"), QString()),
        QStringLiteral("zhipu-key"));
}

void OpenAiCommentaryServiceTest::extractResponseTextReadsStringContent()
{
    const QByteArray payload =
        R"({"choices":[{"message":{"content":"骨架网络客户端已接通。"}}]})";

    QCOMPARE(
        OpenAiCommentaryService::extractResponseText(payload),
        QStringLiteral("骨架网络客户端已接通。"));
}

void OpenAiCommentaryServiceTest::buildChatPayloadFallsBackWithoutImage()
{
    OpenAiCompatibleConfig config;
    config.model = QStringLiteral("gpt-5-mini");

    const QByteArray payload = OpenAiCommentaryService::buildChatPayload(config, QString());
    QVERIFY(payload.contains("抓屏不可用"));
    QVERIFY(payload.contains("gpt-5-mini"));
}

void OpenAiCommentaryServiceTest::buildChatPayloadUsesConfiguredPromptsAndLimits()
{
    OpenAiCompatibleConfig config;
    config.model = QStringLiteral("gpt-5-mini");
    config.commentarySystemPrompt = QStringLiteral("custom system");
    config.commentaryUserPrompt = QStringLiteral("custom user");
    config.commentaryNoImagePrompt = QStringLiteral("custom no image");
    config.commentaryMaxTokens = 88;
    config.commentaryTemperature = 0.35;

    const QByteArray payloadWithImage = OpenAiCommentaryService::buildChatPayload(config, QStringLiteral("abc123"));
    QVERIFY(payloadWithImage.contains("custom system"));
    QVERIFY(payloadWithImage.contains("custom user"));
    QVERIFY(payloadWithImage.contains("\"max_tokens\":88"));
    QVERIFY(payloadWithImage.contains("\"temperature\":0.35"));

    const QByteArray payloadWithoutImage = OpenAiCommentaryService::buildChatPayload(config, QString());
    QVERIFY(payloadWithoutImage.contains("custom no image"));
}

void OpenAiCommentaryServiceTest::buildChatPayloadIncludesForegroundContextWhenFallbackEnabled()
{
    OpenAiCompatibleConfig config;
    config.commentaryOcrFallbackEnabled = true;
    config.commentaryNoImagePrompt = QStringLiteral("抓屏失败");

    ForegroundWindowContext context;
    context.title = QStringLiteral("Visual Studio Code");
    context.processName = QStringLiteral("Code.exe");

    const QByteArray payload = OpenAiCommentaryService::buildChatPayload(config, QString(), context);
    QVERIFY(payload.contains("抓屏失败"));
    QVERIFY(payload.contains("前台窗口上下文"));
    QVERIFY(payload.contains("Visual Studio Code"));
    QVERIFY(payload.contains("Code.exe"));
}

void OpenAiCommentaryServiceTest::sanitizeCommentaryTextTrimsToConfiguredLimit()
{
    OpenAiCompatibleConfig config;
    config.commentaryMaxResponseChars = 12;

    QCOMPARE(
        OpenAiCommentaryService::sanitizeCommentaryText(config, QStringLiteral("  这是一段会被截断的长评论内容  ")),
        QStringLiteral("这是一段会被截断的长评论内容"));
}

void OpenAiCommentaryServiceTest::buildFailureMessageHandlesUnauthorized()
{
    const QString message = OpenAiCommentaryService::buildFailureMessage(401, QStringLiteral("Unauthorized"), QByteArray());
    QVERIFY(message.contains(QStringLiteral("鉴权失败")));
}

void OpenAiCommentaryServiceTest::requestCommentaryFailsFastWhenOfflineModeEnabled()
{
    OpenAiCompatibleConfig config;
    config.offlineMode = true;
    OpenAiCommentaryService service(config);
    QSignalSpy failedSpy(&service, &ScreenCommentaryService::commentaryFailed);

    service.requestCommentary();

    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(failedSpy.constFirst().constFirst().toString().contains(QStringLiteral("离线模式")));
}

QTEST_MAIN(OpenAiCommentaryServiceTest)

#include "test_openai_commentary_service.moc"
