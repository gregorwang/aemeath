#include "services/voice_input_service.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QBuffer>
#include <QByteArray>
#include <QHttpMultiPart>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "services/openai_compatible_client.h"
#include "runtime/voice_command_matcher.h"

namespace {

constexpr int kPushToTalkListenMs = 6000;
constexpr int kContinuousListenMs = 3500;
constexpr int kContinuousRetryDelayMs = 800;
constexpr int kMinimumRecordedBytes = 2048;

QByteArray littleEndian32(quint32 value)
{
    QByteArray bytes(4, Qt::Uninitialized);
    bytes[0] = static_cast<char>(value & 0xff);
    bytes[1] = static_cast<char>((value >> 8) & 0xff);
    bytes[2] = static_cast<char>((value >> 16) & 0xff);
    bytes[3] = static_cast<char>((value >> 24) & 0xff);
    return bytes;
}

QByteArray littleEndian16(quint16 value)
{
    QByteArray bytes(2, Qt::Uninitialized);
    bytes[0] = static_cast<char>(value & 0xff);
    bytes[1] = static_cast<char>((value >> 8) & 0xff);
    return bytes;
}

}

OpenAiVoiceInputService::OpenAiVoiceInputService(QObject *parent)
    : VoiceInputService(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_captureTimer(new QTimer(this))
{
    m_captureTimer->setSingleShot(true);
    connect(m_captureTimer, &QTimer::timeout, this, &OpenAiVoiceInputService::finishAudioCapture);
}

OpenAiVoiceInputService::~OpenAiVoiceInputService()
{
    stop();
}

void OpenAiVoiceInputService::configure(const VoiceInputConfig &config)
{
    m_config = config;
}

void OpenAiVoiceInputService::startContinuousListening()
{
    if (!m_config.microphoneEnabled) {
        Q_EMIT listenerWarning(QStringLiteral("麦克风未启用，连续语音唤醒不会启动。"));
        return;
    }
    if (m_config.voiceInputMode.trimmed().compare(QStringLiteral("continuous"), Qt::CaseInsensitive) != 0) {
        return;
    }
    if (!m_config.wakeupEnabled) {
        return;
    }
    const QString validationError = validateContinuousListeningConfig();
    if (!validationError.isEmpty()) {
        degradeContinuousListening(validationError);
        return;
    }
    m_continuousListeningRequested = true;
    if (!m_busy) {
        startAudioCapture(kContinuousListenMs, QStringLiteral("wakeup"));
    }
}

void OpenAiVoiceInputService::startPushToTalkOnce()
{
    if (!m_config.microphoneEnabled) {
        Q_EMIT listenerError(QStringLiteral("麦克风未启用，请先在设置中打开。"));
        return;
    }
    if (m_config.voiceInputMode.trimmed().compare(QStringLiteral("push_to_talk"), Qt::CaseInsensitive) != 0) {
        Q_EMIT listenerWarning(QStringLiteral("当前不是 push_to_talk 模式，Ctrl+B 单次转写未启用。"));
        return;
    }
    if (m_busy) {
        return;
    }

    const QString validationError = validatePushToTalkConfig();
    if (!validationError.isEmpty()) {
        Q_EMIT listenerError(validationError);
        return;
    }

    startAudioCapture(kPushToTalkListenMs, QStringLiteral("push_to_talk"));
}

void OpenAiVoiceInputService::stop()
{
    m_continuousListeningRequested = false;
    if (m_captureTimer) {
        m_captureTimer->stop();
    }
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
    }
    if (m_captureBuffer) {
        m_captureBuffer->close();
        delete m_captureBuffer;
        m_captureBuffer = nullptr;
    }
    abortNetworkReply();
    if (m_captureActive) {
        m_captureActive = false;
        Q_EMIT listenerStateChanged(false);
        Q_EMIT captureStateChanged(false, m_activeCaptureSource);
    }
    m_busy = false;
}

