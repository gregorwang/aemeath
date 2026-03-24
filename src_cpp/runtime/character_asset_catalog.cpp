#include "runtime/character_asset_catalog.h"

#include <QDir>
#include <QFileInfo>

#include "runtime/app_paths.h"

namespace {

QString canonicalStateName(const QString &stateName)
{
    const QString key = stateName.trimmed().toLower();
    if (key == QStringLiteral("state1") || key == QStringLiteral("idle") || key == QStringLiteral("curious")
        || key == QStringLiteral("ambient") || key == QStringLiteral("probe")) {
        return QStringLiteral("state1");
    }
    if (key == QStringLiteral("state2") || key == QStringLiteral("excited") || key == QStringLiteral("commentary")) {
        return QStringLiteral("state2");
    }
    if (key == QStringLiteral("state3") || key == QStringLiteral("roam") || key == QStringLiteral("roaming")
        || key == QStringLiteral("moving")) {
        return QStringLiteral("state3");
    }
    if (key == QStringLiteral("state4") || key == QStringLiteral("flee") || key == QStringLiteral("fleeing")
        || key == QStringLiteral("shy")) {
        return QStringLiteral("state4");
    }
    if (key == QStringLiteral("state5") || key == QStringLiteral("hover") || key == QStringLiteral("thinking")
        || key == QStringLiteral("peeking") || key == QStringLiteral("peek")) {
        return QStringLiteral("state5");
    }
    if (key == QStringLiteral("state6") || key == QStringLiteral("greeting") || key == QStringLiteral("happy")
        || key == QStringLiteral("engaged")) {
        return QStringLiteral("state6");
    }
    if (key == QStringLiteral("state8") || key == QStringLiteral("aemeath") || key == QStringLiteral("main")) {
        return QStringLiteral("state8");
    }
    return {};
}

}

CharacterAssetCatalog::CharacterAssetCatalog() = default;

void CharacterAssetCatalog::clear()
{
    m_gifPaths.clear();
}

void CharacterAssetCatalog::scanDefaultLocations()
{
    const struct Mapping {
        const char *stateName;
        const char *relativePath;
    } mappings[] = {
        {"state1", "characters/state1.gif"},
        {"state2", "characters/state2.gif"},
        {"state3", "characters/state3.gif"},
        {"state4", "characters/state4.gif"},
        {"state5", "characters/state5.gif"},
        {"state6", "characters/state6.gif"},
        {"state8", "characters/aemeath.gif"},
    };

    for (const Mapping &mapping : mappings) {
        const QString resolved = AppPaths::resolveOptionalAsset(QString::fromLatin1(mapping.relativePath));
        if (!resolved.isEmpty()) {
            registerGif(QString::fromLatin1(mapping.stateName), resolved);
        }
    }
}

void CharacterAssetCatalog::scanCharacterDirectory(const QString &characterRoot, const QString &previewImagePath)
{
    const QDir root(characterRoot.trimmed());
    if (!root.exists()) {
        return;
    }

    const struct Mapping {
        const char *stateName;
        const char *candidates[6];
    } mappings[] = {
        {"state1", {"state1.gif", "assets/sprites/state1.gif", "assets/sprites/idle.gif", "assets/sprites/idle.png", nullptr, nullptr}},
        {"state4", {"state4.gif", "assets/sprites/state4.gif", "assets/sprites/panic.gif", "assets/sprites/panic.png", nullptr, nullptr}},
        {"state5", {"state5.gif", "assets/sprites/state5.gif", "assets/sprites/peek.gif", "assets/sprites/peek.png", nullptr, nullptr}},
        {"state8", {"aemeath.gif", "assets/sprites/aemeath.gif", "assets/sprites/aemeath.png", nullptr, nullptr, nullptr}},
    };

    for (const Mapping &mapping : mappings) {
        for (const char *candidate : mapping.candidates) {
            if (!candidate) {
                break;
            }
            const QFileInfo info(root.filePath(QString::fromLatin1(candidate)));
            if (info.exists()) {
                registerGif(QString::fromLatin1(mapping.stateName), info.absoluteFilePath());
                break;
            }
        }
    }

    const QFileInfo preview(previewImagePath.trimmed());
    if (preview.exists()) {
        registerGif(QStringLiteral("state8"), preview.absoluteFilePath());
    }
}

void CharacterAssetCatalog::registerGif(StateKey key, const QString &path)
{
    registerGif(keyName(key), path);
}

void CharacterAssetCatalog::registerGif(const QString &stateName, const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    const QString normalized = normalizeStateName(stateName);
    if (normalized.isEmpty()) {
        return;
    }
    m_gifPaths.insert(normalized, path);
}

QString CharacterAssetCatalog::gifFor(StateKey key) const
{
    return gifForStateName(keyName(key));
}

QString CharacterAssetCatalog::gifForStateName(const QString &stateName) const
{
    const QString normalized = normalizeStateName(stateName);
    if (normalized.isEmpty()) {
        return {};
    }
    return m_gifPaths.value(normalized);
}

bool CharacterAssetCatalog::hasGif(StateKey key) const
{
    return !gifFor(key).isEmpty();
}

QString CharacterAssetCatalog::normalizeStateName(const QString &stateName) const
{
    return canonicalStateName(stateName);
}

QString CharacterAssetCatalog::keyName(StateKey key)
{
    switch (key) {
    case StateKey::Idle:
        return QStringLiteral("state1");
    case StateKey::Peeking:
        return QStringLiteral("state5");
    case StateKey::Engaged:
        return QStringLiteral("state6");
    case StateKey::Fleeing:
        return QStringLiteral("state4");
    case StateKey::Commentary:
        return QStringLiteral("state2");
    }
    return QStringLiteral("state1");
}
