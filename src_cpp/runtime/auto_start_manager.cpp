#include "runtime/auto_start_manager.h"

#include <QDir>
#include <QFileInfo>

namespace {

QString normalizedExecutablePath(const QString &applicationPath)
{
    const QFileInfo fileInfo(applicationPath.trimmed());
    if (fileInfo.exists()) {
        return QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    }
    return QDir::toNativeSeparators(applicationPath.trimmed());
}

}

AutoStartManager::AutoStartManager(QString settingsLocation, QSettings::Format format, QString valueName)
    : m_settingsLocation(std::move(settingsLocation))
    , m_format(format)
    , m_valueName(std::move(valueName))
{
}

bool AutoStartManager::isEnabled() const
{
    return !currentCommand().trimmed().isEmpty();
}

QString AutoStartManager::currentCommand() const
{
    const QSettings settings = createSettings();
    return settings.value(m_valueName).toString().trimmed();
}

bool AutoStartManager::syncEnabled(bool enabled, const QString &applicationPath, bool startMinimized, QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }

    QSettings settings = createSettings();
    if (enabled) {
        const QString command = buildCommand(applicationPath, startMinimized);
        if (command.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("应用路径为空，无法写入开机自启动。");
            }
            return false;
        }
        settings.setValue(m_valueName, command);
    } else {
        settings.remove(m_valueName);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (errorMessage) {
            *errorMessage = enabled
                ? QStringLiteral("写入开机自启动项失败。")
                : QStringLiteral("移除开机自启动项失败。");
        }
        return false;
    }
    return true;
}

QString AutoStartManager::buildCommand(const QString &applicationPath, bool startMinimized)
{
    const QString normalizedPath = normalizedExecutablePath(applicationPath);
    if (normalizedPath.trimmed().isEmpty()) {
        return {};
    }
    QStringList parts;
    parts.append(QStringLiteral("\"%1\"").arg(normalizedPath));
    parts.append(QStringLiteral("--autostart"));
    if (startMinimized) {
        parts.append(QStringLiteral("--start-minimized"));
    }
    return parts.join(QLatin1Char(' '));
}

QSettings AutoStartManager::createSettings() const
{
    return QSettings(m_settingsLocation, m_format);
}