QString OpenAiVoiceInputService::normalizeRecognitionProvider(const QString &provider)
{
    const QString normalized = provider.trimmed().toLower();
    if (normalized == QStringLiteral("openai") || normalized == QStringLiteral("whisper")) {
        return QStringLiteral("openai_whisper");
    }
    if (normalized == QStringLiteral("google_webspeech")) {
        return QStringLiteral("zhipu_asr");
    }
    if (normalized == QStringLiteral("zhipu")) {
        return QStringLiteral("zhipu_asr");
    }
    if (normalized == QStringLiteral("xai")) {
        return QStringLiteral("xai_realtime");
    }
    if (normalized == QStringLiteral("openai_whisper")
        || normalized == QStringLiteral("google")
        || normalized == QStringLiteral("zhipu_asr")
        || normalized == QStringLiteral("xai_realtime")) {
        if (normalized == QStringLiteral("google")) {
            return QStringLiteral("zhipu_asr");
        }
        return normalized;
    }
    return QStringLiteral("zhipu_asr");
}

QString OpenAiVoiceInputService::normalizeWhisperLanguage(const QString &language)
{
    const QString cleaned = language.trimmed();
    if (cleaned.isEmpty()) {
        return {};
    }
    const QString base = cleaned.section(QChar('-'), 0, 0).toLower();
    if (base.size() != 2) {
        return {};
    }
    for (const QChar ch : base) {
        if (!ch.isLetter()) {
            return {};
        }
    }
    return base;
}

QString OpenAiVoiceInputService::defaultModelForProvider(const QString &provider)
{
    const QString normalizedProvider = normalizeRecognitionProvider(provider);
    if (normalizedProvider == QStringLiteral("zhipu_asr")) {
        return QStringLiteral("glm-asr-2512");
    }
    if (normalizedProvider == QStringLiteral("xai_realtime")) {
        return QStringLiteral("grok-2-mini-transcribe");
    }
    if (normalizedProvider == QStringLiteral("openai_whisper")) {
        return QStringLiteral("whisper-1");
    }
    return {};
}

QString OpenAiVoiceInputService::defaultBaseUrlForProvider(const QString &provider, const QString &llmFallbackBaseUrl)
{
    const QString normalizedProvider = normalizeRecognitionProvider(provider);
    if (normalizedProvider == QStringLiteral("zhipu_asr")) {
        return QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions");
    }
    const QString fallback = llmFallbackBaseUrl.trimmed();
    if (!fallback.isEmpty()) {
        return fallback;
    }
    if (normalizedProvider == QStringLiteral("xai_realtime")) {
        return QStringLiteral("https://api.x.ai/v1");
    }
    return QStringLiteral("https://api.openai.com/v1");
}

QString OpenAiVoiceInputService::validateContinuousListeningConfig() const
{
    if (m_config.offlineMode) {
        return QStringLiteral("离线模式已启用，连续语音唤醒不可用。");
    }
    const QString provider = normalizeRecognitionProvider(m_config.asrProvider);
    if (resolveApiKey().trimmed().isEmpty()) {
        return QStringLiteral("ASR API Key 未配置，连续语音唤醒已暂停。");
    }

    const QString endpoint = buildTranscriptionEndpoint(provider, m_config.asrBaseUrl, m_config.llmBaseUrl);
    const QUrl url(endpoint);
    if (!url.isValid()) {
        return QStringLiteral("ASR Base URL 无效，连续语音唤醒已暂停。");
    }

    return {};
}

QString OpenAiVoiceInputService::validatePushToTalkConfig() const
{
    if (m_config.offlineMode) {
        return QStringLiteral("离线模式已启用，语音转写不可用。");
    }
    const QString provider = normalizeRecognitionProvider(m_config.asrProvider);
    if (resolveApiKey().trimmed().isEmpty()) {
        return QStringLiteral("ASR API Key 未配置。");
    }
    const QString endpoint = buildTranscriptionEndpoint(provider, m_config.asrBaseUrl, m_config.llmBaseUrl);
    const QUrl url(endpoint);
    if (!url.isValid()) {
        return QStringLiteral("ASR Base URL 无效。");
    }
    return {};
}

