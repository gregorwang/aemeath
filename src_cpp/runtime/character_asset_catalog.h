#pragma once

#include <QHash>
#include <QString>

class CharacterAssetCatalog
{
public:
    enum class StateKey {
        Idle,
        Peeking,
        Engaged,
        Fleeing,
        Commentary
    };

    CharacterAssetCatalog();

    void clear();
    void scanDefaultLocations();
    void scanCharacterDirectory(const QString &characterRoot, const QString &previewImagePath = QString());
    void registerGif(StateKey key, const QString &path);
    void registerGif(const QString &stateName, const QString &path);
    QString gifFor(StateKey key) const;
    QString gifForStateName(const QString &stateName) const;
    bool hasGif(StateKey key) const;
    QString normalizeStateName(const QString &stateName) const;

private:
    static QString keyName(StateKey key);

    QHash<QString, QString> m_gifPaths;
};
