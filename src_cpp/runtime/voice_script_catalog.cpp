#include "runtime/voice_script_catalog.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QtGlobal>

#include "runtime/app_paths.h"

namespace {

QString resolveOptionalPath(const QString &baseFilePath, const QString &value)
{
    const QString cleaned = value.trimmed();
    if (cleaned.isEmpty()) {
        return {};
    }
    if (cleaned.startsWith(QStringLiteral(":/"))) {
        return cleaned;
    }

    const QFileInfo info(cleaned);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }

    const QFileInfo baseInfo(baseFilePath);
    return QFileInfo(baseInfo.dir(), cleaned).absoluteFilePath();
}

VoiceScriptEntry parseScript(const QJsonObject &object, const QString &eventType, int defaultPriority)
{
    VoiceScriptEntry entry;
    entry.id = object.value(QStringLiteral("id")).toString().trimmed();
    entry.text = object.value(QStringLiteral("text")).toString().trimmed();
    entry.audioPath = resolveOptionalPath(
        object.value(QStringLiteral("__source_path")).toString(),
        object.value(QStringLiteral("audio_cache")).toString(object.value(QStringLiteral("audio_path")).toString()));
    entry.spritePath = resolveOptionalPath(
        object.value(QStringLiteral("__source_path")).toString(),
        object.value(QStringLiteral("sprite")).toString());
    entry.animSpeed = object.value(QStringLiteral("anim_speed")).toString(QStringLiteral("normal")).trimmed();
    entry.priority = object.value(QStringLiteral("priority")).toInt(defaultPriority);
    entry.timeRange = object.value(QStringLiteral("time_range")).toString(QStringLiteral("default")).trimmed();
    entry.probability = qMax(0.01, object.value(QStringLiteral("probability")).toDouble(1.0));
    entry.cooldownMinutes = object.value(QStringLiteral("cooldown_minutes")).toInt(0);
    const QJsonArray tags = object.value(QStringLiteral("tags")).toArray();
    for (const QJsonValue &tag : tags) {
        const QString text = tag.toString().trimmed();
        if (!text.isEmpty()) {
            entry.tags.append(text);
        }
    }
    entry.eventType = eventType;
    return entry;
}

bool isValidEntry(const VoiceScriptEntry &entry)
{
    return !entry.id.isEmpty() && !entry.text.isEmpty();
}

}

VoiceScriptCatalog::VoiceScriptCatalog(const QString &scriptsPath)
    : m_scriptsPath(scriptsPath.trimmed())
{
    loadDefault();
}

void VoiceScriptCatalog::loadDefault()
{
    m_idleScripts.clear();
    m_panicScripts.clear();
    m_lastTriggeredAt.clear();
    m_lastIdleScriptId.clear();

    const QString scriptsPath = !m_scriptsPath.isEmpty()
        ? m_scriptsPath
        : AppPaths::resolveOptionalAsset(QStringLiteral("characters/default/scripts.json"));
    if (!scriptsPath.isEmpty()) {
        loadFromJsonFile(scriptsPath);
    }

    if (m_idleScripts.isEmpty()) {
        m_idleScripts = buildBuiltinIdleScripts();
    }
    if (m_panicScripts.isEmpty()) {
        m_panicScripts = buildBuiltinPanicScripts();
    }
}

void VoiceScriptCatalog::setScriptsPath(const QString &scriptsPath)
{
    m_scriptsPath = scriptsPath.trimmed();
    loadDefault();
}

QString VoiceScriptCatalog::scriptsPath() const
{
    return m_scriptsPath;
}

QVector<VoiceScriptEntry> VoiceScriptCatalog::idleScripts() const
{
    return m_idleScripts;
}

QVector<VoiceScriptEntry> VoiceScriptCatalog::panicScripts() const
{
    return m_panicScripts;
}

VoiceScriptEntry VoiceScriptCatalog::selectIdleScript(const QDateTime &now)
{
    return pickScript(m_idleScripts, now, true, true);
}

VoiceScriptEntry VoiceScriptCatalog::selectPanicScript(const QDateTime &now)
{
    return pickScript(m_panicScripts, now, false, false);
}

