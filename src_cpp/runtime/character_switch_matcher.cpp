#include "runtime/character_switch_matcher.h"

#include <QStringList>
#include <QtGlobal>

namespace {

QString normalizeMatcherText(const QString &text)
{
    QString normalized = text.trimmed().toLower();
    if (normalized.isEmpty()) {
        return {};
    }

    static const QString punctuation = QStringLiteral(
        " \t\r\n,.!?;:'\"`~@#$%^&*()_+-=[]{}|\\<>/，。！？；：、（）【】《》“”‘’");
    QString compact;
    compact.reserve(normalized.size());
    for (const QChar ch : normalized) {
        if (!punctuation.contains(ch)) {
            compact.append(ch);
        }
    }
    return compact;
}

bool containsAny(const QString &text, const QStringList &phrases)
{
    for (const QString &phrase : phrases) {
        if (text.contains(phrase)) {
            return true;
        }
    }
    return false;
}

int bestAliasLength(const QString &normalizedTranscript, const CharacterManifest &manifest)
{
    int best = -1;
    for (const QString &alias : manifest.aliases) {
        const QString normalizedAlias = normalizeMatcherText(alias);
        if (!normalizedAlias.isEmpty() && normalizedTranscript.contains(normalizedAlias)) {
            best = qMax(best, normalizedAlias.size());
        }
    }
    return best;
}

}

CharacterManifest CharacterSwitchMatcher::match(const QString &transcript, const QVector<CharacterManifest> &manifests)
{
    const QString normalized = normalizeText(transcript);
    if (normalized.isEmpty()) {
        return {};
    }

    static const QStringList strongIntentMarkers = {
        QStringLiteral("切换角色"),
        QStringLiteral("切角色"),
        QStringLiteral("换角色"),
        QStringLiteral("角色切换"),
        QStringLiteral("使用角色"),
    };
    static const QStringList weakIntentMarkers = {
        QStringLiteral("切换到"),
        QStringLiteral("换成"),
        QStringLiteral("变成"),
    };

    const bool hasStrongIntent = containsAny(normalized, strongIntentMarkers);
    const bool hasWeakIntent = containsAny(normalized, weakIntentMarkers);
    if (!hasStrongIntent && !hasWeakIntent) {
        return {};
    }

    CharacterManifest bestMatch;
    int bestLength = -1;

    for (const CharacterManifest &manifest : manifests) {
        if (!manifest.isValid()) {
            continue;
        }

        const QString normalizedId = normalizeText(manifest.id);
        const QString normalizedName = normalizeText(manifest.name);
        const int aliasLength = bestAliasLength(normalized, manifest);
        const bool matchedId = !normalizedId.isEmpty() && normalized.contains(normalizedId);
        const bool matchedName = !normalizedName.isEmpty() && normalized.contains(normalizedName);
        const bool matchedAlias = aliasLength > 0;
        if (!matchedId && !matchedName && !matchedAlias) {
            continue;
        }

        if (!hasStrongIntent && normalizedName.isEmpty() && !matchedAlias) {
            // Weak commands like "换成..." only accept name matches, to avoid collisions
            // with unrelated toggle phrases when the character id is generic.
            continue;
        }

        const int matchLength = qMax(qMax(normalizedId.size(), normalizedName.size()), aliasLength);
        if (matchLength > bestLength) {
            bestMatch = manifest;
            bestLength = matchLength;
        }
    }

    return bestMatch;
}

QString CharacterSwitchMatcher::normalizeText(const QString &text)
{
    return normalizeMatcherText(text);
}
