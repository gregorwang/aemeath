#include "runtime/character_manifest_catalog.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "runtime/app_paths.h"

namespace {

QString resolveRelativeFile(const QString &baseDir, const QString &relativePath)
{
    const QString cleaned = relativePath.trimmed();
    if (cleaned.isEmpty()) {
        return {};
    }

    const QFileInfo info(cleaned);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    return QFileInfo(QDir(baseDir), cleaned).absoluteFilePath();
}

CharacterManifest parseManifestFile(const QString &manifestPath)
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return {};
    }

    const QFileInfo manifestInfo(manifestPath);
    const QDir rootDir = manifestInfo.dir();
    const QJsonObject object = document.object();

    CharacterManifest manifest;
    manifest.id = object.value(QStringLiteral("id")).toString(rootDir.dirName()).trimmed();
    manifest.name = object.value(QStringLiteral("name")).toString(manifest.id).trimmed();
    manifest.rootDir = rootDir.absolutePath();
    manifest.manifestPath = manifestInfo.absoluteFilePath();
    manifest.defaultVoice = object.value(QStringLiteral("default_voice")).toString().trimmed();
    const QJsonArray aliases = object.value(QStringLiteral("aliases")).toArray();
    for (const QJsonValue &value : aliases) {
        const QString alias = value.toString().trimmed();
        if (!alias.isEmpty() && !manifest.aliases.contains(alias, Qt::CaseInsensitive)) {
            manifest.aliases.append(alias);
        }
    }
    manifest.previewImagePath = resolveRelativeFile(
        rootDir.absolutePath(),
        object.value(QStringLiteral("preview_image")).toString());

    const QString explicitScripts = object.value(QStringLiteral("scripts_path")).toString().trimmed();
    if (!explicitScripts.isEmpty()) {
        manifest.scriptsPath = resolveRelativeFile(rootDir.absolutePath(), explicitScripts);
    } else {
        const QString defaultScriptsPath = rootDir.filePath(QStringLiteral("scripts.json"));
        if (QFileInfo::exists(defaultScriptsPath)) {
            manifest.scriptsPath = QFileInfo(defaultScriptsPath).absoluteFilePath();
        }
    }

    if (manifest.id.isEmpty()) {
        manifest.id = rootDir.dirName().trimmed();
    }
    if (manifest.name.isEmpty()) {
        manifest.name = manifest.id;
    }
    return manifest;
}

} // namespace

CharacterManifestCatalog::CharacterManifestCatalog(const QString &charactersRoot)
    : m_charactersRoot(charactersRoot.trimmed())
{
    reload();
}

void CharacterManifestCatalog::reload()
{
    m_manifests.clear();

    const QString rootPath = resolvedCharactersRoot();
    if (rootPath.isEmpty()) {
        return;
    }

    const QDir rootDir(rootPath);
    const QFileInfoList directories = rootDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);

    for (const QFileInfo &entry : directories) {
        const QString manifestPath = entry.absoluteFilePath() + QStringLiteral("/manifest.json");
        if (!QFileInfo::exists(manifestPath)) {
            continue;
        }

        const CharacterManifest manifest = parseManifestFile(manifestPath);
        if (manifest.isValid()) {
            m_manifests.push_back(manifest);
        }
    }

    std::sort(m_manifests.begin(), m_manifests.end(), [](const CharacterManifest &left, const CharacterManifest &right) {
        if (left.id.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0) {
            return right.id.compare(QStringLiteral("default"), Qt::CaseInsensitive) != 0;
        }
        if (right.id.compare(QStringLiteral("default"), Qt::CaseInsensitive) == 0) {
            return false;
        }
        const int byName = QString::compare(left.name, right.name, Qt::CaseInsensitive);
        if (byName != 0) {
            return byName < 0;
        }
        return QString::compare(left.id, right.id, Qt::CaseInsensitive) < 0;
    });
}

QVector<CharacterManifest> CharacterManifestCatalog::manifests() const
{
    return m_manifests;
}

CharacterManifest CharacterManifestCatalog::findById(const QString &characterId) const
{
    const QString target = characterId.trimmed();
    for (const CharacterManifest &manifest : m_manifests) {
        if (manifest.id.compare(target, Qt::CaseInsensitive) == 0) {
            return manifest;
        }
    }
    return {};
}

QString CharacterManifestCatalog::charactersRoot() const
{
    return resolvedCharactersRoot();
}

QString CharacterManifestCatalog::resolvedCharactersRoot() const
{
    if (!m_charactersRoot.isEmpty()) {
        return m_charactersRoot;
    }
    return AppPaths::resolveOptionalAsset(QStringLiteral("characters"));
}
