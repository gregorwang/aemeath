#include "ui/trajectory_player.h"

#include <QAbstractAnimation>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>
#include <QtMath>
#include <QVariantAnimation>
#include <QWidget>

namespace {

bool appendFrame(
    QVector<TrajectoryPlayer::TimelineKeyframe> &frames,
    int rawTimeMs,
    const QPoint &position,
    int stateId,
    int &lastTimeMs)
{
    if (rawTimeMs < 0) {
        return false;
    }

    int timeMs = rawTimeMs;
    if (timeMs <= lastTimeMs) {
        timeMs = lastTimeMs + 1;
    }

    TrajectoryPlayer::TimelineKeyframe frame;
    frame.timeMs = timeMs;
    frame.position = position;
    frame.stateId = stateId;
    frames.push_back(frame);
    lastTimeMs = timeMs;
    return true;
}

QVector<TrajectoryPlayer::TimelineKeyframe> loadKeyframeSchema(const QJsonObject &root)
{
    const QJsonArray keyframes = root.value(QStringLiteral("keyframes")).toArray();
    QVector<TrajectoryPlayer::TimelineKeyframe> frames;
    frames.reserve(keyframes.size());

    int lastTimeMs = -1;
    for (const QJsonValue &value : keyframes) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        if (!object.contains(QStringLiteral("x"))
            || !object.contains(QStringLiteral("y"))
            || !object.contains(QStringLiteral("time_ms"))) {
            continue;
        }

        appendFrame(
            frames,
            object.value(QStringLiteral("time_ms")).toInt(),
            QPoint(object.value(QStringLiteral("x")).toInt(), object.value(QStringLiteral("y")).toInt()),
            object.value(QStringLiteral("state")).toInt(1),
            lastTimeMs);
    }

    return frames;
}

QVector<TrajectoryPlayer::TimelineKeyframe> loadPointsSchema(const QJsonObject &root)
{
    const QJsonArray points = root.value(QStringLiteral("points")).toArray();
    QVector<TrajectoryPlayer::TimelineKeyframe> frames;
    frames.reserve(points.size());

    int lastTimeMs = -1;
    for (const QJsonValue &value : points) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        if (!object.contains(QStringLiteral("x"))
            || !object.contains(QStringLiteral("y"))
            || !object.contains(QStringLiteral("t"))) {
            continue;
        }

        const double seconds = object.value(QStringLiteral("t")).toDouble(-1.0);
        if (seconds < 0.0) {
            continue;
        }

        appendFrame(
            frames,
            qMax(0, qRound64(seconds * 1000.0)),
            QPoint(object.value(QStringLiteral("x")).toInt(), object.value(QStringLiteral("y")).toInt()),
            object.value(QStringLiteral("s")).toInt(1),
            lastTimeMs);
    }

    int trailingDurationMs = -1;
    if (root.contains(QStringLiteral("duration_ms"))) {
        trailingDurationMs = qMax(trailingDurationMs, root.value(QStringLiteral("duration_ms")).toInt(-1));
    }
    if (root.contains(QStringLiteral("total_duration"))) {
        const double seconds = root.value(QStringLiteral("total_duration")).toDouble(-1.0);
        if (seconds > 0.0) {
            trailingDurationMs = qMax(trailingDurationMs, qMax(0, qRound64(seconds * 1000.0)));
        }
    }

    if (!frames.isEmpty() && trailingDurationMs > frames.last().timeMs) {
        const TrajectoryPlayer::TimelineKeyframe lastFrame = frames.last();
        appendFrame(frames, trailingDurationMs, lastFrame.position, lastFrame.stateId, lastTimeMs);
    }

    return frames;
}

QVector<TrajectoryPlayer::TimelineKeyframe> loadTimelineFrames(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    if (root.contains(QStringLiteral("keyframes"))) {
        return loadKeyframeSchema(root);
    }
    if (root.contains(QStringLiteral("points"))) {
        return loadPointsSchema(root);
    }
    return {};
}

}

TrajectoryPlayer::TrajectoryPlayer(QWidget *target, QObject *parent)
    : QObject(parent)
    , m_target(target)
    , m_animation(new QVariantAnimation(this))
{
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);

    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (!m_target) {
            return;
        }

        if (m_mode == Mode::Offsets) {
            if (m_offsets.isEmpty()) {
                return;
            }
            const qreal progress = value.toReal();
            m_target->move(m_origin + interpolateOffset(progress));
            return;
        }

        if (m_mode == Mode::Timeline) {
            const int elapsedMs = value.toInt();
            updateTimelineFrame(elapsedMs);
        }
    });

    connect(m_animation, &QVariantAnimation::finished, this, [this]() {
        if (!m_target) {
            clearTimelineState();
            m_offsets.clear();
            m_mode = Mode::None;
            Q_EMIT finished();
            return;
        }

        const Mode finishedMode = m_mode;
        if (m_mode == Mode::Offsets) {
            m_target->move(m_origin);
        } else if (m_mode == Mode::Timeline && !m_timelineFrames.isEmpty()) {
            const TrajectoryPlayer::TimelineKeyframe &last = m_timelineFrames.last();
            m_target->move(last.position);
            if (m_currentTimelineState != last.stateId) {
                m_currentTimelineState = last.stateId;
                Q_EMIT stateCue(last.stateId);
            }
        }

        clearTimelineState();
        m_offsets.clear();
        m_mode = Mode::None;
        if (finishedMode == Mode::Timeline) {
            Q_EMIT timelineFinished();
        }
        Q_EMIT finished();
    });
}

