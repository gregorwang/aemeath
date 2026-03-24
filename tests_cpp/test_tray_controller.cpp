#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSignalSpy>
#include <QSystemTrayIcon>
#include <QtTest>

#include "ui/tray_controller.h"

class TrayControllerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void setterMethodsSynchronizeCheckState();
    void toggleActionsEmitSignals();
};

namespace {

QAction *findAction(QMenu *menu, const QString &text)
{
    if (!menu) {
        return nullptr;
    }
    const QList<QAction *> actions = menu->actions();
    for (QAction *action : actions) {
        if (action && action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

QMenu *contextMenu(QObject *controller)
{
    const QList<QSystemTrayIcon *> trays = controller->findChildren<QSystemTrayIcon *>();
    if (trays.isEmpty() || !trays.constFirst()) {
        return nullptr;
    }
    return trays.constFirst()->contextMenu();
}

}

void TrayControllerTest::setterMethodsSynchronizeCheckState()
{
    TrayController controller{QIcon()};
    const QList<QSystemTrayIcon *> trays = controller.findChildren<QSystemTrayIcon *>();
    QVERIFY(!trays.isEmpty());

    controller.setDndChecked(true);
    controller.setOfflineChecked(true);
    controller.setResidentChecked(true);
    controller.setAutoCommentaryChecked(true);
    controller.setCameraChecked(true);
    controller.setMicrophoneChecked(true);
    controller.setWakeupChecked(true);
    controller.setEyeTrackingChecked(true);
    controller.setPeriodicScanChecked(true);
    controller.setAudioReactiveChecked(true);
    controller.setContinuousVoiceModeChecked(true);
    controller.setScriptedEntranceChecked(true);
    controller.setFullscreenPauseChecked(true);
    controller.setIdleInvasionChecked(true);

    QVERIFY(controller.isDndChecked());
    QVERIFY(controller.isOfflineChecked());
    QVERIFY(controller.isResidentChecked());
    QVERIFY(controller.isAutoCommentaryChecked());
    QVERIFY(controller.isCameraChecked());
    QVERIFY(controller.isMicrophoneChecked());
    QVERIFY(controller.isWakeupChecked());
    QVERIFY(controller.isEyeTrackingChecked());
    QVERIFY(controller.isPeriodicScanChecked());
    QVERIFY(controller.isAudioReactiveChecked());
    QVERIFY(controller.isContinuousVoiceModeChecked());
    QVERIFY(controller.isScriptedEntranceChecked());
    QVERIFY(controller.isFullscreenPauseChecked());
    QVERIFY(controller.isIdleInvasionChecked());
    QCOMPARE(trays.constFirst()->toolTip(), QStringLiteral("CyberCompanionCpp (请勿打扰)"));
}

void TrayControllerTest::toggleActionsEmitSignals()
{
    TrayController controller{QIcon()};
    QMenu *menu = contextMenu(&controller);
    QVERIFY(menu != nullptr);

    QSignalSpy offlineSpy(&controller, &TrayController::offlineModeToggled);
    QSignalSpy residentSpy(&controller, &TrayController::residentModeToggled);
    QSignalSpy cameraSpy(&controller, &TrayController::cameraToggled);
    QSignalSpy continuousSpy(&controller, &TrayController::continuousVoiceModeToggled);
    QSignalSpy scriptedSpy(&controller, &TrayController::scriptedEntranceToggled);
    QSignalSpy fullscreenSpy(&controller, &TrayController::fullscreenPauseToggled);
    QSignalSpy invasionSpy(&controller, &TrayController::idleInvasionToggled);
    QSignalSpy reloadCharactersSpy(&controller, &TrayController::reloadCharactersRequested);
    QSignalSpy checkUpdatesSpy(&controller, &TrayController::checkUpdatesRequested);

    QAction *offlineAction = findAction(menu, QStringLiteral("离线模式"));
    QAction *residentAction = findAction(menu, QStringLiteral("常驻模式"));
    QAction *cameraAction = findAction(menu, QStringLiteral("启用摄像头"));
    QAction *continuousAction = findAction(menu, QStringLiteral("连续唤醒模式"));
    QAction *scriptedAction = findAction(menu, QStringLiteral("启用脚本式登场"));
    QAction *fullscreenAction = findAction(menu, QStringLiteral("全屏时暂停"));
    QAction *invasionAction = findAction(menu, QStringLiteral("启用空闲入侵"));
    QAction *reloadCharactersAction = findAction(menu, QStringLiteral("重载角色"));
    QAction *checkUpdatesAction = findAction(menu, QStringLiteral("检查更新"));

    QVERIFY(offlineAction != nullptr);
    QVERIFY(residentAction != nullptr);
    QVERIFY(cameraAction != nullptr);
    QVERIFY(continuousAction != nullptr);
    QVERIFY(scriptedAction != nullptr);
    QVERIFY(fullscreenAction != nullptr);
    QVERIFY(invasionAction != nullptr);
    QVERIFY(reloadCharactersAction != nullptr);
    QVERIFY(checkUpdatesAction != nullptr);

    offlineAction->setChecked(true);
    residentAction->setChecked(true);
    cameraAction->setChecked(true);
    continuousAction->setChecked(true);
    scriptedAction->setChecked(true);
    fullscreenAction->setChecked(true);
    invasionAction->setChecked(true);
    reloadCharactersAction->trigger();
    checkUpdatesAction->trigger();

    QCOMPARE(offlineSpy.count(), 1);
    QCOMPARE(residentSpy.count(), 1);
    QCOMPARE(cameraSpy.count(), 1);
    QCOMPARE(continuousSpy.count(), 1);
    QCOMPARE(scriptedSpy.count(), 1);
    QCOMPARE(fullscreenSpy.count(), 1);
    QCOMPARE(invasionSpy.count(), 1);
    QCOMPARE(reloadCharactersSpy.count(), 1);
    QCOMPARE(checkUpdatesSpy.count(), 1);
    QVERIFY(controller.isOfflineChecked());
    QVERIFY(controller.isResidentChecked());
    QVERIFY(controller.isCameraChecked());
    QVERIFY(controller.isContinuousVoiceModeChecked());
    QVERIFY(controller.isScriptedEntranceChecked());
    QVERIFY(controller.isFullscreenPauseChecked());
    QVERIFY(controller.isIdleInvasionChecked());
}

QTEST_MAIN(TrayControllerTest)

#include "test_tray_controller.moc"