QString OpenAiVoiceInputService::normalizeBaseUrl(const QString &provider, const QString &baseUrl, const QString &fallbackBaseUrl)
{
    const QString normalizedProvider = normalizeRecognitionProvider(provider);
    QString candidate = baseUrl.trimmed();
    if (candidate.isEmpty()) {
        candidate = defaultBaseUrlForProvider(normalizedProvider, fallbackBaseUrl);
    }

    if (normalizedProvider == QStringLiteral("zhipu_asr")) {
        if (candidate.endsWith(QStringLiteral("/audio/transcriptions"))) {
            return candidate;
        }
        while (candidate.endsWith('/')) {
            candidate.chop(1);
        }
        if (!candidate.endsWith(QStringLiteral("/api/paas/v4"))) {
            return QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions");
        }
        return candidate + QStringLiteral("/audio/transcriptions");
    }

    return OpenAiCompatibleClient::normalizeBaseUrl(candidate);
}

QString OpenAiVoiceInputService::buildTranscriptionEndpoint(const QString &provider, const QString &baseUrl, const QString &fallbackBaseUrl)
{
    const QString normalizedProvider = normalizeRecognitionProvider(provider);
    const QString normalizedBase = normalizeBaseUrl(provider, baseUrl, fallbackBaseUrl);
    if (normalizedProvider == QStringLiteral("zhipu_asr")) {
        return normalizedBase;
    }
    return normalizedBase + QStringLiteral("/audio/transcriptions");
}

void OpenAiVoiceInputService::degradeContinuousListening(const QString &message)
{
    m_continuousListeningRequested = false;
    if (m_captureTimer) {
        m_captureTimer->stop();
    }
    if (m_captureActive) {
        finishAudioCapture();
    }
    m_busy = false;
    Q_EMIT continuousListeningDegraded(message);
    Q_EMIT listenerWarning(message);
}

QString OpenAiVoiceInputService::extractTranscriptionText(const QByteArray &payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (document.isNull()) {
        return QString::fromUtf8(payload).trimmed();
    }
    if (!document.isObject()) {
        return {};
    }
    const QJsonObject object = document.object();
    const QJsonValue textValue = object.value(QStringLiteral("text"));
    if (textValue.isString()) {
        return textValue.toString();
    }
    const QJsonObject dataObject = object.value(QStringLiteral("data")).toObject();
    const QJsonValue nestedText = dataObject.value(QStringLiteral("text"));
    if (nestedText.isString()) {
        return nestedText.toString();
    }
    return {};
}

QString OpenAiVoiceInputService::buildHttpStatusHint(
    int statusCode,
    const QString &baseUrl,
    const QString &responseBody,
    const QString &provider)
{
    const QString status = QString::number(statusCode);
    QString bodyPreview = responseBody.trimmed();
    bodyPreview.replace(QChar('\r'), QChar(' '));
    bodyPreview.replace(QChar('\n'), QChar(' '));
    bodyPreview = bodyPreview.left(180);

    QString hint;
    if (status == QStringLiteral("401") || status == QStringLiteral("403")) {
        hint = QStringLiteral("API Key 无效、过期或无此模型权限。");
    } else if (status == QStringLiteral("404")) {
        hint = normalizeRecognitionProvider(provider) == QStringLiteral("zhipu_asr") || isZhipuBaseUrl(baseUrl)
            ? QStringLiteral("服务端不存在该接口或模型。请确认路径为 /api/paas/v4/audio/transcriptions。")
            : QStringLiteral("服务端不存在该接口或模型。请确认服务支持 /v1/audio/transcriptions。");
    } else if (status == QStringLiteral("429")) {
        hint = QStringLiteral("请求过快或额度不足。请稍后重试。");
    } else {
        hint = QStringLiteral("请检查 API Key、模型名与网络连通性。");
    }

    if (isXaiBaseUrl(baseUrl)) {
        if (normalizeRecognitionProvider(provider) == QStringLiteral("xai_realtime")) {
            hint += QStringLiteral(" 检测到 xAI 域名；请确认使用支持转写的 xAI 模型并检查鉴权配置。");
        } else {
            hint += QStringLiteral(" 检测到 xAI 域名；若使用 xAI，请将 ASR 提供商改为 xai_realtime。");
        }
    }
    if (isZhipuBaseUrl(baseUrl)) {
        hint += QStringLiteral(" 检测到智谱域名；建议使用 glm-asr-2512，且路径应为 /api/paas/v4/audio/transcriptions。");
    }
    if (!bodyPreview.isEmpty()) {
        hint += QStringLiteral(" 服务端返回: %1").arg(bodyPreview);
    }
    return hint;
}

