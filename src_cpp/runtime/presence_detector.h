#pragma once

#include <QtGlobal>

#include "services/service_contracts.h"

enum class PresenceState {
    PresentActive,
    PresentPassive,
    Absent,
    Unknown,
};

struct PresenceDetectorOptions
{
    int referenceFps = 15;
    int idleThresholdMs = 300000;
    int faceAbsentFrames = 30;
    double stillnessMotionThreshold = 5.0;
    int stillNoFaceFrames = 20;
    double darkBrightnessThreshold = 30.0;
    int darkNoFaceFrames = 24;
};

class PresenceDetector
{
public:
    explicit PresenceDetector(PresenceDetectorOptions options = {});

    void setTargetFps(int targetFps);
    PresenceState determinePresence(qint64 idleTimeMs, const GazeSample *sample);
    PresenceState determinePresence(qint64 idleTimeMs, const GazeSample &sample);

private:
    double frameWindowSeconds(int frames) const;
    double sampleSeconds();

    PresenceDetectorOptions m_options;
    int m_targetFps = 15;
    double m_faceAbsentSeconds = 0.0;
    double m_stillNoFaceSeconds = 0.0;
    double m_darkNoFaceSeconds = 0.0;
    qint64 m_lastSampleMs = -1;
};
