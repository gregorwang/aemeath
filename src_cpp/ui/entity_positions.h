#pragma once

#include <QPoint>
#include <QRect>
#include <QSize>

class EntityPositions
{
public:
    enum class Edge {
        Right,
        Left
    };

    QPoint hidden;
    QPoint peeking;
    QPoint engaged;

    static EntityPositions calculate(
        const QRect &screenRect,
        const QSize &windowSize,
        int y,
        int margin = 24,
        Edge edge = Edge::Right);

    static QPoint clampPointToScreen(
        const QRect &screenRect,
        const QSize &windowSize,
        const QPoint &point,
        double visibleXRatio = 1.0,
        double visibleYRatio = 1.0);
};
