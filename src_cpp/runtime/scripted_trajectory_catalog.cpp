#include "runtime/scripted_trajectory_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "runtime/app_paths.h"

namespace {

QString normalizeRelativeCandidate(const QString &rawPath)
{
    return rawPath.trimmed();
}

bool containsUsableTrajectoryPayload(const QByteArray &payload)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("keyframes")).toArray().size() >= 2) {
        return true;
    }
    if (root.value(QStringLiteral("points")).toArray().size() >= 2) {
        return true;
    }
    return false;
}

QString resolveDirectCandidate(const QString &candidatePath, const QString &defaultFileName)
{
    const QFileInfo info(candidatePath);
    if (!info.exists()) {
        return QString();
    }
    if (info.isFile() && ScriptedTrajectoryCatalog::isTrajectoryFileUsable(info.absoluteFilePath())) {
        return info.absoluteFilePath();
    }
    if (!info.isDir()) {
        return QString();
    }

    const QDir directory(info.absoluteFilePath());
    if (!defaultFileName.trimmed().isEmpty()) {
        const QString preferredPath = directory.filePath(defaultFileName);
        if (ScriptedTrajectoryCatalog::isTrajectoryFileUsable(preferredPath)) {
            return QFileInfo(preferredPath).absoluteFilePath();
        }
    }

    const QFileInfoList preferred = directory.entryInfoList(
        QStringList() << QStringLiteral("*_qt_animation.json"),
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::Reversed);
    for (const QFileInfo &entry : preferred) {
        if (ScriptedTrajectoryCatalog::isTrajectoryFileUsable(entry.absoluteFilePath())) {
            return entry.absoluteFilePath();
        }
    }

    const QFileInfoList fallback = directory.entryInfoList(
        QStringList() << QStringLiteral("trajectory_*.json"),
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::Reversed);
    for (const QFileInfo &entry : fallback) {
        if (ScriptedTrajectoryCatalog::isTrajectoryFileUsable(entry.absoluteFilePath())) {
            return entry.absoluteFilePath();
        }
    }

    return QString();
}

} // namespace

QString ScriptedTrajectoryCatalog::resolvePreferredTrajectory(
    const QString &rawPath,
    const QString &defaultFileName)
{
    const QString trimmed = normalizeRelativeCandidate(rawPath);
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QString directResolved = resolveDirectCandidate(trimmed, defaultFileName);
    if (!directResolved.isEmpty()) {
        return directResolved;
    }

    const QString assetResolved = AppPaths::resolveOptionalAsset(trimmed);
    if (assetResolved.isEmpty()) {
        return QString();
    }

    const QFileInfo assetInfo(assetResolved);
    if (assetInfo.isFile()) {
        return isTrajectoryFileUsable(assetInfo.absoluteFilePath())
            ? assetInfo.absoluteFilePath()
            : QString();
    }

    if (!assetInfo.isDir()) {
        return QString();
    }

    if (!defaultFileName.trimmed().isEmpty()) {
        const QString preferred = QDir(assetInfo.absoluteFilePath()).filePath(defaultFileName);
        if (isTrajectoryFileUsable(preferred)) {
            return QFileInfo(preferred).absoluteFilePath();
        }
    }

    return resolveDirectCandidate(assetInfo.absoluteFilePath(), defaultFileName);
}

QString ScriptedTrajectoryCatalog::scanDefaultDirectory(
    const QString &relativeDirectory,
    const QString &defaultFileName)
{
    const QString relative = normalizeRelativeCandidate(relativeDirectory);
    if (relative.isEmpty()) {
        return QString();
    }

    if (!defaultFileName.trimmed().isEmpty()) {
        const QString preferred = AppPaths::resolveOptionalAsset(relative + QStringLiteral("/") + defaultFileName);
        if (isTrajectoryFileUsable(preferred)) {
            return preferred;
        }
    }

    return resolvePreferredTrajectory(relative, defaultFileName);
}

bool ScriptedTrajectoryCatalog::isTrajectoryFileUsable(const QString &filePath)
{
    if (filePath.trimmed().isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    return containsUsableTrajectoryPayload(file.readAll());
}
