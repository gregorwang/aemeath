#pragma once

#include <QSettings>
#include <QString>

class AutoStartManager
{
public:
    explicit AutoStartManager(
        QString settingsLocation = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::Format format = QSettings::NativeFormat,
        QString valueName = QStringLiteral("CyberCompanionCpp"));

    bool isEnabled() const;
    QString currentCommand() const;
    bool syncEnabled(bool enabled, const QString &applicationPath, bool startMinimized, QString *errorMessage = nullptr) const;

    static QString buildCommand(const QString &applicationPath, bool startMinimized = false);

private:
    QSettings createSettings() const;

    QString m_settingsLocation;
    QSettings::Format m_format = QSettings::NativeFormat;
    QString m_valueName;
};
