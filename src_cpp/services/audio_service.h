#pragma once

#include <QStringList>

#include <QDir>
#include <QList>
#include <QProcess>

#include "services/service_contracts.h"

class QAudioOutput;
class QMediaPlayer;

class QtAudioService final : public AudioService
{
    Q_OBJECT

public:
    explicit QtAudioService(
        const QString &notificationFilePath = QString(),
        const QString &cacheDirPath = QString(),
        QObject *parent = nullptr);
    ~QtAudioService() override;

public Q_SLOTS:
    void speak(const QString &text) override;
    void speakRequest(const AudioPlaybackRequest &request) override;
    void setTtsProvider(const QString &provider) override;
    void configureVoice(const QString &voice, const QString &rate) override;
    void setVolume(double volume) override;
    void setCacheEnabled(bool enabled) override;
    void interrupt() override;
    void shutdown() override;

private Q_SLOTS:
    void handleMediaStatusChanged();
    void handleTtsFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void playNextIfIdle();
    void finishCurrentPlayback();
    QString cacheFilePathForText(const QString &text) const;
    QString transientFilePath() const;
    void startSynthesis(const AudioPlaybackRequest &request, const QString &outputFilePath);
    void playResolvedAudio(const AudioPlaybackRequest &request, const QString &audioFilePath, bool deleteAfterPlayback = false);
    int normalizePriority(int priority) const;

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QProcess *m_ttsProcess = nullptr;
    QString m_notificationFilePath;
    QString m_cacheDirPath;
    struct PendingSpeechRequest
    {
        AudioPlaybackRequest request;
        int order = 0;
    };

    QList<PendingSpeechRequest> m_pendingRequests;
    QStringList m_spokenHistory;
    QString m_currentPlaybackText;
    QString m_currentSynthesisOutputFilePath;
    QString m_currentTransientAudioFilePath;
    QString m_edgeTtsExecutablePath;
    QString m_ttsProvider = QStringLiteral("edge");
    QString m_voice = QStringLiteral("zh-CN-XiaoxiaoNeural");
    QString m_voiceRate = QStringLiteral("+0%");
    int m_currentPlaybackPriority = static_cast<int>(AudioPlaybackPriority::Normal);
    int m_sequenceCounter = 0;
    bool m_playbackActive = false;
    bool m_cacheEnabled = true;
};
