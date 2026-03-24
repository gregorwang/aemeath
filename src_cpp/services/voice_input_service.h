#pragma once

#include <QByteArray>
#include <QPointer>

#include "services/service_contracts.h"

class QAudioSource;
class QBuffer;
class QHttpMultiPart;
class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class OpenAiVoiceInputService final : public VoiceInputService
{
    Q_OBJECT

public:
    explicit OpenAiVoiceInputService(QObject *parent = nullptr);
    ~OpenAiVoiceInputService() override;

    void configure(const VoiceInputConfig &config) override;
    void startContinuousListening() override;
    void startPushToTalkOnce() override;
    void stop() override;

    static QString normalizeRecognitionProvider(const QString &provider);
    static QString normalizeWhisperLanguage(const QString &language);
    static QString defaultModelForProvider(const QString &provider);
    static QString defaultBaseUrlForProvider(const QString &provider, const QString &llmFallbackBaseUrl);
    static QString normalizeBaseUrl(const QString &provider, const QString &baseUrl, const QString &fallbackBaseUrl);
    static QString buildTranscriptionEndpoint(const QString &provider, const QString &baseUrl, const QString &fallbackBaseUrl);
    static QString extractTranscriptionText(const QByteArray &payload);
    static QString buildHttpStatusHint(
        int statusCode,
        const QString &baseUrl,
        const QString &responseBody,
        const QString &provider = QStringLiteral("openai_whisper"));
    static QString buildFailureMessage(
        int statusCode,
        const QString &errorString,
        const QByteArray &responseBody,
        const QString &provider,
        const QString &baseUrl);

private:
    QString validatePushToTalkConfig() const;
    static bool isXaiBaseUrl(const QString &baseUrl);
    static bool isZhipuBaseUrl(const QString &baseUrl);
    QByteArray buildWaveFile(const QByteArray &pcmData, int sampleRate, int channelCount, int bitsPerSample) const;
    QString validateContinuousListeningConfig() const;
    void degradeContinuousListening(const QString &message);
    void startAudioCapture(int durationMs, const QString &source);
    void finishAudioCapture();
    void abortNetworkReply();
    void requestTranscription(const QByteArray &wavePayload, const QString &source);
    QString resolveApiKey() const;
    bool shouldEmitWakeupTranscript(const QString &transcript) const;
    void scheduleNextContinuousCapture(int delayMs);

    VoiceInputConfig m_config;
    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_activeReply;
    QAudioSource *m_audioSource = nullptr;
    QBuffer *m_captureBuffer = nullptr;
    QTimer *m_captureTimer = nullptr;
    QByteArray m_recordedPcm;
    int m_captureSampleRate = 16000;
    int m_captureChannelCount = 1;
    int m_captureBitsPerSample = 16;
    bool m_captureActive = false;
    bool m_busy = false;
    bool m_continuousListeningRequested = false;
    QString m_activeCaptureSource;
};
