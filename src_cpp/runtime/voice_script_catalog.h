#pragma once

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVector>

struct VoiceScriptEntry
{
    QString id;
    QString text;
    QString audioPath;
    QString spritePath;
    QString animSpeed = QStringLiteral("normal");
    int priority = 2;
    QString timeRange = QStringLiteral("default");
    double probability = 1.0;
    int cooldownMinutes = 0;
    QStringList tags;
    QString eventType = QStringLiteral("idle");
};

class VoiceScriptCatalog
{
public:
    explicit VoiceScriptCatalog(const QString &scriptsPath = QString());

    void loadDefault();
    void setScriptsPath(const QString &scriptsPath);
    QString scriptsPath() const;
    QVector<VoiceScriptEntry> idleScripts() const;
    QVector<VoiceScriptEntry> panicScripts() const;
    VoiceScriptEntry selectIdleScript(const QDateTime &now);
    VoiceScriptEntry selectPanicScript(const QDateTime &now);

private:
    void loadFromJsonFile(const QString &filePath);
    static QVector<VoiceScriptEntry> buildBuiltinIdleScripts();
    static QVector<VoiceScriptEntry> buildBuiltinPanicScripts();
    static bool matchesTimeRange(const QString &timeRange, const QTime &time);
    static bool isDefaultRange(const QString &timeRange);
    bool isCoolingDown(const VoiceScriptEntry &entry, const QDateTime &now) const;
    VoiceScriptEntry pickScript(
        const QVector<VoiceScriptEntry> &scripts,
        const QDateTime &now,
        bool honorCooldown,
        bool avoidImmediateRepeat);

    QVector<VoiceScriptEntry> m_idleScripts;
    QVector<VoiceScriptEntry> m_panicScripts;
    QHash<QString, QDateTime> m_lastTriggeredAt;
    QString m_lastIdleScriptId;
    QString m_scriptsPath;
};
