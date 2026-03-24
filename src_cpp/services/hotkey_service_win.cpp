#include "services/hotkey_service_win.h"

#include <QApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int kSummonHotkeyId = 1;
constexpr int kPushToTalkHotkeyId = 2;
}

WindowsHotkeyService::WindowsHotkeyService(QApplication &app, QObject *parent)
    : HotkeyService(parent)
    , m_app(app)
{
}

WindowsHotkeyService::~WindowsHotkeyService()
{
    stop();
}

bool WindowsHotkeyService::isSummonRegistered() const
{
    return m_summonRegistered;
}

bool WindowsHotkeyService::isPushToTalkRegistered() const
{
    return m_pushToTalkRegistered;
}

void WindowsHotkeyService::start()
{
    if (m_started) {
        return;
    }

#ifdef Q_OS_WIN
    m_app.installNativeEventFilter(this);
    m_summonRegistered = ::RegisterHotKey(nullptr, kSummonHotkeyId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x53);
    m_pushToTalkRegistered = ::RegisterHotKey(nullptr, kPushToTalkHotkeyId, MOD_CONTROL | MOD_NOREPEAT, 0x42);
    qInfo() << "[HotkeyService]" << "summon=" << m_summonRegistered << "ptt=" << m_pushToTalkRegistered;
#else
    m_summonRegistered = false;
    m_pushToTalkRegistered = false;
    qInfo() << "[HotkeyService] global hotkeys are only implemented on Windows";
#endif

    m_started = true;
}

void WindowsHotkeyService::stop()
{
    if (!m_started) {
        return;
    }

#ifdef Q_OS_WIN
    ::UnregisterHotKey(nullptr, kSummonHotkeyId);
    ::UnregisterHotKey(nullptr, kPushToTalkHotkeyId);
    m_app.removeNativeEventFilter(this);
#endif

    m_summonRegistered = false;
    m_pushToTalkRegistered = false;
    m_started = false;
}

bool WindowsHotkeyService::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)

#ifndef Q_OS_WIN
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    return false;
#else
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
        return false;
    }

    MSG *msg = static_cast<MSG *>(message);
    if (!msg || msg->message != WM_HOTKEY) {
        return false;
    }

    if (static_cast<int>(msg->wParam) == kSummonHotkeyId) {
        Q_EMIT summonRequested();
        return true;
    }
    if (static_cast<int>(msg->wParam) == kPushToTalkHotkeyId) {
        Q_EMIT pushToTalkRequested();
        return true;
    }
    return false;
#endif
}
