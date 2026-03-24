#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/auto_start_manager.h"

class AutoStartManagerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void buildCommandQuotesExecutablePath();
    void syncEnabledWritesAndRemovesValue();
};

void AutoStartManagerTest::buildCommandQuotesExecutablePath()
{
    const QString command = AutoStartManager::buildCommand(QStringLiteral("C:/Program Files/CyberCompanionCpp/CyberCompanionCpp.exe"), true);
    QCOMPARE(
        command,
        QStringLiteral("\"C:\\Program Files\\CyberCompanionCpp\\CyberCompanionCpp.exe\" --autostart --start-minimized"));
}

void AutoStartManagerTest::syncEnabledWritesAndRemovesValue()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString settingsPath = tempDir.filePath(QStringLiteral("autorun.ini"));
    AutoStartManager manager(settingsPath, QSettings::IniFormat, QStringLiteral("CyberCompanionCpp"));

    QVERIFY(!manager.isEnabled());
    QVERIFY(manager.currentCommand().isEmpty());

    QString errorMessage;
    QVERIFY(manager.syncEnabled(true, QStringLiteral("C:/Apps/CyberCompanionCpp.exe"), false, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QVERIFY(manager.isEnabled());
    QCOMPARE(
        manager.currentCommand(),
        QStringLiteral("\"C:\\Apps\\CyberCompanionCpp.exe\" --autostart"));

    QVERIFY(manager.syncEnabled(false, QStringLiteral("C:/Apps/CyberCompanionCpp.exe"), false, &errorMessage));
    QVERIFY2(errorMessage.isEmpty(), qPrintable(errorMessage));
    QVERIFY(!manager.isEnabled());
    QVERIFY(manager.currentCommand().isEmpty());
}

QTEST_MAIN(AutoStartManagerTest)

#include "test_auto_start_manager.moc"
