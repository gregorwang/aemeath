#include <QSignalSpy>
#include <QTemporaryFile>
#include <QWidget>
#include <QtTest>

#include "ui/trajectory_player.h"

class TrajectoryPlayerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void playTimelineFileSupportsKeyframeSchema();
    void playTimelineFileSupportsRecordedPointsSchema();
    void playTimelineFileRespectsRecordedTotalDuration();
    void stopEmitsTimelineAbortedForRunningTimeline();
};

namespace {

QString writePayload(QTemporaryFile &file, const QByteArray &payload)
{
    if (!file.open()) {
        return {};
    }
    if (file.write(payload) != payload.size()) {
        return {};
    }
    file.flush();
    return file.fileName();
}

}

void TrajectoryPlayerTest::playTimelineFileSupportsKeyframeSchema()
{
    QWidget target;
    target.resize(120, 120);
    target.show();

    TrajectoryPlayer player(&target);
    QSignalSpy stateSpy(&player, &TrajectoryPlayer::stateCue);
    QSignalSpy finishedSpy(&player, &TrajectoryPlayer::timelineFinished);

    QTemporaryFile file;
    const QString filePath = writePayload(
        file,
        R"({"keyframes":[{"time_ms":0,"x":5,"y":6,"state":1},{"time_ms":40,"x":25,"y":30,"state":6}]})");
    QVERIFY(!filePath.isEmpty());

    QVERIFY(player.playTimelineFile(filePath));
    QCOMPARE(player.currentDurationMs(), 40);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 1000);
    QVERIFY(!stateSpy.isEmpty());
    QCOMPARE(stateSpy.last().at(0).toInt(), 6);
    QCOMPARE(target.pos(), QPoint(25, 30));
}

void TrajectoryPlayerTest::playTimelineFileSupportsRecordedPointsSchema()
{
    QWidget target;
    target.resize(120, 120);
    target.show();

    TrajectoryPlayer player(&target);
    QSignalSpy stateSpy(&player, &TrajectoryPlayer::stateCue);
    QSignalSpy finishedSpy(&player, &TrajectoryPlayer::timelineFinished);

    QTemporaryFile file;
    const QString filePath = writePayload(
        file,
        R"({"total_duration":0.05,"points":[{"x":2,"y":3,"t":0.0,"s":1},{"x":18,"y":21,"t":0.05,"s":8}]})");
    QVERIFY(!filePath.isEmpty());

    QVERIFY(player.playTimelineFile(filePath));
    QCOMPARE(player.currentDurationMs(), 50);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 1000);
    QVERIFY(!stateSpy.isEmpty());
    QCOMPARE(stateSpy.last().at(0).toInt(), 8);
    QCOMPARE(target.pos(), QPoint(18, 21));
}

void TrajectoryPlayerTest::playTimelineFileRespectsRecordedTotalDuration()
{
    QWidget target;
    target.resize(120, 120);
    target.show();

    TrajectoryPlayer player(&target);
    QSignalSpy finishedSpy(&player, &TrajectoryPlayer::timelineFinished);

    QTemporaryFile file;
    const QString filePath = writePayload(
        file,
        R"({"total_duration":0.20,"points":[{"x":4,"y":5,"t":0.0,"s":1},{"x":20,"y":24,"t":0.05,"s":8}]})");
    QVERIFY(!filePath.isEmpty());

    QVERIFY(player.playTimelineFile(filePath));
    QCOMPARE(player.currentDurationMs(), 200);

    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 1000);
    QCOMPARE(target.pos(), QPoint(20, 24));
}

void TrajectoryPlayerTest::stopEmitsTimelineAbortedForRunningTimeline()
{
    QWidget target;
    target.resize(120, 120);
    target.show();

    TrajectoryPlayer player(&target);
    QSignalSpy abortedSpy(&player, &TrajectoryPlayer::timelineAborted);
    QSignalSpy finishedSpy(&player, &TrajectoryPlayer::timelineFinished);

    QTemporaryFile file;
    const QString filePath = writePayload(
        file,
        R"({"keyframes":[{"time_ms":0,"x":0,"y":0,"state":1},{"time_ms":200,"x":60,"y":40,"state":6}]})");
    QVERIFY(!filePath.isEmpty());

    QVERIFY(player.playTimelineFile(filePath));
    QVERIFY(player.isPlaying());

    player.stop();

    QCOMPARE(abortedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 0);
    QVERIFY(!player.isPlaying());
}

QTEST_MAIN(TrajectoryPlayerTest)

#include "test_trajectory_player.moc"
