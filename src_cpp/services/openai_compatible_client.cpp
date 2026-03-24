#include "services/openai_compatible_client.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include <utility>

OpenAiCompatibleClient::OpenAiCompatibleClient(OpenAiCompatibleConfig config, QNetworkAccessManager *network, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_network(network)
{
}

QString OpenAiCompatibleClient::resolveApiKey(const QString &provider, const QString &explicitApiKey)
{
    const QString cleanedExplicit = explicitApiKey.trimmed();
    if (!cleanedExplicit.isEmpty()) {
        return cleanedExplicit;
    }

    QStringList candidates;
    const QString normalizedProvider = provider.trimmed().toLower();
    if (normalizedProvider == QStringLiteral("xai")) {
        candidates << QStringLiteral("XAI_API_KEY");
    } else if (normalizedProvider == QStringLiteral("deepseek")) {
        candidates << QStringLiteral("DEEPSEEK_API_KEY");
    } else if (normalizedProvider == QStringLiteral("kimi")) {
        candidates << QStringLiteral("KIMI_API_KEY") << QStringLiteral("MOONSHOT_API_KEY");
    } else if (normalizedProvider == QStringLiteral("zhipu")) {
        candidates << QStringLiteral("ZHIPU_API_KEY");
    } else if (normalizedProvider == QStringLiteral("doubao")) {
        candidates << QStringLiteral("DOUBAO_API_KEY") << QStringLiteral("ARK_API_KEY");
    }

    candidates << QStringLiteral("OPENAI_API_KEY")
               << QStringLiteral("POLOAI_API_KEY")
               << QStringLiteral("XAI_API_KEY");

    QSet<QString> seen;
    for (const QString &name : candidates) {
        if (seen.contains(name)) {
            continue;
        }
        seen.insert(name);
        const QString value = qEnvironmentVariable(name.toUtf8().constData()).trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

void OpenAiCompatibleClient::requestChatCompletion(const QByteArray &payload)
{
    if (!m_network) {
        Q_EMIT completionFailed(QStringLiteral("网络管理器未初始化。"));
        return;
    }
    m_config.apiKey = resolveApiKey(m_config.provider, m_config.apiKey);
    if (m_config.apiKey.trimmed().isEmpty()) {
        Q_EMIT completionFailed(QStringLiteral("LLM API Key 未配置。"));
        return;
    }

    const QString normalizedBase = normalizeBaseUrl(m_config.baseUrl);
    const QUrl url(normalizedBase + QStringLiteral("/chat/completions"));
    if (!url.isValid()) {
        Q_EMIT completionFailed(QStringLiteral("LLM Base URL 无效。"));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_config.apiKey.toUtf8());

    cancelCurrentRequest();
    QNetworkReply *reply = m_network->post(request, payload);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_activeReply == reply) {
            m_activeReply.clear();
        }
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                return;
            }
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            Q_EMIT completionFailed(buildFailureMessage(statusCode, reply->errorString(), reply->readAll()));
            return;
        }

        const QString text = extractResponseText(reply->readAll()).trimmed();
        if (text.isEmpty()) {
            Q_EMIT completionFailed(QStringLiteral("LLM 返回为空。"));
            return;
        }
        Q_EMIT completionReady(text);
    });
}

void OpenAiCompatibleClient::cancelCurrentRequest()
{
    if (!m_activeReply) {
        return;
    }
    QNetworkReply *reply = m_activeReply;
    m_activeReply.clear();
    if (reply->isRunning()) {
        reply->abort();
    }
}

QString OpenAiCompatibleClient::normalizeBaseUrl(const QString &baseUrl)
{
    QString normalized = baseUrl.trimmed().trimmed();
    if (normalized.isEmpty()) {
        normalized = QStringLiteral("https://api.openai.com/v1");
    }

    while (normalized.endsWith('/')) {
        normalized.chop(1);
    }

    const QStringList suffixes = {
        QStringLiteral("/chat/completions"),
        QStringLiteral("/responses"),
        QStringLiteral("/audio/transcriptions"),
    };
    for (const QString &suffix : suffixes) {
        if (normalized.endsWith(suffix)) {
            normalized.chop(suffix.size());
            break;
        }
    }

    const QUrl parsed(normalized);
    if (parsed.isValid()
        && (parsed.scheme() == QStringLiteral("http") || parsed.scheme() == QStringLiteral("https"))
        && !parsed.host().isEmpty()) {
        QString path = parsed.path();
        while (path.endsWith('/')) {
            path.chop(1);
        }
        if (path.isEmpty()) {
            path = QStringLiteral("/v1");
        }

        QUrl rebuilt;
        rebuilt.setScheme(parsed.scheme());
        rebuilt.setHost(parsed.host());
        rebuilt.setPort(parsed.port());
        rebuilt.setPath(path);
        return rebuilt.toString(QUrl::RemoveQuery | QUrl::RemoveFragment).trimmed();
    }
    return normalized;
}

QString OpenAiCompatibleClient::extractResponseText(const QByteArray &payload)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return {};
    }

    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty() || !choices.first().isObject()) {
        return {};
    }

    const QJsonObject message = choices.first().toObject().value(QStringLiteral("message")).toObject();
    if (message.isEmpty()) {
        return {};
    }

    const QJsonValue content = message.value(QStringLiteral("content"));
    if (content.isString()) {
        return content.toString();
    }
    if (content.isArray()) {
        QStringList parts;
        const QJsonArray array = content.toArray();
        for (const QJsonValue &item : array) {
            if (!item.isObject()) {
                continue;
            }
            const QString text = item.toObject().value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                parts.append(text);
            }
        }
        return parts.join(QString());
    }
    return {};
}

QString OpenAiCompatibleClient::buildFailureMessage(int statusCode, const QString &errorString, const QByteArray &responseBody)
{
    QString prefix;
    switch (statusCode) {
    case 401:
    case 403:
        prefix = QStringLiteral("鉴权失败，请检查 API Key 或模型权限。");
        break;
    case 404:
        prefix = QStringLiteral("接口不存在，请检查 Base URL 或服务兼容性。");
        break;
    case 429:
        prefix = QStringLiteral("请求过快或额度不足。");
        break;
    case 500:
    case 502:
    case 503:
    case 504:
        prefix = QStringLiteral("服务端暂时不可用。");
        break;
    default:
        prefix = QStringLiteral("评论请求失败。");
        break;
    }

    const QString bodyPreview = QString::fromUtf8(responseBody).simplified().left(180);
    if (!bodyPreview.isEmpty()) {
        return QStringLiteral("%1 %2 服务端返回: %3").arg(prefix, errorString, bodyPreview);
    }
    if (!errorString.trimmed().isEmpty()) {
        return QStringLiteral("%1 %2").arg(prefix, errorString);
    }
    return prefix;
}
