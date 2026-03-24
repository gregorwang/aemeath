#pragma once

#include <QObject>
#include <QPoint>
#include <QString>
#include <QVector>

class QVariantAnimation;
class QWidget;

class TrajectoryPlayer : public QObject
{
    Q_OBJECT

public:
    struct TimelineKeyframe {
        int timeMs = 0;
        QPoint position;
        int stateId = 1;
    };

    explicit TrajectoryPlayer(QWidget *target, QObject *parent = nullptr);

    void play(const QVector<QPoint> &offsets, int durationMs);
    bool playTimelineFile(const QString &filePath);
    void stop();
    bool isPlaying() const;
    int currentDurationMs() const;

Q_SIGNALS:
    void finished();
    void timelineFinished();
    void timelineAborted();
    void stateCue(int stateId);

private:
    void clearTimelineState();
    QPoint interpolateOffset(qreal progress) const;
    void updateTimelineFrame(int elapsedMs);

    enum class Mode {
        None,
        Offsets,
        Timeline
    };

    QWidget *m_target = nullptr;
    QVariantAnimation *m_animation = nullptr;
    QVector<QPoint> m_offsets;
    QVector<TimelineKeyframe> m_timelineFrames;
    QPoint m_origin;
    Mode m_mode = Mode::None;
    int m_currentTimelineIndex = 0;
    int m_currentTimelineState = -1;
};
