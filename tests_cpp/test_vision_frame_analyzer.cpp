#include <QImage>
#include <QtTest>

#include "runtime/vision_frame_analyzer.h"

class VisionFrameAnalyzerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void firstFrameHasNoMotion();
    void movingBrightRegionProducesPresenceAndCentroid();
    void repeatedFrameDropsMotionBackDown();
};

void VisionFrameAnalyzerTest::firstFrameHasNoMotion()
{
    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(Qt::white);

    const VisionFrameAnalysis analysis = VisionFrameAnalyzer::analyze(image, QByteArray());

    QVERIFY(!analysis.lumaSamples.isEmpty());
    QCOMPARE(analysis.sample.motionScore, 0.0);
    QCOMPARE(analysis.sample.faceDetected, false);
    QCOMPARE(analysis.sample.faceX, 0.0);
    QCOMPARE(analysis.sample.faceY, 0.0);
}

void VisionFrameAnalyzerTest::movingBrightRegionProducesPresenceAndCentroid()
{
    QImage previous(64, 64, QImage::Format_RGB32);
    previous.fill(Qt::black);
    const VisionFrameAnalysis previousAnalysis = VisionFrameAnalyzer::analyze(previous, QByteArray());

    QImage current(64, 64, QImage::Format_RGB32);
    current.fill(Qt::black);
    for (int y = 16; y < 48; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(current.scanLine(y));
        for (int x = 40; x < 60; ++x) {
            line[x] = qRgb(255, 255, 255);
        }
    }

    const VisionFrameAnalysis analysis = VisionFrameAnalyzer::analyze(current, previousAnalysis.lumaSamples);

    QCOMPARE(analysis.sample.faceDetected, true);
    QVERIFY(analysis.sample.motionScore > 2.0);
    QVERIFY(analysis.sample.brightness > 25.0);
    QVERIFY(analysis.sample.faceX > 0.2);
    QVERIFY(qAbs(analysis.sample.faceY) < 0.4);
    QVERIFY(analysis.sample.confidence >= 0.2);
}

void VisionFrameAnalyzerTest::repeatedFrameDropsMotionBackDown()
{
    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(Qt::black);
    for (int y = 8; y < 56; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 8; x < 56; ++x) {
            line[x] = qRgb(230, 230, 230);
        }
    }

    const VisionFrameAnalysis first = VisionFrameAnalyzer::analyze(image, QByteArray());
    const VisionFrameAnalysis second = VisionFrameAnalyzer::analyze(image, first.lumaSamples);

    QCOMPARE(first.sample.motionScore, 0.0);
    QCOMPARE(second.sample.motionScore, 0.0);
    QCOMPARE(second.sample.faceDetected, false);
}

QTEST_MAIN(VisionFrameAnalyzerTest)

#include "test_vision_frame_analyzer.moc"
