#include "services/vision_service.h"

#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QImage>
#include <QMediaDevices>
#include <QSize>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtGlobal>
#include <QLoggingCategory>
#include <limits>

#include "runtime/vision_frame_analyzer.h"

namespace {

constexpr int kPreferredWidth = 320;
constexpr int kPreferredHeight = 240;

QCameraFormat chooseCameraFormat(const QCameraDevice &device, int targetFps)
{
    QCameraFormat bestFormat;
    int bestScore = std::numeric_limits<int>::max();
    for (const QCameraFormat &format : device.videoFormats()) {
        const QSize resolution = format.resolution();
        const float minFps = format.minFrameRate();
        const float maxFps = format.maxFrameRate();
        if (resolution.isEmpty()) {
            continue;
        }
        const int resolutionScore = qAbs(resolution.width() - kPreferredWidth) + qAbs(resolution.height() - kPreferredHeight);
        int fpsPenalty = 0;
        if (targetFps < static_cast<int>(minFps)) {
            fpsPenalty = static_cast<int>(minFps) - targetFps;
        } else if (targetFps > static_cast<int>(maxFps + 0.5f)) {
            fpsPenalty = targetFps - static_cast<int>(maxFps + 0.5f);
        } else {
            const double centerFps = (static_cast<double>(minFps) + static_cast<double>(maxFps)) / 2.0;
            fpsPenalty = qAbs(static_cast<int>(centerFps + 0.5) - targetFps);
        }
        const int score = resolutionScore + fpsPenalty * 20;
        if (score < bestScore) {
            bestScore = score;
            bestFormat = format;
        }
    }
    return bestFormat;
}

}

QtVisionService::QtVisionService(const VisionServiceOptions &options, QObject *parent)
    : VisionService(parent)
    , m_options(options)
    , m_videoSink(new QVideoSink(this))
{
    m_options.cameraIndex = qMax(0, m_options.cameraIndex);
    m_options.targetFps = qBound(1, m_options.targetFps, 30);
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &QtVisionService::onVideoFrameChanged);
}

QtVisionService::~QtVisionService()
{
    stop();
}

void QtVisionService::configureCamera(int cameraIndex, int targetFps)
{
    m_options.cameraIndex = qMax(0, cameraIndex);
    m_options.targetFps = qBound(1, targetFps, 30);
    restartIfRunning();
}

void QtVisionService::start()
{
    if (m_running) {
        return;
    }
    if (!initializeCamera()) {
        return;
    }

    m_previousLuma.clear();
    m_lastProcessedMs = -1;
    m_lastFaceDetected = false;
    m_frameClock.invalidate();
    if (m_camera) {
        m_camera->start();
    }
    m_running = true;
}

void QtVisionService::stop()
{
    if (!m_running && !m_camera) {
        return;
    }

    if (m_camera) {
        m_camera->stop();
        delete m_camera;
        m_camera = nullptr;
    }
    m_captureSession.setCamera(nullptr);
    m_captureSession.setVideoSink(nullptr);
    m_previousLuma.clear();
    m_lastProcessedMs = -1;
    const bool wasRunning = m_running;
    const bool wasActive = m_cameraActive;
    m_running = false;
    m_cameraActive = false;
    if (wasActive || wasRunning) {
        Q_EMIT cameraStateChanged(false);
    }
}

void QtVisionService::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (!m_running || !frame.isValid()) {
        return;
    }

    if (!m_frameClock.isValid()) {
        m_frameClock.start();
    }
    const qint64 nowMs = m_frameClock.elapsed();
    const qint64 minFrameIntervalMs = qMax<qint64>(1, 1000 / qMax(1, m_options.targetFps));
    if (m_lastProcessedMs >= 0 && (nowMs - m_lastProcessedMs) < minFrameIntervalMs) {
        return;
    }

    const QImage image = frame.toImage();
    if (image.isNull()) {
        return;
    }
    m_lastProcessedMs = nowMs;

    const GazeSample sample = buildSample(image);
    if (sample.faceDetected != m_lastFaceDetected) {
        m_lastFaceDetected = sample.faceDetected;
        Q_EMIT faceDetected(sample.faceDetected);
    }
    Q_EMIT gazeUpdated(sample);
}

void QtVisionService::restartIfRunning()
{
    if (!m_running) {
        return;
    }
    stop();
    start();
}

bool QtVisionService::initializeCamera()
{
    const QList<QCameraDevice> devices = QMediaDevices::videoInputs();
    if (devices.isEmpty()) {
        emitCameraError(QStringLiteral("当前系统未检测到可用摄像头设备。"));
        return false;
    }

    const int selectedIndex = qBound(0, m_options.cameraIndex, devices.size() - 1);
    const QCameraDevice selectedDevice = devices.at(selectedIndex);

    if (m_camera) {
        delete m_camera;
        m_camera = nullptr;
    }
    m_camera = new QCamera(selectedDevice, this);
    qInfo().noquote() << "[VisionService] selected camera index=" << selectedIndex
                      << "description=" << selectedDevice.description()
                      << "target_fps=" << m_options.targetFps;
    connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &message) {
        emitCameraError(message.trimmed().isEmpty()
            ? QStringLiteral("摄像头运行时发生错误。")
            : message.trimmed());
    });
    connect(m_camera, &QCamera::activeChanged, this, [this](bool active) {
        if (m_cameraActive == active) {
            return;
        }
        m_cameraActive = active;
        qInfo().noquote() << "[VisionService] active=" << active;
        Q_EMIT cameraStateChanged(active);
    });
    const QCameraFormat bestFormat = chooseCameraFormat(selectedDevice, m_options.targetFps);
    if (!bestFormat.isNull()) {
        m_camera->setCameraFormat(bestFormat);
        qInfo().noquote() << "[VisionService] using format"
                          << bestFormat.resolution().width() << "x" << bestFormat.resolution().height()
                          << "fps=[" << bestFormat.minFrameRate() << "," << bestFormat.maxFrameRate() << "]";
    } else {
        qWarning().noquote() << "[VisionService] no preferred camera format found, falling back to device default";
    }
    m_captureSession.setVideoSink(m_videoSink);
    m_captureSession.setCamera(m_camera);
    return true;
}

GazeSample QtVisionService::buildSample(const QImage &image)
{
    const VisionFrameAnalysis analysis = VisionFrameAnalyzer::analyze(image, m_previousLuma);
    m_previousLuma = analysis.lumaSamples;
    return analysis.sample;
}

void QtVisionService::emitCameraError(const QString &message)
{
    m_running = false;
    m_cameraActive = false;
    Q_EMIT cameraStateChanged(false);
    Q_EMIT cameraError(message);
}