void TrajectoryPlayer::play(const QVector<QPoint> &offsets, int durationMs)
{
    if (!m_target || offsets.size() < 2) {
        return;
    }

    clearTimelineState();
    m_offsets = offsets;
    m_origin = m_target->pos();
    m_mode = Mode::Offsets;
    m_animation->stop();
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setDuration(durationMs);
    m_animation->start();
}

bool TrajectoryPlayer::playTimelineFile(const QString &filePath)
{
    if (!m_target) {
        return false;
    }

    const QVector<TimelineKeyframe> frames = loadTimelineFrames(filePath);
    if (frames.size() < 2) {
        return false;
    }

    m_animation->stop();
    m_offsets.clear();
    clearTimelineState();
    m_timelineFrames = frames;
    m_mode = Mode::Timeline;
    m_target->move(m_timelineFrames.first().position);
    m_target->show();
    m_target->raise();
    m_animation->setStartValue(0);
    m_animation->setEndValue(m_timelineFrames.last().timeMs);
    m_animation->setDuration(m_timelineFrames.last().timeMs);
    m_animation->start();
    updateTimelineFrame(0);
    return true;
}

void TrajectoryPlayer::stop()
{
    const bool abortedTimeline =
        m_mode == Mode::Timeline && m_animation->state() == QAbstractAnimation::Running;
    m_animation->stop();
    if (m_target && m_mode == Mode::Offsets) {
        m_target->move(m_origin);
    }
    m_offsets.clear();
    clearTimelineState();
    m_mode = Mode::None;
    if (abortedTimeline) {
        Q_EMIT timelineAborted();
    }
}

bool TrajectoryPlayer::isPlaying() const
{
    return m_animation->state() == QAbstractAnimation::Running;
}

int TrajectoryPlayer::currentDurationMs() const
{
    if (m_timelineFrames.isEmpty()) {
        return 0;
    }
    return m_timelineFrames.last().timeMs;
}

void TrajectoryPlayer::clearTimelineState()
{
    m_timelineFrames.clear();
    m_currentTimelineIndex = 0;
    m_currentTimelineState = -1;
}

QPoint TrajectoryPlayer::interpolateOffset(qreal progress) const
{
    if (m_offsets.isEmpty()) {
        return QPoint();
    }
    if (m_offsets.size() == 1 || progress <= 0.0) {
        return m_offsets.first();
    }
    if (progress >= 1.0) {
        return m_offsets.last();
    }

    const qreal scaled = progress * (m_offsets.size() - 1);
    const int index = static_cast<int>(scaled);
    const int nextIndex = qMin(index + 1, m_offsets.size() - 1);
    const qreal localProgress = scaled - index;

    const QPoint a = m_offsets.at(index);
    const QPoint b = m_offsets.at(nextIndex);
    return QPoint(
        static_cast<int>(a.x() + (b.x() - a.x()) * localProgress),
        static_cast<int>(a.y() + (b.y() - a.y()) * localProgress));
}

void TrajectoryPlayer::updateTimelineFrame(int elapsedMs)
{
    if (!m_target || m_timelineFrames.isEmpty()) {
        return;
    }

    while (m_currentTimelineIndex < m_timelineFrames.size() - 1
        && m_timelineFrames.at(m_currentTimelineIndex + 1).timeMs <= elapsedMs) {
        ++m_currentTimelineIndex;
    }

    const TimelineKeyframe &a = m_timelineFrames.at(m_currentTimelineIndex);
    const TimelineKeyframe &b = (m_currentTimelineIndex < m_timelineFrames.size() - 1)
        ? m_timelineFrames.at(m_currentTimelineIndex + 1)
        : a;

    const int duration = qMax(1, b.timeMs - a.timeMs);
    const qreal alpha = qBound(0.0, static_cast<qreal>(elapsedMs - a.timeMs) / duration, 1.0);
    const QPoint current(
        static_cast<int>(a.position.x() + (b.position.x() - a.position.x()) * alpha),
        static_cast<int>(a.position.y() + (b.position.y() - a.position.y()) * alpha));
    m_target->move(current);

    const int targetState = (b.stateId != a.stateId && alpha >= 0.35) ? b.stateId : a.stateId;
    if (targetState != m_currentTimelineState) {
        m_currentTimelineState = targetState;
        Q_EMIT stateCue(targetState);
    }
}
