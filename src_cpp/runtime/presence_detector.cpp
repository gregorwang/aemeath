#include "runtime/presence_detector.h"

#include <QDateTime>

PresenceDetector::PresenceDetector(PresenceDetectorOptions options)
    : m_options(options)
{
    setTargetFps(m_options.referenceFps);
}

void PresenceDetector::setTargetFps(int targetFps)
{
    m_targetFps = qBound(1, targetFps, 60);
    m_lastSampleMs = -1;
}

PresenceState PresenceDetector::determinePresence(qint64 idleTimeMs, const GazeSample *sample)
{
    if (idleTimeMs < 60000) {
        m_lastSampleMs = -1;
        return PresenceState::PresentActive;
    }

    if (!sample) {
        m_lastSampleMs = -1;
        return PresenceState::Unknown;
    }
    return determinePresence(idleTimeMs, *sample);
}

PresenceState PresenceDetector::determinePresence(qint64 idleTimeMs, const GazeSample &sample)
{
    if (idleTimeMs < 60000) {
        m_lastSampleMs = -1;
        return PresenceState::PresentActive;
    }

    const double elapsedSeconds = sampleSeconds();
    const double motionScore = qMax(0.0, sample.motionScore);
    const double brightness = qMax(0.0, sample.brightness);

    if (!sample.faceDetected) {
        m_faceAbsentSeconds += elapsedSeconds;
        if (motionScore <= m_options.stillnessMotionThreshold) {
            m_stillNoFaceSeconds += elapsedSeconds;
        } else {
            m_stillNoFaceSeconds = 0.0;
        }
        if (brightness <= m_options.darkBrightnessThreshold) {
            m_darkNoFaceSeconds += elapsedSeconds;
        } else {
            m_darkNoFaceSeconds = 0.0;
        }
    } else {
        m_faceAbsentSeconds = 0.0;
        m_stillNoFaceSeconds = 0.0;
        m_darkNoFaceSeconds = 0.0;
    }

    if (idleTimeMs >= m_options.idleThresholdMs) {
        if (m_faceAbsentSeconds >= frameWindowSeconds(m_options.faceAbsentFrames)) {
            return PresenceState::Absent;
        }
        if (m_stillNoFaceSeconds >= frameWindowSeconds(m_options.stillNoFaceFrames)) {
            return PresenceState::Absent;
        }
        if (m_darkNoFaceSeconds >= frameWindowSeconds(m_options.darkNoFaceFrames)) {
            return PresenceState::Absent;
        }
        if (sample.faceDetected) {
            return PresenceState::PresentPassive;
        }
    }

    return PresenceState::Unknown;
}

double PresenceDetector::frameWindowSeconds(int frames) const
{
    return qMax(0.0, static_cast<double>(frames) / static_cast<double>(qMax(1, m_options.referenceFps)));
}

double PresenceDetector::sampleSeconds()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const double baselineSeconds = 1.0 / static_cast<double>(qMax(1, m_targetFps));
    if (m_lastSampleMs < 0) {
        m_lastSampleMs = nowMs;
        return baselineSeconds;
    }

    const qint64 elapsedMs = qMax<qint64>(0, nowMs - m_lastSampleMs);
    m_lastSampleMs = nowMs;
    const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;
    return qMax(baselineSeconds, qMin(elapsedSeconds, 1.0));
}
