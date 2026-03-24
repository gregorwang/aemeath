#pragma once

#include <QDir>
#include <QIcon>
#include <QString>
#include <QUrl>

class AppPaths
{
public:
    static QDir baseDir();
    static QDir userDataDir();
    static QDir legacyUserDataDir();
    static QDir logDir();
    static QDir ttsCacheDir();
    static QString logFilePath();
    static QString recentLogTail(int maxLines = 50);
    static QString configFilePath();
    static QString legacyConfigFilePath();
    static QString resolveOptionalAsset(const QString &relativePath);
    static QIcon resolveTrayIcon();
    static QUrl quickStartGuideUrl();
    static QUrl feedbackIssueUrl();
};
