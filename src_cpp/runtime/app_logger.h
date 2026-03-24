#pragma once

#include <QString>

class AppLogger
{
public:
    static void initialize(const QString &logFilePath, bool debugMode);
};
