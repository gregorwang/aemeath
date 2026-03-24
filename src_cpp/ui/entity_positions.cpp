#include "ui/entity_positions.h"

#include <QtGlobal>

EntityPositions EntityPositions::calculate(
    const QRect &screenRect,
    const QSize &windowSize,
    int y,
    int margin,
    Edge edge)
{
    const int width = qMax(1, windowSize.width());
    const int height = qMax(1, windowSize.height());
    const int clampedY = qBound(screenRect.top(), y, screenRect.bottom() - height + 1);

    if (edge == Edge::Left) {
        return EntityPositions{
            QPoint(screenRect.left() - width, clampedY),
            QPoint(screenRect.left() - width / 3, clampedY),
            QPoint(screenRect.left() + margin, clampedY),
        };
    }

    const int right = screenRect.right() + 1;
    return EntityPositions{
        QPoint(right, clampedY),
        QPoint(right - width / 3, clampedY),
        QPoint(right - width - margin, clampedY),
    };
}

QPoint EntityPositions::clampPointToScreen(
    const QRect &screenRect,
    const QSize &windowSize,
    const QPoint &point,
    double visibleXRatio,
    double visibleYRatio)
{
    const int width = qMax(1, windowSize.width());
    const int height = qMax(1, windowSize.height());
    const double safeVisibleX = qBound(0.0, visibleXRatio, 1.0);
    const double safeVisibleY = qBound(0.0, visibleYRatio, 1.0);
    const int visibleWidth = qMax(0, qMin(width, static_cast<int>(width * safeVisibleX)));
    const int visibleHeight = qMax(0, qMin(height, static_cast<int>(height * safeVisibleY)));

    const int minX = screenRect.left() - (width - visibleWidth);
    const int maxX = screenRect.left() + screenRect.width() - visibleWidth;
    const int minY = screenRect.top() - (height - visibleHeight);
    const int maxY = screenRect.top() + screenRect.height() - visibleHeight;

    return QPoint(
        qBound(minX, point.x(), qMax(minX, maxX)),
        qBound(minY, point.y(), qMax(minY, maxY)));
}
