#include <QtTest>

#include "runtime/idle_invasion_controller.h"

class IdleInvasionControllerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void debugTriggerStartsInvasion();
    void userActivityRetreatsBackToInactive();
    void dndBlocksIdleStart();
};

void IdleInvasionControllerTest::debugTriggerStartsInvasion()
{
    IdleInvasionController controller;
    IdleInvasionConfig config;
    config.enabled = true;
    config.maxInvaders = 2;
    controller.applyConfig(config);

    const bool started = controller.triggerDebugInvasion();

    QVERIFY(started);
    QTRY_VERIFY_WITH_TIMEOUT(controller.activeCount() >= 1, 1500);
    QVERIFY(
        controller.state() == IdleInvasionController::InvasionState::Spawning
        || controller.state() == IdleInvasionController::InvasionState::Saturated);

    controller.shutdown();
    QCOMPARE(controller.state(), IdleInvasionController::InvasionState::Inactive);
}

void IdleInvasionControllerTest::userActivityRetreatsBackToInactive()
{
    IdleInvasionController controller;
    IdleInvasionConfig config;
    config.enabled = true;
    config.maxInvaders = 2;
    config.retreatStyle = QStringLiteral("instant");
    config.startDelayMs = 1000;
    controller.applyConfig(config);

    controller.onIdleTimeUpdated(1200);
    QTRY_VERIFY_WITH_TIMEOUT(controller.activeCount() >= 1, 1500);

    controller.onUserActivityDetected();

    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), IdleInvasionController::InvasionState::Inactive, 1500);
    QCOMPARE(controller.activeCount(), 0);
}

void IdleInvasionControllerTest::dndBlocksIdleStart()
{
    IdleInvasionController controller;
    IdleInvasionConfig config;
    config.enabled = true;
    config.startDelayMs = 1000;
    controller.applyConfig(config);
    controller.setDndEnabled(true);

    controller.onIdleTimeUpdated(120000);

    QCOMPARE(controller.state(), IdleInvasionController::InvasionState::Inactive);
    QCOMPARE(controller.activeCount(), 0);
}

QTEST_MAIN(IdleInvasionControllerTest)

#include "test_idle_invasion_controller.moc"
