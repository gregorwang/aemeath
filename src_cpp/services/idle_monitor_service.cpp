#include <algorithm>
#include "services/idle_monitor_service.h"

#include <QTimer>

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kPollIntervalMs = 100;
constexpr double kPreIdleRatio = 0.8;
constexpr int kActiveResetMs = 1000;

}

IdleMonitorServiceNative::IdleMonitorServiceNative(QObject *parent)
    : IdleMonitorService(parent)
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &IdleMonitorServiceNative::pollIdleState);
}

void IdleMonitorServiceNative::start()
{
    resetToStandby();
    m_pollTimer->start();
}

void IdleMonitorServiceNative::stop()
{
    m_pollTimer->stop();
    resetToStandby();
}

void IdleMonitorServiceNative::setThresholdMs(int thresholdMs)
{
    m_idleThresholdMs = qMax(1, thresholdMs);
}

void IdleMonitorServiceNative::resetToStandby()
{
    setState(IdleState::Standby);
}

void IdleMonitorServiceNative::pollIdleState()
{
    const qint64 idleMs = currentIdleMs();
    Q_EMIT idleTimeUpdated(idleMs);

    switch (m_state) {
    case IdleState::Standby:
        if (idleMs >= m_idleThresholdMs) {
            setState(IdleState::IdleTriggered);
            Q_EMIT idleDetected();
        } else if (idleMs >= static_cast<qint64>(m_idleThresholdMs * kPreIdleRatio)) {
            setState(IdleState::PreIdle);
        }
        break;
    case IdleState::PreIdle:
        if (idleMs >= m_idleThresholdMs) {
            setState(IdleState::IdleTriggered);
            Q_EMIT idleDetected();
        } else if (idleMs < kActiveResetMs) {
            setState(IdleState::Standby);
        }
        break;
    case IdleState::IdleTriggered:
        if (idleMs < kActiveResetMs) {
            setState(IdleState::Active);
            Q_EMIT activityDetected();
        }
        break;
    case IdleState::Active:
        if (idleMs < kActiveResetMs) {
            setState(IdleState::Standby);
        }
        break;
    }
}

qint64 IdleMonitorServiceNative::currentIdleMs() const
{
#ifdef Q_OS_WIN
    LASTINPUTINFO info;
    info.cbSize = sizeof(LASTINPUTINFO);
    if (::GetLastInputInfo(&info)) {
        return static_cast<qint64>(::GetTickCount64() - info.dwTime);
    }
#endif
    return 0;
}

void IdleMonitorServiceNative::setState(IdleState state)
{
    m_state = state;
}
