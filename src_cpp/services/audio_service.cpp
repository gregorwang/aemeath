#include "services/audio_service.h"

#include <QAudioOutput>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <QMediaPlayer>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <QtGlobal>

QtAudioService::QtAudioService(
    const QString &notificationFilePath,
    const QString &cacheDirPath,
    QObject *parent)
    : AudioService(parent)
    , m_player(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_ttsProcess(new QProcess(this))
    , m_notificationFilePath(notificationFilePath)
    , m_cacheDirPath(cacheDirPath)
{
    qRegisterMetaType<AudioPlaybackRequest>("AudioPlaybackRequest");
    m_audioOutput->setVolume(0.8f);
    m_player->setAudioOutput(m_audioOutput);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &QtAudioService::handleMediaStatusChanged);
    connect(
        m_ttsProcess,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this,
        &QtAudioService::handleTtsFinished);
    m_edgeTtsExecutablePath = QStandardPaths::findExecutable(QStringLiteral("edge-tts"));
    if (!m_cacheDirPath.trimmed().isEmpty()) {
        QDir().mkpath(m_cacheDirPath);
    }
}

QtAudioService::~QtAudioService() = default;

void QtAudioService::speak(const QString &text)
{
    speakRequest(AudioPlaybackRequest{
        text,
        static_cast<int>(AudioPlaybackPriority::Normal),
        false,
        QString(),
    });
}

void QtAudioService::setTtsProvider(const QString &provider)
{
    const QString normalized = provider.trimmed().toLower();
    m_ttsProvider = normalized.isEmpty() || normalized != QStringLiteral("edge")
        ? QStringLiteral("edge")
        : normalized;
}

void QtAudioService::configureVoice(const QString &voice, const QString &rate)
{
    const QString cleanedVoice = voice.trimmed();
    const QString cleanedRate = rate.trimmed();
    if (!cleanedVoice.isEmpty()) {
        m_voice = cleanedVoice;
    }
    if (!cleanedRate.isEmpty()) {
        m_voiceRate = cleanedRate;
    }
}

void QtAudioService::setVolume(double volume)
{
    if (!m_audioOutput) {
        return;
    }
    m_audioOutput->setVolume(static_cast<float>(qBound(0.0, volume, 1.0)));
}

void QtAudioService::setCacheEnabled(bool enabled)
{
    m_cacheEnabled = enabled;
}

void QtAudioService::speakRequest(const AudioPlaybackRequest &request)
{
    const QString cleaned = request.text.trimmed();
    if (cleaned.isEmpty()) {
        return;
    }

    AudioPlaybackRequest normalizedRequest = request;
    normalizedRequest.text = cleaned;
    normalizedRequest.priority = normalizePriority(request.priority);
    normalizedRequest.interrupt = request.interrupt
        || normalizedRequest.priority == static_cast<int>(AudioPlaybackPriority::Critical);

    const bool serviceBusy = m_player->playbackState() == QMediaPlayer::PlayingState
        || m_ttsProcess->state() != QProcess::NotRunning
        || m_playbackActive
        || !m_pendingRequests.isEmpty();
    if (normalizedRequest.priority >= static_cast<int>(AudioPlaybackPriority::Low) && serviceBusy) {
        qInfo().noquote() << "[AudioService][drop-low-priority]" << cleaned;
        return;
    }

    if (normalizedRequest.interrupt) {
        interrupt();
    }

    qInfo().noquote() << "[AudioService]" << cleaned << "(priority=" << normalizedRequest.priority << ")";
    ++m_sequenceCounter;
    m_pendingRequests.append(PendingSpeechRequest{ normalizedRequest, m_sequenceCounter });
    std::stable_sort(
        m_pendingRequests.begin(),
        m_pendingRequests.end(),
        [](const PendingSpeechRequest &left, const PendingSpeechRequest &right) {
            if (left.request.priority != right.request.priority) {
                return left.request.priority < right.request.priority;
            }
            return left.order < right.order;
        });
    playNextIfIdle();
}

