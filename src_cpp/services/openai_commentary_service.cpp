#include "services/openai_commentary_service.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <utility>

#include "runtime/screen_capture.h"

OpenAiCommentaryService::OpenAiCommentaryService(OpenAiCompatibleConfig config, QObject *parent)
    : ScreenCommentaryService(parent)
    , m_config(std::move(config))
    , m_client(m_config, &m_network, this)
{
    connect(&m_client, &OpenAiCompatibleClient::completionReady, this, [this](const QString &text) {
        const QString sanitized = sanitizeCommentaryText(m_config, text);
        if (sanitized.isEmpty()) {
            Q_EMIT commentaryFailed(QStringLiteral("LLM 返回为空。"));
            return;
        }
        Q_EMIT commentaryReady(sanitized);
    });
    connect(&m_client, &OpenAiCompatibleClient::completionFailed, this, &OpenAiCommentaryService::commentaryFailed);
}

void OpenAiCommentaryService::requestCommentary()
{
    if (m_config.offlineMode) {
        Q_EMIT commentaryFailed(QStringLiteral("离线模式已启用，屏幕评论不可用。"));
        return;
    }
    const QString imageBase64 = ScreenCapture::captureForegroundWindowBase64Jpeg();
    const ForegroundWindowContext fallbackContext =
        imageBase64.trimmed().isEmpty() && m_config.commentaryOcrFallbackEnabled
            ? ScreenCapture::captureForegroundWindowContext()
            : ForegroundWindowContext{};
    const QByteArray payload = buildChatPayload(m_config, imageBase64, fallbackContext);
    m_client.requestChatCompletion(payload);
}

void OpenAiCommentaryService::cancel()
{
    m_client.cancelCurrentRequest();
}

QString OpenAiCommentaryService::normalizeBaseUrl(const QString &baseUrl)
{
    return OpenAiCompatibleClient::normalizeBaseUrl(baseUrl);
}

QString OpenAiCommentaryService::extractResponseText(const QByteArray &payload)
{
    return OpenAiCompatibleClient::extractResponseText(payload);
}

QString OpenAiCommentaryService::buildFailureMessage(int statusCode, const QString &errorString, const QByteArray &responseBody)
{
    return OpenAiCompatibleClient::buildFailureMessage(statusCode, errorString, responseBody);
}

QByteArray OpenAiCommentaryService::buildChatPayload(
    const OpenAiCompatibleConfig &config,
    const QString &imageBase64,
    const ForegroundWindowContext &fallbackContext)
{
    QJsonObject systemMessage;
    systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
    systemMessage.insert(
        QStringLiteral("content"),
        config.commentarySystemPrompt.trimmed().isEmpty()
            ? QStringLiteral("你是一个中文桌面伴侣，请先说结论，再补一句细节，总长度不超过30字。")
            : config.commentarySystemPrompt.trimmed());

    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));

    if (!imageBase64.trimmed().isEmpty()) {
        QJsonArray content;

        QJsonObject textPart;
        textPart.insert(QStringLiteral("type"), QStringLiteral("text"));
        textPart.insert(
            QStringLiteral("text"),
            config.commentaryUserPrompt.trimmed().isEmpty()
                ? QStringLiteral("看看我的当前屏幕，用一句短句告诉我你看到了什么。")
                : config.commentaryUserPrompt.trimmed());
        content.append(textPart);

        QJsonObject imagePart;
        imagePart.insert(QStringLiteral("type"), QStringLiteral("image_url"));
        QJsonObject imageUrl;
        imageUrl.insert(QStringLiteral("url"), QStringLiteral("data:image/jpeg;base64,%1").arg(imageBase64));
        imagePart.insert(QStringLiteral("image_url"), imageUrl);
        content.append(imagePart);

        userMessage.insert(QStringLiteral("content"), content);
    } else {
        userMessage.insert(QStringLiteral("content"), buildNoImagePrompt(config, fallbackContext));
    }

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMessage);

    QJsonObject payload;
    payload.insert(QStringLiteral("model"), config.model);
    payload.insert(QStringLiteral("messages"), messages);
    payload.insert(QStringLiteral("max_tokens"), qBound(1, config.commentaryMaxTokens, 512));
    payload.insert(QStringLiteral("temperature"), qBound(0.0, config.commentaryTemperature, 2.0));

    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QString OpenAiCommentaryService::buildNoImagePrompt(
    const OpenAiCompatibleConfig &config,
    const ForegroundWindowContext &fallbackContext)
{
    QString prompt = config.commentaryNoImagePrompt.trimmed().isEmpty()
        ? QStringLiteral("当前抓屏不可用。请给一句不超过30字的状态播报。")
        : config.commentaryNoImagePrompt.trimmed();

    if (!config.commentaryOcrFallbackEnabled || !fallbackContext.isValid()) {
        return prompt;
    }

    QStringList parts;
    if (!fallbackContext.title.trimmed().isEmpty()) {
        parts.append(QStringLiteral("窗口标题：%1").arg(fallbackContext.title.trimmed()));
    }
    if (!fallbackContext.processName.trimmed().isEmpty()) {
        parts.append(QStringLiteral("程序名：%1").arg(fallbackContext.processName.trimmed()));
    }
    if (parts.isEmpty()) {
        return prompt;
    }

    prompt.append(QStringLiteral("\n前台窗口上下文：%1").arg(parts.join(QStringLiteral("；"))));
    prompt.append(QStringLiteral("\n请基于这些上下文给出一句简短状态播报。"));
    return prompt;
}

QString OpenAiCommentaryService::sanitizeCommentaryText(const OpenAiCompatibleConfig &config, const QString &text)
{
    QString cleaned = text.simplified().trimmed();
    const int maxChars = qBound(20, config.commentaryMaxResponseChars, 300);
    if (cleaned.size() > maxChars) {
        cleaned = cleaned.left(maxChars).trimmed();
    }
    return cleaned;
}
