#pragma once

#include <QByteArray>
#include <QString>

struct LocalVersionManifest
{
    QString version;
    QString updateUrl;
    QString buildDate;
    QString pythonVersion;
    QString commitHash;
    QString phase;

    bool isValid() const;
};

struct RemoteReleaseInfo
{
    QString version;
    QString htmlUrl;
    QString releaseName;

    bool isValid() const;
};

class VersionManifest
{
public:
    static QString defaultUpdateUrl();
    static QString versionFilePath();
    static LocalVersionManifest loadLocal(const QString &path = QString());
    static RemoteReleaseInfo parseRemoteReleasePayload(const QByteArray &payload);
    static QString normalizeVersionString(const QString &value);
    static int compareVersions(const QString &left, const QString &right);
};
