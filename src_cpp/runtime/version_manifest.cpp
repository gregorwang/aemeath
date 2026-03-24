#include "runtime/version_manifest.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "runtime/app_paths.h"

bool LocalVersionManifest::isValid() const
{
    return !version.trimmed().isEmpty() || !updateUrl.trimmed().isEmpty();
}

bool RemoteReleaseInfo::isValid() const
{
    return !version.trimmed().isEmpty() && !htmlUrl.trimmed().isEmpty();
}

QString VersionManifest::defaultUpdateUrl()
{
    return QStringLiteral("https://api.github.com/repos/gregorwang/aemeath/releases/latest");
}

QString VersionManifest::versionFilePath()
{
    return AppPaths::resolveOptionalAsset(QStringLiteral("version.json"));
}

LocalVersionManifest VersionManifest::loadLocal(const QString &path)
{
    const QString resolvedPath = path.trimmed().isEmpty() ? versionFilePath() : path.trimmed();
    QFile file(resolvedPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    LocalVersionManifest manifest;
    manifest.version = root.value(QStringLiteral("version")).toString().trimmed();
    manifest.updateUrl = root.value(QStringLiteral("update_url")).toString().trimmed();
    manifest.buildDate = root.value(QStringLiteral("build_date")).toString().trimmed();
    manifest.pythonVersion = root.value(QStringLiteral("python_version")).toString().trimmed();
    manifest.commitHash = root.value(QStringLiteral("commit_hash")).toString().trimmed();
    manifest.phase = root.value(QStringLiteral("phase")).toString().trimmed();
    return manifest;
}

RemoteReleaseInfo VersionManifest::parseRemoteReleasePayload(const QByteArray &payload)
{
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        return {};
    }

    const QJsonObject root = document.object();
    RemoteReleaseInfo info;
    info.releaseName = root.value(QStringLiteral("name")).toString().trimmed();
    info.htmlUrl = root.value(QStringLiteral("html_url")).toString().trimmed();

    const QString tagName = root.value(QStringLiteral("tag_name")).toString().trimmed();
    info.version = normalizeVersionString(tagName);
    if (info.version.isEmpty()) {
        info.version = normalizeVersionString(info.releaseName);
    }
    return info;
}

QString VersionManifest::normalizeVersionString(const QString &value)
{
    const QString cleaned = value.trimmed();
    if (cleaned.isEmpty()) {
        return {};
    }

    static const QRegularExpression numericPattern(QStringLiteral("(\\d+(?:\\.\\d+)*)"));
    const QRegularExpressionMatch match = numericPattern.match(cleaned);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    QString fallback = cleaned.toLower();
    fallback.remove(QRegularExpression(QStringLiteral("^[^a-z0-9]+")));
    return fallback;
}

int VersionManifest::compareVersions(const QString &left, const QString &right)
{
    const QString normalizedLeft = normalizeVersionString(left);
    const QString normalizedRight = normalizeVersionString(right);
    if (normalizedLeft == normalizedRight) {
        return 0;
    }

    const QStringList leftParts = normalizedLeft.split('.', Qt::SkipEmptyParts);
    const QStringList rightParts = normalizedRight.split('.', Qt::SkipEmptyParts);
    if (!leftParts.isEmpty() && !rightParts.isEmpty()) {
        const int maxSize = qMax(leftParts.size(), rightParts.size());
        for (int index = 0; index < maxSize; ++index) {
            bool leftOk = false;
            bool rightOk = false;
            const int leftValue = index < leftParts.size() ? leftParts.at(index).toInt(&leftOk) : 0;
            const int rightValue = index < rightParts.size() ? rightParts.at(index).toInt(&rightOk) : 0;
            if (leftOk && rightOk) {
                if (leftValue < rightValue) {
                    return -1;
                }
                if (leftValue > rightValue) {
                    return 1;
                }
                continue;
            }
            break;
        }
    }

    return QString::compare(normalizedLeft, normalizedRight, Qt::CaseInsensitive);
}