QString OpenAiVoiceInputService::buildFailureMessage(
    int statusCode,
    const QString &errorString,
    const QByteArray &responseBody,
    const QString &provider,
    const QString &baseUrl)
{
    const QString normalizedProvider = normalizeRecognitionProvider(provider);
    const QString providerLabel = normalizedProvider == QStringLiteral("zhipu_asr")
        ? QStringLiteral("Zhipu ASR")
        : normalizedProvider == QStringLiteral("xai_realtime")
            ? QStringLiteral("xAI Realtime")
            : QStringLiteral("ASR");

    QString message = QStringLiteral("%1 请求失败").arg(providerLabel);
    if (statusCode > 0) {
        message += QStringLiteral(" (HTTP %1)").arg(statusCode);
    }
    if (!errorString.trimmed().isEmpty()) {
        message += QStringLiteral("：%1").arg(errorString.trimmed());
    } else {
        message += QStringLiteral("。");
    }
    message += QStringLiteral(" ");
    message += buildHttpStatusHint(statusCode, baseUrl, QString::fromUtf8(responseBody), normalizedProvider);
    return message.simplified();
}

bool OpenAiVoiceInputService::isXaiBaseUrl(const QString &baseUrl)
{
    const QUrl url(baseUrl.trimmed());
    return url.isValid() && url.host().trimmed().toLower().endsWith(QStringLiteral("x.ai"));
}

bool OpenAiVoiceInputService::isZhipuBaseUrl(const QString &baseUrl)
{
    const QUrl url(baseUrl.trimmed());
    return url.isValid() && url.host().trimmed().toLower().endsWith(QStringLiteral("bigmodel.cn"));
}

QByteArray OpenAiVoiceInputService::buildWaveFile(
    const QByteArray &pcmData,
    int sampleRate,
    int channelCount,
    int bitsPerSample) const
{
    const quint32 byteRate = static_cast<quint32>(sampleRate * channelCount * bitsPerSample / 8);
    const quint16 blockAlign = static_cast<quint16>(channelCount * bitsPerSample / 8);
    QByteArray wave;
    wave.reserve(44 + pcmData.size());
    wave.append("RIFF", 4);
    wave.append(littleEndian32(static_cast<quint32>(36 + pcmData.size())));
    wave.append("WAVE", 4);
    wave.append("fmt ", 4);
    wave.append(littleEndian32(16));
    wave.append(littleEndian16(1));
    wave.append(littleEndian16(static_cast<quint16>(channelCount)));
    wave.append(littleEndian32(static_cast<quint32>(sampleRate)));
    wave.append(littleEndian32(byteRate));
    wave.append(littleEndian16(blockAlign));
    wave.append(littleEndian16(static_cast<quint16>(bitsPerSample)));
    wave.append("data", 4);
    wave.append(littleEndian32(static_cast<quint32>(pcmData.size())));
    wave.append(pcmData);
    return wave;
}

