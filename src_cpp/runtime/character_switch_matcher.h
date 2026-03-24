#pragma once

#include <QString>
#include <QVector>

#include "runtime/character_manifest_catalog.h"

class CharacterSwitchMatcher
{
public:
    static CharacterManifest match(const QString &transcript, const QVector<CharacterManifest> &manifests);

private:
    static QString normalizeText(const QString &text);
};
