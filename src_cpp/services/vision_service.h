#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QMediaCaptureSession>
#include <QObject>

#include "services/service_contracts.h"

class QCamera;
class QImage;
class QVideoFrame;
class QVideoSink;

struct VisionServiceOptions
{
    int cameraIndex = 0;
    int targetFps = 15;
};

class QtVisionService final : public VisionService
{
    Q_OBJECT

public:
    explicit QtVisionService(const VisionServiceOptions &options = {}, QObject *parent = nullptr);
    ~QtVisionService() override;

public Q_SLOTS:
    void configureCamera(int cameraIndex, int targetFps) override;
    void start() override;
    void stop() override;

private Q_SLOTS:
    void onVideoFrameChanged(const QVideoFrame &frame);

private:
    void restartIfRunning();
    bool initializeCamera();
    GazeSample buildSample(const QImage &image);
    void emitCameraError(const QString &message);

    VisionServiceOptions m_options;
    QMediaCaptureSession m_captureSession;
    QCamera *m_camera = nullptr;
    QVideoSink *m_videoSink = nullptr;
    QElapsedTimer m_frameClock;
    qint64 m_lastProcessedMs = -1;
    QByteArray m_previousLuma;
    bool m_running = false;
    bool m_cameraActive = false;
    bool m_lastFaceDetected = false;
};