void QtAudioService::interrupt()
{
    m_pendingRequests.clear();
    if (m_ttsProcess->state() != QProcess::NotRunning) {
        m_ttsProcess->kill();
        m_ttsProcess->waitForFinished(1000);
    }
    m_currentSynthesisOutputFilePath.clear();
    if (!m_currentTransientAudioFilePath.isEmpty()) {
        QFile::remove(m_currentTransientAudioFilePath);
        m_currentTransientAudioFilePath.clear();
    }
    m_currentPlaybackText.clear();
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->stop();
    }
    finishCurrentPlayback();
}

void QtAudioService::shutdown()
{
    interrupt();
    m_player->setSource(QUrl());
}

void QtAudioService::handleMediaStatusChanged()
{
    const auto status = m_player->mediaStatus();
    if (status == QMediaPlayer::EndOfMedia || status == QMediaPlayer::InvalidMedia) {
        finishCurrentPlayback();
        playNextIfIdle();
    }
}

void QtAudioService::handleTtsFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const AudioPlaybackRequest request{
        m_currentPlaybackText,
        m_currentPlaybackPriority,
        false,
        QString(),
    };
    const QString outputFilePath = m_currentSynthesisOutputFilePath;
    m_currentSynthesisOutputFilePath.clear();

    if (exitStatus == QProcess::NormalExit
        && exitCode == 0
        && QFileInfo::exists(outputFilePath)) {
        playResolvedAudio(request, outputFilePath);
        return;
    }

    qWarning().noquote() << "[AudioService][edge-tts-failed]" << request.text;
    Q_EMIT playbackWarning(QStringLiteral("edge-tts 合成失败，将回退到提示音或静默完成。"));
    m_currentPlaybackText.clear();
    finishCurrentPlayback();
    playNextIfIdle();
}

void QtAudioService::playNextIfIdle()
{
    if (m_pendingRequests.isEmpty()) {
        return;
    }
    if (m_player->playbackState() == QMediaPlayer::PlayingState || m_ttsProcess->state() != QProcess::NotRunning) {
        return;
    }

    const PendingSpeechRequest nextEntry = m_pendingRequests.takeFirst();
    const AudioPlaybackRequest request = nextEntry.request;
    m_spokenHistory.append(request.text);
    m_currentPlaybackText = request.text;
    m_currentPlaybackPriority = request.priority;
    const QString preferredAudioFilePath = request.audioFilePath.trimmed();
    if (!preferredAudioFilePath.isEmpty()) {
        if (QFileInfo::exists(preferredAudioFilePath)) {
            playResolvedAudio(request, preferredAudioFilePath);
            return;
        }
        qWarning().noquote() << "[AudioService][missing-preferred-audio]" << preferredAudioFilePath;
        Q_EMIT playbackWarning(QStringLiteral("脚本缓存语音不存在，将回退到 TTS 或提示音。"));
    }

    const QString reusableCachedFilePath = m_cacheEnabled ? cacheFilePathForText(request.text) : QString();
    if (!reusableCachedFilePath.isEmpty() && QFileInfo::exists(reusableCachedFilePath)) {
        playResolvedAudio(request, reusableCachedFilePath);
        return;
    }

    const QString synthesisOutputFilePath = m_cacheEnabled
        ? cacheFilePathForText(request.text)
        : transientFilePath();
    if (!synthesisOutputFilePath.isEmpty() && QFileInfo::exists(synthesisOutputFilePath)) {
        QFile::remove(synthesisOutputFilePath);
    }
    if (!m_cacheEnabled && !synthesisOutputFilePath.isEmpty()) {
        m_currentTransientAudioFilePath = synthesisOutputFilePath;
    }

    if (!m_edgeTtsExecutablePath.isEmpty() && !synthesisOutputFilePath.isEmpty()) {
        startSynthesis(request, synthesisOutputFilePath);
        return;
    }

    if (QFileInfo::exists(m_notificationFilePath)) {
        playResolvedAudio(request, m_notificationFilePath);
        return;
    }

    m_playbackActive = true;
    Q_EMIT playbackStarted(request.text);
    Q_EMIT playbackWarning(QStringLiteral("没有可用的语音音频输出，当前文本只会记录日志。"));
    qInfo().noquote() << "[AudioService][fallback-no-audio-file]" << request.text;
    finishCurrentPlayback();
    playNextIfIdle();
}