void OpenAiVoiceInputService::startAudioCapture(int durationMs, const QString &source)
{
    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        if (source == QStringLiteral("wakeup")) {
            degradeContinuousListening(QStringLiteral("当前系统未检测到可用麦克风，连续语音唤醒已暂停。"));
        } else {
            Q_EMIT listenerError(QStringLiteral("当前系统未检测到可用麦克风。"));
        }
        return;
    }

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    if (!inputDevice.isFormatSupported(format)) {
        if (source == QStringLiteral("wakeup")) {
            degradeContinuousListening(QStringLiteral("当前麦克风不支持 16kHz PCM16 录音，连续语音唤醒已暂停。"));
        } else {
            Q_EMIT listenerError(QStringLiteral("当前麦克风不支持 16kHz PCM16 录音，暂时无法进行原生转写。"));
        }
        return;
    }

    abortNetworkReply();
    if (m_captureTimer) {
        m_captureTimer->stop();
    }
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
    }
    if (m_captureBuffer) {
        m_captureBuffer->close();
        delete m_captureBuffer;
        m_captureBuffer = nullptr;
    }

    m_recordedPcm.clear();
    m_captureSampleRate = format.sampleRate();
    m_captureChannelCount = format.channelCount();
    m_captureBitsPerSample = format.bytesPerSample() * 8;
    m_captureBuffer = new QBuffer(&m_recordedPcm, this);
    m_captureBuffer->open(QIODevice::WriteOnly);
    m_audioSource = new QAudioSource(inputDevice, format, this);
    m_audioSource->setBufferSize(format.sampleRate() * format.bytesPerFrame() * 2);
    m_audioSource->start(m_captureBuffer);
    m_activeCaptureSource = source;
    m_captureActive = true;
    m_busy = true;
    Q_EMIT listenerStateChanged(true);
    Q_EMIT captureStateChanged(true, source);
    m_captureTimer->start(durationMs);
}

void OpenAiVoiceInputService::finishAudioCapture()
{
    if (!m_captureActive) {
        return;
    }
    const QString captureSource = m_activeCaptureSource;
    if (m_audioSource) {
        m_audioSource->stop();
    }
    if (m_captureBuffer) {
        m_captureBuffer->close();
    }
    m_captureActive = false;
    Q_EMIT listenerStateChanged(false);
    Q_EMIT captureStateChanged(false, captureSource);

    if (m_recordedPcm.size() < kMinimumRecordedBytes) {
        m_busy = false;
        if (captureSource == QStringLiteral("push_to_talk")) {
            Q_EMIT listenerError(QStringLiteral("未识别到有效语音，请重试。"));
        } else if (m_continuousListeningRequested) {
            scheduleNextContinuousCapture(kContinuousRetryDelayMs);
        }
        return;
    }

    requestTranscription(buildWaveFile(
        m_recordedPcm,
        m_captureSampleRate,
        m_captureChannelCount,
        m_captureBitsPerSample),
        captureSource);
}

void OpenAiVoiceInputService::abortNetworkReply()
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