void VoiceScriptCatalog::loadFromJsonFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (document.isNull()) {
        return;
    }

    QJsonArray idleEvents;
    QJsonArray panicEvents;
    if (document.isArray()) {
        idleEvents = document.array();
    } else if (document.isObject()) {
        const QJsonObject root = document.object();
        idleEvents = root.value(QStringLiteral("idle_events")).toArray();
        if (idleEvents.isEmpty()) {
            const QJsonValue scriptsValue = root.value(QStringLiteral("scripts"));
            if (scriptsValue.isArray()) {
                idleEvents = scriptsValue.toArray();
            }
        }
        panicEvents = root.value(QStringLiteral("panic_events")).toArray();
    } else {
        return;
    }

    for (const QJsonValue &value : idleEvents) {
        if (!value.isObject()) {
            continue;
        }
        QJsonObject object = value.toObject();
        object.insert(QStringLiteral("__source_path"), filePath);
        const VoiceScriptEntry entry = parseScript(object, QStringLiteral("idle"), 2);
        if (isValidEntry(entry)) {
            m_idleScripts.append(entry);
        }
    }

    for (const QJsonValue &value : panicEvents) {
        if (!value.isObject()) {
            continue;
        }
        QJsonObject object = value.toObject();
        object.insert(QStringLiteral("__source_path"), filePath);
        const VoiceScriptEntry entry = parseScript(object, QStringLiteral("panic"), 1);
        if (isValidEntry(entry)) {
            m_panicScripts.append(entry);
        }
    }
}

QVector<VoiceScriptEntry> VoiceScriptCatalog::buildBuiltinIdleScripts()
{
    return {
        VoiceScriptEntry{
            QStringLiteral("morning_default"),
            QStringLiteral("早上好，要不要先喝口水？"),
            QString(),
            QString(),
            QStringLiteral("normal"),
            2,
            QStringLiteral("05:00-11:00"),
            1.0,
            10,
            {},
            QStringLiteral("idle"),
        },
        VoiceScriptEntry{
            QStringLiteral("afternoon_default"),
            QStringLiteral("午后效率时间到了，继续推进吧。"),
            QString(),
            QString(),
            QStringLiteral("normal"),
            2,
            QStringLiteral("11:00-18:00"),
            1.0,
            10,
            {},
            QStringLiteral("idle"),
        },
        VoiceScriptEntry{
            QStringLiteral("night_default"),
            QStringLiteral("已经很晚了，注意休息。"),
            QString(),
            QString(),
            QStringLiteral("normal"),
            1,
            QStringLiteral("22:00-06:00"),
            1.0,
            20,
            {},
            QStringLiteral("idle"),
        },
        VoiceScriptEntry{
            QStringLiteral("fallback_default"),
            QStringLiteral("我在屏幕边缘看着你。"),
            QString(),
            QString(),
            QStringLiteral("normal"),
            3,
            QStringLiteral("default"),
            1.0,
            5,
            {},
            QStringLiteral("idle"),
        },
    };
}

QVector<VoiceScriptEntry> VoiceScriptCatalog::buildBuiltinPanicScripts()
{
    return {
        VoiceScriptEntry{
            QStringLiteral("panic_default"),
            QStringLiteral("哇！被发现了！"),
            QString(),
            QString(),
            QStringLiteral("normal"),
            1,
            QStringLiteral("default"),
            0.6,
            0,
            {},
            QStringLiteral("panic"),
        },
        VoiceScriptEntry{
            QStringLiteral("panic_shy"),
            QStringLiteral("才...才没有在偷看你..."),
            QString(),
            QString(),
            QStringLiteral("normal"),
            1,
            QStringLiteral("default"),
            0.4,
            0,
            {},
            QStringLiteral("panic"),
        },
    };
}

bool VoiceScriptCatalog::matchesTimeRange(const QString &timeRange, const QTime &time)
{
    const QString normalized = timeRange.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("default")) {
        return true;
    }

    const QStringList parts = normalized.split('-');
    if (parts.size() != 2) {
        return false;
    }

    const QTime start = QTime::fromString(parts.at(0), QStringLiteral("HH:mm"));
    const QTime end = QTime::fromString(parts.at(1), QStringLiteral("HH:mm"));
    if (!start.isValid() || !end.isValid()) {
        return false;
    }

    if (start <= end) {
        return time >= start && time < end;
    }
    return time >= start || time < end;
}

