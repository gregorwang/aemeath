#pragma once

#include <QString>

struct ForegroundWindowContext
{
    QString title;
    QString processName;

    bool isValid() const
    {
        return !title.trimmed().isEmpty() || !processName.trimmed().isEmpty();
    }
};

class ScreenCapture
{
public:
    static QString captureForegroundWindowBase64Jpeg();
    static ForegroundWindowContext captureForegroundWindowContext();
};
