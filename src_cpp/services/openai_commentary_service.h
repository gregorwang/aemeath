#pragma once

#include <QNetworkAccessManager>
#include <QUrl>

#include "runtime/screen_capture.h"
#include "services/openai_compatible_client.h"
#include "services/service_contracts.h"

class OpenAiCommentaryService final : public ScreenCommentaryService
{
    Q_OBJECT

public:
    explicit OpenAiCommentaryService(OpenAiCompatibleConfig config, QObject *parent = nullptr);

    void requestCommentary() override;
    void cancel() override;

    static QString normalizeBaseUrl(const QString &baseUrl);
    static QString extractResponseText(const QByteArray &payload);
    static QString buildFailureMessage(int statusCode, const QString &errorString, const QByteArray &responseBody);
    static QByteArray buildChatPayload(
        const OpenAiCompatibleConfig &config,
        const QString &imageBase64,
        const ForegroundWindowContext &fallbackContext = {});
    static QString sanitizeCommentaryText(const OpenAiCompatibleConfig &config, const QString &text);

private:
    static QString buildNoImagePrompt(const OpenAiCompatibleConfig &config, const ForegroundWindowContext &fallbackContext);

    OpenAiCompatibleConfig m_config;
    QNetworkAccessManager m_network;
    OpenAiCompatibleClient m_client;
};