void OpenAiVoiceInputService::requestTranscription(const QByteArray &wavePayload, const QString &source)
{
    const QString provider = normalizeRecognitionProvider(m_config.asrProvider);
    const QString apiKey = resolveApiKey();
    if (apiKey.trimmed().isEmpty()) {
        m_busy = false;
        if (source == QStringLiteral("push_to_talk")) {
            Q_EMIT listenerError(QStringLiteral("ASR API Key 未配置。"));
        } else {
            degradeContinuousListening(QStringLiteral("ASR API Key 未配置，连续语音唤醒已暂停。"));
        }
        return;
    }

    const QString endpoint = buildTranscriptionEndpoint(provider, m_config.asrBaseUrl, m_config.llmBaseUrl);
    const QUrl url(endpoint);
    if (!url.isValid()) {
        m_busy = false;
        if (source == QStringLiteral("push_to_talk")) {
            Q_EMIT listenerError(QStringLiteral("ASR Base URL 无效。"));
        } else {
            degradeContinuousListening(QStringLiteral("ASR Base URL 无效，连续语音唤醒已暂停。"));
        }
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);

    auto appendTextPart = [multiPart](const QByteArray &name, const QString &value) {
        if (value.trimmed().isEmpty()) {
            return;
        }
        QHttpPart part;
        part.setHeader(
            QNetworkRequest::ContentDispositionHeader,
            QStringLiteral("form-data; name=\"%1\"").arg(QString::fromUtf8(name)));
        part.setBody(value.toUtf8());
        multiPart->append(part);
    };

    const QString effectiveModel = m_config.asrModel.trimmed().isEmpty()
        ? defaultModelForProvider(provider)
        : m_config.asrModel.trimmed();
    appendTextPart("model", effectiveModel);
    appendTextPart("prompt", m_config.asrPrompt);
    appendTextPart("temperature", QString::number(m_config.asrTemperature, 'f', 2));
    appendTextPart("language", normalizeWhisperLanguage(m_config.wakeupLanguage));

    auto *fileBuffer = new QBuffer(multiPart);
    fileBuffer->setData(wavePayload);
    fileBuffer->open(QIODevice::ReadOnly);

    QHttpPart filePart;
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"speech.wav\"")));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("audio/wav")));
    filePart.setBodyDevice(fileBuffer);
    multiPart->append(filePart);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());

    abortNetworkReply();
    QNetworkReply *reply = m_network->post(request, multiPart);
    multiPart->setParent(reply);
    m_activeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, source, provider, endpoint]() {
        if (m_activeReply == reply) {
            m_activeReply.clear();
        }
        reply->deleteLater();

        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError) {
                return;
            }
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString error = buildFailureMessage(
                statusCode,
                reply->errorString(),
                reply->readAll(),
                provider,
                endpoint);
            if (source == QStringLiteral("push_to_talk")) {
                Q_EMIT listenerError(error);
            } else {
                Q_EMIT listenerWarning(QStringLiteral("连续语音识别失败，稍后自动重试。"));
                if (m_continuousListeningRequested) {
                    scheduleNextContinuousCapture(kContinuousRetryDelayMs);
                }
            }
            return;
        }

        const QString transcript = extractTranscriptionText(reply->readAll()).trimmed();
        if (transcript.isEmpty()) {
            if (source == QStringLiteral("push_to_talk")) {
                Q_EMIT listenerError(QStringLiteral("未识别到有效语音，请重试。"));
            } else if (m_continuousListeningRequested) {
                scheduleNextContinuousCapture(kContinuousRetryDelayMs);
            }
            return;
        }
        if (source == QStringLiteral("push_to_talk")) {
            Q_EMIT transcriptReady(transcript, source);
        } else if (shouldEmitWakeupTranscript(transcript)) {
            Q_EMIT transcriptReady(transcript, source);
        }
        if (source == QStringLiteral("wakeup") && m_continuousListeningRequested) {
            scheduleNextContinuousCapture(kContinuousRetryDelayMs);
        }
    });
}

QString OpenAiVoiceInputService::resolveApiKey() const
{
    const QString explicitKey = !m_config.asrApiKey.trimmed().isEmpty()
        ? m_config.asrApiKey
        : m_config.llmApiKey;
    return OpenAiCompatibleClient::resolveApiKey(m_config.asrProvider, explicitKey);
}

bool OpenAiVoiceInputService::shouldEmitWakeupTranscript(const QString &transcript) const
{
    const QString normalizedTranscript = VoiceCommandMatcher::normalizeText(transcript);
    if (normalizedTranscript.isEmpty()) {
        return false;
    }
    for (const QString &phrase : m_config.wakeupPhrases) {
        const QString normalizedPhrase = VoiceCommandMatcher::normalizeText(phrase);
        if (!normalizedPhrase.isEmpty() && normalizedTranscript.contains(normalizedPhrase)) {
            return true;
        }
    }
    return false;
}

void OpenAiVoiceInputService::scheduleNextContinuousCapture(int delayMs)
{
    if (!m_continuousListeningRequested) {
        return;
    }
    QTimer::singleShot(delayMs, this, [this]() {
        if (!m_continuousListeningRequested || m_busy) {
            return;
        }
        startAudioCapture(kContinuousListenMs, QStringLiteral("wakeup"));
    });
}
