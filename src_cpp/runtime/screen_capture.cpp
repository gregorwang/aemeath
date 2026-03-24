#include "runtime/screen_capture.h"

#include <QBuffer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

#ifdef Q_OS_WIN
#include <windows.h>
#include <vector>
#endif

QString ScreenCapture::captureForegroundWindowBase64Jpeg()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return {};
    }

#ifdef Q_OS_WIN
    WId targetWindow = 0;
    if (HWND hwnd = ::GetForegroundWindow()) {
        targetWindow = reinterpret_cast<WId>(hwnd);
    }
#else
    WId targetWindow = 0;
#endif

    QPixmap pixmap = screen->grabWindow(targetWindow);
    if (pixmap.isNull()) {
        pixmap = screen->grabWindow(0);
    }
    if (pixmap.isNull()) {
        return {};
    }

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return {};
    }
    if (!pixmap.save(&buffer, "JPG", 75)) {
        return {};
    }
    return QString::fromLatin1(bytes.toBase64());
}

ForegroundWindowContext ScreenCapture::captureForegroundWindowContext()
{
    ForegroundWindowContext context;

#ifdef Q_OS_WIN
    HWND hwnd = ::GetForegroundWindow();
    if (!hwnd) {
        return context;
    }

    wchar_t titleBuffer[512] = {};
    const int titleLength = ::GetWindowTextW(
        hwnd,
        titleBuffer,
        static_cast<int>(sizeof(titleBuffer) / sizeof(titleBuffer[0])));
    if (titleLength > 0) {
        context.title = QString::fromWCharArray(titleBuffer, titleLength).trimmed();
    }

    DWORD processId = 0;
    ::GetWindowThreadProcessId(hwnd, &processId);
    if (processId != 0) {
        HANDLE processHandle = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (processHandle) {
            std::vector<wchar_t> processPath(1024, L'\0');
            DWORD size = static_cast<DWORD>(processPath.size());
            if (::QueryFullProcessImageNameW(processHandle, 0, processPath.data(), &size) && size > 0) {
                const QString fullPath = QString::fromWCharArray(processPath.data(), static_cast<int>(size));
                context.processName = QFileInfo(fullPath).fileName().trimmed();
            }
            ::CloseHandle(processHandle);
        }
    }
#endif

    return context;
}
