#pragma once

#include <QString>

class ScriptedTrajectoryCatalog
{
public:
    static QString resolvePreferredTrajectory(
        const QString &rawPath,
        const QString &defaultFileName = QString());

    static QString scanDefaultDirectory(
        const QString &relativeDirectory,
        const QString &defaultFileName = QString());

    static bool isTrajectoryFileUsable(const QString &filePath);
};
