#include <QtTest>

#include "ui/entity_positions.h"

class EntityPositionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void calculateRightEdgePositions();
    void calculateLeftEdgePositions();
    void clampPointKeepsWindowOnScreen();
    void clampPointAllowsPartialVisibility();
    void clampPointAllowsFullyHiddenPosition();
};

void EntityPositionsTest::calculateRightEdgePositions()
{
    const QRect screen(0, 0, 1920, 1080);
    const QSize window(300, 250);
    const EntityPositions positions = EntityPositions::calculate(screen, window, 700);

    QCOMPARE(positions.hidden, QPoint(1920, 700));
    QCOMPARE(positions.peeking, QPoint(1820, 700));
    QCOMPARE(positions.engaged, QPoint(1596, 700));
}

void EntityPositionsTest::calculateLeftEdgePositions()
{
    const QRect screen(0, 0, 1920, 1080);
    const QSize window(300, 250);
    const EntityPositions positions = EntityPositions::calculate(screen, window, 700, 24, EntityPositions::Edge::Left);

    QCOMPARE(positions.hidden, QPoint(-300, 700));
    QCOMPARE(positions.peeking, QPoint(-100, 700));
    QCOMPARE(positions.engaged, QPoint(24, 700));
}

void EntityPositionsTest::clampPointKeepsWindowOnScreen()
{
    const QRect screen(0, 0, 1920, 1080);
    const QSize window(300, 250);

    const QPoint clamped = EntityPositions::clampPointToScreen(screen, window, QPoint(1900, 1000));

    QCOMPARE(clamped, QPoint(1620, 830));
}

void EntityPositionsTest::clampPointAllowsPartialVisibility()
{
    const QRect screen(0, 0, 1920, 1080);
    const QSize window(300, 250);

    const QPoint clamped = EntityPositions::clampPointToScreen(screen, window, QPoint(-500, -400), 0.5, 0.5);

    QCOMPARE(clamped, QPoint(-150, -125));
}

void EntityPositionsTest::clampPointAllowsFullyHiddenPosition()
{
    const QRect screen(0, 0, 1920, 1080);
    const QSize window(300, 250);

    const QPoint clamped = EntityPositions::clampPointToScreen(screen, window, QPoint(-500, 700), 0.0, 1.0);

    QCOMPARE(clamped, QPoint(-300, 700));
}

QTEST_MAIN(EntityPositionsTest)

#include "test_entity_positions.moc"
