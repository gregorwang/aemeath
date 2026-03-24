#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct CharacterManifest
{
    QString id;
    QString name;
    QString rootDir;
    QString manifestPath;
    QString scriptsPath;
    QString previewImagePath;
    QString defaultVoice;
    QStringList aliases;

    bool isValid() const
    {
        return !id.trimmed().isEmpty() && !rootDir.trimmed().isEmpty();
    }
};

class CharacterManifestCatalog
{
public:
    explicit CharacterManifestCatalog(const QString &charactersRoot = QString());

    void reload();
    QVector<CharacterManifest> manifests() const;
    CharacterManifest findById(const QString &characterId) const;
    QString charactersRoot() const;

private:
    QString resolvedCharactersRoot() const;

    QString m_charactersRoot;
    QVector<CharacterManifest> m_manifests;
};