bool VoiceScriptCatalog::isDefaultRange(const QString &timeRange)
{
    const QString normalized = timeRange.trimmed().toLower();
    return normalized.isEmpty() || normalized == QStringLiteral("default");
}

bool VoiceScriptCatalog::isCoolingDown(const VoiceScriptEntry &entry, const QDateTime &now) const
{
    if (entry.cooldownMinutes <= 0) {
        return false;
    }

    const QDateTime last = m_lastTriggeredAt.value(entry.id);
    if (!last.isValid()) {
        return false;
    }
    return last.secsTo(now) < entry.cooldownMinutes * 60;
}

VoiceScriptEntry VoiceScriptCatalog::pickScript(
    const QVector<VoiceScriptEntry> &scripts,
    const QDateTime &now,
    bool honorCooldown,
    bool avoidImmediateRepeat)
{
    QVector<VoiceScriptEntry> exactMatches;
    QVector<VoiceScriptEntry> defaultMatches;
    for (const VoiceScriptEntry &entry : scripts) {
        if (!matchesTimeRange(entry.timeRange, now.time())) {
            continue;
        }
        if (isDefaultRange(entry.timeRange)) {
            defaultMatches.append(entry);
        } else {
            exactMatches.append(entry);
        }
    }

    QVector<VoiceScriptEntry> primaryPool = exactMatches.isEmpty()
        ? (!defaultMatches.isEmpty() ? defaultMatches : scripts)
        : exactMatches;
    if (primaryPool.isEmpty()) {
        return {};
    }

    auto filterPool = [&](const QVector<VoiceScriptEntry> &pool, bool allowRepeat) {
        QVector<VoiceScriptEntry> filtered;
        for (const VoiceScriptEntry &entry : pool) {
            if (honorCooldown && isCoolingDown(entry, now)) {
                continue;
            }
            if (avoidImmediateRepeat && !allowRepeat && !m_lastIdleScriptId.isEmpty()
                && entry.id == m_lastIdleScriptId && pool.size() > 1) {
                continue;
            }
            filtered.append(entry);
        }
        return filtered;
    };

    QVector<VoiceScriptEntry> candidates = filterPool(primaryPool, false);
    if (candidates.isEmpty() && !exactMatches.isEmpty() && !defaultMatches.isEmpty()) {
        candidates = filterPool(defaultMatches, false);
    }
    if (candidates.isEmpty()) {
        candidates = filterPool(primaryPool, true);
    }
    if (candidates.isEmpty()) {
        candidates = primaryPool;
    }

    int bestPriority = candidates.first().priority;
    for (const VoiceScriptEntry &entry : candidates) {
        bestPriority = qMin(bestPriority, entry.priority);
    }

    QVector<VoiceScriptEntry> topCandidates;
    for (const VoiceScriptEntry &entry : candidates) {
        if (entry.priority == bestPriority) {
            topCandidates.append(entry);
        }
    }
    if (topCandidates.isEmpty()) {
        topCandidates = candidates;
    }

    double totalWeight = 0.0;
    for (const VoiceScriptEntry &entry : topCandidates) {
        totalWeight += qMax(0.01, entry.probability);
    }

    int index = 0;
    if (topCandidates.size() > 1 && totalWeight > 0.0) {
        double threshold = QRandomGenerator::global()->generateDouble() * totalWeight;
        for (int candidateIndex = 0; candidateIndex < topCandidates.size(); ++candidateIndex) {
            threshold -= qMax(0.01, topCandidates.at(candidateIndex).probability);
            if (threshold <= 0.0) {
                index = candidateIndex;
                break;
            }
        }
    }

    const VoiceScriptEntry selected = topCandidates.at(index);
    m_lastTriggeredAt.insert(selected.id, now);
    if (selected.eventType == QStringLiteral("idle")) {
        m_lastIdleScriptId = selected.id;
    }
    return selected;
}
