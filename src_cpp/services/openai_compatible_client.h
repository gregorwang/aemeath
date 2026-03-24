#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct OpenAiCompatibleConfig
{
    QString provider = QStringLiteral("openai");
    QString model = QStringLiteral("gpt-5-mini");
    QString apiKey;
    QString baseUrl = QStringLiteral("https://api.openai.com/v1");
    bool offlineMode = false;
    QString commentarySystemPrompt = QStringLiteral("你是一个中文桌面伴侣，请先说结论，再补一句细节，总长度不超过30字。");
    QString commentaryUserPrompt = QStringLiteral("看看我的当前屏幕，用一句短句告诉我你看到了什么。");
    QString commentaryNoImagePrompt = QStringLiteral("当前抓屏不可用。请给一句不超过30字的状态播报。");
    int commentaryMaxTokens = 60;
    double commentaryTemperature = 0.7;
    bool commentaryStreamingEnabled = true;
    bool commentaryOcrFallbackEnabled = false;
    int commentaryStreamChunkChars = 22;
    int commentaryMaxResponseChars = 90;
    QString commentaryPreambleText = QStringLiteral("正在看你的屏幕内容，让我看看你在做什么。");
};

class OpenAiCompatibleClient : public QObject
{
    Q_OBJECT

public:
    explicit OpenAiCompatibleClient(OpenAiCompatibleConfig config, QNetworkAccessManager *network, QObject *parent = nullptr);

    void requestChatCompletion(const QByteArray &payload);
    void cancelCurrentRequest();

    static QString resolveApiKey(const QString &provider, const QString &explicitApiKey);
    static QString normalizeBaseUrl(const QString &baseUrl);
    static QString extractResponseText(const QByteArray &payload);
    static QString buildFailureMessage(int statusCode, const QString &errorString, const QByteArray &responseBody);

Q_SIGNALS:
    void completionReady(const QString &text);
    void completionFailed(const QString &error);

private:
    OpenAiCompatibleConfig m_config;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_activeReply;
};
