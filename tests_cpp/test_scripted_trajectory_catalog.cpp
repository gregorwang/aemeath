#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/scripted_trajectory_catalog.h"

class ScriptedTrajectoryCatalogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvesDirectFileWhenUsable();
    void ignoresInvalidJsonAndFallsBackToUsableFile();
    void directoryPrefersDefaultNamedFileWhenUsable();
};

void ScriptedTrajectoryCatalogTest::resolvesDirectFileWhenUsable()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile file(tempDir.filePath(QStringLiteral("trajectory.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":12,"x":12,"y":12,"state":6}]})");
    file.close();

    QCOMPARE(
        ScriptedTrajectoryCatalog::resolvePreferredTrajectory(file.fileName(), QStringLiteral("trajectory_1771029879_qt_animation.json")),
        QFileInfo(file).absoluteFilePath());
}

void ScriptedTrajectoryCatalogTest::ignoresInvalidJsonAndFallsBackToUsableFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile invalid(tempDir.filePath(QStringLiteral("trajectory_1771999999_qt_animation.json")));
    QVERIFY(invalid.open(QIODevice::WriteOnly | QIODevice::Text));
    invalid.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0}]})");
    invalid.close();

    QFile usable(tempDir.filePath(QStringLiteral("trajectory_1771029879_qt_animation.json")));
    QVERIFY(usable.open(QIODevice::WriteOnly | QIODevice::Text));
    usable.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":20,"x":20,"y":20,"state":6}]})");
    usable.close();

    QCOMPARE(
        ScriptedTrajectoryCatalog::resolvePreferredTrajectory(tempDir.path(), QStringLiteral("trajectory_1771029879_qt_animation.json")),
        QFileInfo(usable).absoluteFilePath());
}

void ScriptedTrajectoryCatalogTest::directoryPrefersDefaultNamedFileWhenUsable()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile newer(tempDir.filePath(QStringLiteral("trajectory_1999999999_qt_animation.json")));
    QVERIFY(newer.open(QIODevice::WriteOnly | QIODevice::Text));
    newer.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":30,"x":30,"y":30,"state":6}]})");
    newer.close();

    QFile preferred(tempDir.filePath(QStringLiteral("trajectory_1771029879_qt_animation.json")));
    QVERIFY(preferred.open(QIODevice::WriteOnly | QIODevice::Text));
    preferred.write(R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":25,"x":25,"y":25,"state":5}]})");
    preferred.close();

    QCOMPARE(
        ScriptedTrajectoryCatalog::resolvePreferredTrajectory(tempDir.path(), QStringLiteral("trajectory_1771029879_qt_animation.json")),
        QFileInfo(preferred).absoluteFilePath());
}

QTEST_MAIN(ScriptedTrajectoryCatalogTest)

#include "test_scripted_trajectory_catalog.moc"
