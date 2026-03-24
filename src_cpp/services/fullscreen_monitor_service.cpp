#include "services/fullscreen_monitor_service.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kPollIntervalMs = 1000;
constexpr int kRectTolerancePx = 2;

#ifdef Q_OS_WIN
bool rectApproximatelyMatchesMonitor(const RECT &windowRect, const RECT &monitorRect)
{
    return qAbs(windowRect.left - monitorRect.left) <= kRectTolerancePx
        && qAbs(windowRect.top - monitorRect.top) <= kRectTolerancePx
        && qAbs(windowRect.right - monitorRect.right) <= kRectTolerancePx
        && qAbs(windowRect.bottom - monitorRect.bottom) <= kRectTolerancePx;
}
#endif

}

WindowsFullscreenMonitorService::WindowsFullscreenMonitorService(QObject *parent)
    : FullscreenMonitorService(parent)
{
    m_timer.setInterval(kPollIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &WindowsFullscreenMonitorService::poll);
}

bool WindowsFullscreenMonitorService::isFullscreenActive() const
{
    return m_isFullscreenActive;
}

void WindowsFullscreenMonitorService::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    poll();
    m_timer.start();
}

void WindowsFullscreenMonitorService::stop()
{
    m_timer.stop();
    m_running = false;
}

void WindowsFullscreenMonitorService::poll()
{
    const bool fullscreenNow = detectFullscreen();
    if (fullscreenNow == m_isFullscreenActive) {
        return;
    }
    m_isFullscreenActive = fullscreenNow;
    Q_EMIT fullscreenChanged(m_isFullscreenActive);
}

bool WindowsFullscreenMonitorService::detectFullscreen() const
{
#ifdef Q_OS_WIN
    HWND foregroundWindow = GetForegroundWindow();
    if (foregroundWindow == nullptr || !IsWindowVisible(foregroundWindow) || IsIconic(foregroundWindow)) {
        return false;
    }

    RECT windowRect{};
    if (!GetWindowRect(foregroundWindow, &windowRect)) {
        return false;
    }

    HMONITOR monitor = MonitorFromWindow(foregroundWindow, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr) {
        return false;
    }

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfoW(monitor, &monitorInfo)) {
        return false;
    }

    return rectApproximatelyMatchesMonitor(windowRect, monitorInfo.rcMonitor);
#else
    return false;
#endif
}
