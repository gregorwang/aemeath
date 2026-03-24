#include "runtime/app_paths.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

namespace {

QDir ensureDir(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir;
}

QString preferredUserDataPath()
{
#ifdef Q_OS_WIN
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        return localAppData + QStringLiteral("/CyberCompanionCpp");
    }
#endif

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        return appData;
    }

    return QDir::homePath() + QStringLiteral("/.cybercompanioncpp");
}

QString findRelativeAssetPath(const QString &relativePath)
{
    QDir probe(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 6; ++depth) {
        const QString candidate = probe.filePath(relativePath);
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
        if (!probe.cdUp()) {
            break;
        }
    }
    return QString();
}

} // namespace

QDir AppPaths::baseDir()
{
    return QDir(QCoreApplication::applicationDirPath());
}

QDir AppPaths::userDataDir()
{
    return ensureDir(preferredUserDataPath());
}

QDir AppPaths::legacyUserDataDir()
{
#ifdef Q_OS_WIN
    const QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (!localAppData.isEmpty()) {
        return ensureDir(localAppData + QStringLiteral("/CyberCompanion"));
    }
#endif
    return ensureDir(userDataDir().filePath(QStringLiteral("legacy")));
}

QDir AppPaths::logDir()
{
    return ensureDir(userDataDir().filePath(QStringLiteral("logs")));
}

QDir AppPaths::ttsCacheDir()
{
    return ensureDir(userDataDir().filePath(QStringLiteral("tts-cache")));
}

QString AppPaths::logFilePath()
{
    return logDir().filePath(QStringLiteral("app.log"));
}

QString AppPaths::recentLogTail(int maxLines)
{
    const int safeMaxLines = qMax(1, maxLines);
    QFile file(logFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    const QString payload = QString::fromUtf8(file.readAll());
    QStringList lines = payload.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::KeepEmptyParts);
    while (!lines.isEmpty() && lines.constLast().isEmpty()) {
        lines.removeLast();
    }
    if (lines.size() > safeMaxLines) {
        lines = lines.mid(lines.size() - safeMaxLines);
    }
    return lines.join(QStringLiteral("\n"));
}

QString AppPaths::configFilePath()
{
    return userDataDir().filePath(QStringLiteral("config.json"));
}

QString AppPaths::legacyConfigFilePath()
{
    return legacyUserDataDir().filePath(QStringLiteral("config.json"));
}

QString AppPaths::resolveOptionalAsset(const QString &relativePath)
{
    return findRelativeAssetPath(relativePath);
}

QIcon AppPaths::resolveTrayIcon()
{
    const QString iconPath = findRelativeAssetPath(QStringLiteral("assets/icon.ico"));
    if (!iconPath.isEmpty()) {
        return QIcon(iconPath);
    }
    return QIcon();
}

QUrl AppPaths::quickStartGuideUrl()
{
    return QUrl(QStringLiteral("https://github.com/gregorwang/aemeath#-快速上手"));
}

QUrl AppPaths::feedbackIssueUrl()
{
    return QUrl(QStringLiteral("https://github.com/gregorwang/aemeath/issues/new"));
}
