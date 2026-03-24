#include <QtTest>

#include "runtime/presence_detector.h"

class PresenceDetectorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void recentActivityIsPresentActive();
    void nullSampleIsUnknownAfterIdle();
    void passivePresenceRequiresFaceWhenIdleLongEnough();
    void prolongedNoFaceBecomesAbsent();
};

void PresenceDetectorTest::recentActivityIsPresentActive()
{
    PresenceDetector detector;
    const GazeSample sample{ true, 0.1, 0.0, 0.9, QStringLiteral("neutral"), 0.0, 90.0, 0.0 };

    QCOMPARE(detector.determinePresence(30'000, sample), PresenceState::PresentActive);
}

void PresenceDetectorTest::nullSampleIsUnknownAfterIdle()
{
    PresenceDetector detector;

    QCOMPARE(detector.determinePresence(120'000, static_cast<const GazeSample *>(nullptr)), PresenceState::Unknown);
}

void PresenceDetectorTest::passivePresenceRequiresFaceWhenIdleLongEnough()
{
    PresenceDetectorOptions options;
    options.idleThresholdMs = 10'000;
    options.faceAbsentFrames = 2;
    options.stillNoFaceFrames = 2;
    options.darkNoFaceFrames = 2;

    PresenceDetector detector(options);
    detector.setTargetFps(15);

    GazeSample sample;
    sample.faceDetected = true;
    sample.faceX = 0.2;
    sample.confidence = 0.8;
    sample.brightness = 80.0;
    sample.motionScore = 2.0;

    QCOMPARE(detector.determinePresence(120'000, sample), PresenceState::PresentPassive);
}

void PresenceDetectorTest::prolongedNoFaceBecomesAbsent()
{
    PresenceDetectorOptions options;
    options.idleThresholdMs = 10'000;
    options.referenceFps = 10;
    options.faceAbsentFrames = 2;
    options.stillNoFaceFrames = 2;
    options.darkNoFaceFrames = 2;

    PresenceDetector detector(options);
    detector.setTargetFps(10);

    GazeSample sample;
    sample.faceDetected = false;
    sample.brightness = 10.0;
    sample.motionScore = 0.0;

    QCOMPARE(detector.determinePresence(120'000, sample), PresenceState::Unknown);
    QTest::qWait(130);
    QCOMPARE(detector.determinePresence(120'000, sample), PresenceState::Absent);
}

QTEST_MAIN(PresenceDetectorTest)

#include "test_presence_detector.moc"
