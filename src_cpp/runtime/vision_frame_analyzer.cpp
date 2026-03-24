#include "runtime/vision_frame_analyzer.h"

#include <QtGlobal>

namespace {

constexpr int kMinimumBrightnessForPresence = 25;
constexpr double kMinimumMotionForPresence = 2.0;

}

VisionFrameAnalysis VisionFrameAnalyzer::analyze(const QImage &image, const QByteArray &previousLuma)
{
    VisionFrameAnalysis analysis;

    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB32);
    const int width = rgbImage.width();
    const int height = rgbImage.height();
    const int step = qMax(1, qMin(width, height) / 48);

    analysis.lumaSamples.reserve((width / step + 1) * (height / step + 1));

    qint64 brightnessSum = 0;
    qint64 sampleCount = 0;
    for (int y = 0; y < height; y += step) {
        const QRgb *line = reinterpret_cast<const QRgb *>(rgbImage.constScanLine(y));
        for (int x = 0; x < width; x += step) {
            const int luma = qGray(line[x]);
            analysis.lumaSamples.append(static_cast<char>(luma));
            brightnessSum += luma;
            ++sampleCount;
        }
    }

    double motionScore = 0.0;
    double weightedX = 0.0;
    double weightedY = 0.0;
    double motionWeight = 0.0;
    if (!previousLuma.isEmpty() && previousLuma.size() == analysis.lumaSamples.size()) {
        qint64 motionSum = 0;
        int sampleIndex = 0;
        for (int y = 0; y < height; y += step) {
            for (int x = 0; x < width; x += step, ++sampleIndex) {
                const int currentLuma = static_cast<int>(static_cast<unsigned char>(analysis.lumaSamples.at(sampleIndex)));
                const int previous = static_cast<int>(static_cast<unsigned char>(previousLuma.at(sampleIndex)));
                const int diff = qAbs(currentLuma - previous);
                motionSum += diff;
                if (diff >= 8) {
                    motionWeight += static_cast<double>(diff);
                    weightedX += static_cast<double>(x) * static_cast<double>(diff);
                    weightedY += static_cast<double>(y) * static_cast<double>(diff);
                }
            }
        }
        motionScore = static_cast<double>(motionSum) / static_cast<double>(analysis.lumaSamples.size());
    }

    const double brightness = sampleCount > 0
        ? static_cast<double>(brightnessSum) / static_cast<double>(sampleCount)
        : 0.0;

    analysis.sample.brightness = brightness;
    analysis.sample.motionScore = motionScore;
    analysis.sample.faceDetected = brightness >= kMinimumBrightnessForPresence && motionScore >= kMinimumMotionForPresence;
    if (motionWeight > 0.0 && width > 0 && height > 0) {
        const double centroidX = weightedX / motionWeight;
        const double centroidY = weightedY / motionWeight;
        analysis.sample.faceX = qBound(-1.0, (centroidX / static_cast<double>(qMax(1, width - 1))) * 2.0 - 1.0, 1.0);
        analysis.sample.faceY = qBound(-1.0, (centroidY / static_cast<double>(qMax(1, height - 1))) * 2.0 - 1.0, 1.0);
    }
    analysis.sample.confidence = analysis.sample.faceDetected ? qMin(0.8, qMax(0.2, motionScore / 18.0)) : 0.0;
    analysis.sample.emotionLabel = analysis.sample.faceDetected ? QStringLiteral("neutral") : QStringLiteral("unknown");
    analysis.sample.emotionScore = analysis.sample.faceDetected ? 0.2 : 0.0;

    return analysis;
}