void QtAudioService::finishCurrentPlayback()
{
    if (!m_playbackActive) {
        return;
    }
    m_playbackActive = false;
    m_currentPlaybackText.clear();
    m_currentPlaybackPriority = static_cast<int>(AudioPlaybackPriority::Normal);
    if (!m_currentTransientAudioFilePath.isEmpty()) {
        QFile::remove(m_currentTransientAudioFilePath);
        m_currentTransientAudioFilePath.clear();
    }
    Q_EMIT playbackFinished();
}

QString QtAudioService::cacheFilePathForText(const QString &text) const
{
    if (m_cacheDirPath.trimmed().isEmpty()) {
        return QString();
    }

    const QByteArray digest = QCryptographicHash::hash(
        QStringLiteral("%1:%2:%3:%4").arg(m_ttsProvider, m_voice, m_voiceRate, text).toUtf8(),
        QCryptographicHash::Md5).toHex();
    return QDir(m_cacheDirPath).filePath(QStringLiteral("%1.mp3").arg(QString::fromLatin1(digest)));
}

QString QtAudioService::transientFilePath() const
{
    const QString baseDir = !m_cacheDirPath.trimmed().isEmpty()
        ? m_cacheDirPath
        : QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseDir.trimmed().isEmpty()) {
        return QString();
    }
    return QDir(baseDir).filePath(QStringLiteral("_tts_transient_%1.mp3").arg(m_sequenceCounter));
}

void QtAudioService::startSynthesis(const AudioPlaybackRequest &request, const QString &outputFilePath)
{
    QDir().mkpath(QFileInfo(outputFilePath).absolutePath());
    if (QFileInfo::exists(outputFilePath)) {
        QFile::remove(outputFilePath);
    }

    QStringList arguments;
    arguments << QStringLiteral("--text")
              << request.text
              << QStringLiteral("--voice")
              << m_voice
              << QStringLiteral("--rate")
              << m_voiceRate
              << QStringLiteral("--write-media")
              << outputFilePath;

    m_currentSynthesisOutputFilePath = outputFilePath;
    m_ttsProcess->start(m_edgeTtsExecutablePath, arguments);
    if (!m_ttsProcess->waitForStarted(1000)) {
        qWarning().noquote() << "[AudioService][edge-tts-unavailable]" << request.text;
        Q_EMIT playbackWarning(QStringLiteral("未找到 edge-tts，将回退到提示音或静默完成。"));
        m_currentSynthesisOutputFilePath.clear();
        if (QFileInfo::exists(m_notificationFilePath)) {
            playResolvedAudio(request, m_notificationFilePath);
            return;
        }
        m_playbackActive = true;
        Q_EMIT playbackStarted(request.text);
        finishCurrentPlayback();
        playNextIfIdle();
    }
}

void QtAudioService::playResolvedAudio(const AudioPlaybackRequest &request, const QString &audioFilePath, bool deleteAfterPlayback)
{
    m_currentPlaybackText = request.text;
    m_currentPlaybackPriority = request.priority;
    if (deleteAfterPlayback) {
        m_currentTransientAudioFilePath = audioFilePath;
    }
    m_playbackActive = true;
    Q_EMIT playbackStarted(request.text);
    m_player->setSource(QUrl::fromLocalFile(audioFilePath));
    m_player->play();
}

int QtAudioService::normalizePriority(int priority) const
{
    if (priority <= static_cast<int>(AudioPlaybackPriority::Critical)) {
        return static_cast<int>(AudioPlaybackPriority::Critical);
    }
    if (priority >= static_cast<int>(AudioPlaybackPriority::Low)) {
        return static_cast<int>(AudioPlaybackPriority::Low);
    }
    return priority;
}
