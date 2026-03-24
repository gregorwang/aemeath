#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/voice_script_catalog.h"

class VoiceScriptCatalogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsDefaultScripts();
    void selectsPanicScript();
    void loadsAudioPathAndSpritePathFromJson();
    void prefersExactTimeRangeOverDefaultForIdle();
    void avoidsImmediateIdleRepeatWhenAlternativesExist();
    void timeRangeEndBoundaryIsExclusive();
    void loadsLegacyScriptsArrayAsIdleSource();
    void loadsTopLevelArrayAsIdleSource();
};

void VoiceScriptCatalogTest::loadsDefaultScripts()
{
    VoiceScriptCatalog catalog;

    QVERIFY(!catalog.idleScripts().isEmpty());
    QVERIFY(!catalog.panicScripts().isEmpty());
}

void VoiceScriptCatalogTest::selectsPanicScript()
{
    VoiceScriptCatalog catalog;
    const VoiceScriptEntry entry = catalog.selectPanicScript(QDateTime::currentDateTime());

    QVERIFY(!entry.id.trimmed().isEmpty());
    QVERIFY(!entry.text.trimmed().isEmpty());
    QCOMPARE(entry.eventType, QStringLiteral("panic"));
}

void VoiceScriptCatalogTest::loadsAudioPathAndSpritePathFromJson()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("voice_cache")));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("assets/sprites")));

    QFile audioFile(dir.filePath(QStringLiteral("voice_cache/idle.mp3")));
    QVERIFY(audioFile.open(QIODevice::WriteOnly));
    audioFile.write("stub");
    audioFile.close();

    QFile spriteFile(dir.filePath(QStringLiteral("assets/sprites/idle.gif")));
    QVERIFY(spriteFile.open(QIODevice::WriteOnly));
    spriteFile.write("gif");
    spriteFile.close();

    QFile scriptsFile(dir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"({
        "idle_events": [{
            "id": "idle_1",
            "text": "hello",
            "audio_cache": "voice_cache/idle.mp3",
            "sprite": "assets/sprites/idle.gif",
            "anim_speed": "slow",
            "tags": ["idle", "general"]
        }],
        "panic_events": []
    })");
    scriptsFile.close();

    VoiceScriptCatalog catalog(scriptsFile.fileName());
    const VoiceScriptEntry entry = catalog.selectIdleScript(QDateTime(QDate(2026, 3, 12), QTime(12, 0)));

    QCOMPARE(entry.id, QStringLiteral("idle_1"));
    QCOMPARE(QFileInfo(entry.audioPath).absoluteFilePath(), QFileInfo(audioFile).absoluteFilePath());
    QCOMPARE(QFileInfo(entry.spritePath).absoluteFilePath(), QFileInfo(spriteFile).absoluteFilePath());
    QCOMPARE(entry.animSpeed, QStringLiteral("slow"));
    QCOMPARE(entry.tags, QStringList({QStringLiteral("idle"), QStringLiteral("general")}));
}

void VoiceScriptCatalogTest::prefersExactTimeRangeOverDefaultForIdle()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile scriptsFile(dir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"({
        "idle_events": [
            {"id": "default_idle", "text": "default", "time_range": "default", "priority": 1},
            {"id": "lunch_idle", "text": "lunch", "time_range": "12:00-13:00", "priority": 2}
        ],
        "panic_events": []
    })");
    scriptsFile.close();

    VoiceScriptCatalog catalog(scriptsFile.fileName());
    const VoiceScriptEntry entry = catalog.selectIdleScript(QDateTime(QDate(2026, 3, 12), QTime(12, 30)));

    QCOMPARE(entry.id, QStringLiteral("lunch_idle"));
}

void VoiceScriptCatalogTest::avoidsImmediateIdleRepeatWhenAlternativesExist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile scriptsFile(dir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"({
        "idle_events": [
            {"id": "idle_a", "text": "A", "time_range": "default", "priority": 1, "probability": 1.0},
            {"id": "idle_b", "text": "B", "time_range": "default", "priority": 1, "probability": 1.0}
        ],
        "panic_events": []
    })");
    scriptsFile.close();

    VoiceScriptCatalog catalog(scriptsFile.fileName());
    const QDateTime now(QDate(2026, 3, 12), QTime(12, 0));
    const VoiceScriptEntry first = catalog.selectIdleScript(now);
    const VoiceScriptEntry second = catalog.selectIdleScript(now.addSecs(30));

    QVERIFY(!first.id.isEmpty());
    QVERIFY(!second.id.isEmpty());
    QVERIFY(first.id != second.id);
}

void VoiceScriptCatalogTest::timeRangeEndBoundaryIsExclusive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile scriptsFile(dir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"({
        "idle_events": [
            {"id": "lunch_idle", "text": "lunch", "time_range": "12:00-13:00", "priority": 1},
            {"id": "fallback_idle", "text": "fallback", "time_range": "default", "priority": 2}
        ],
        "panic_events": []
    })");
    scriptsFile.close();

    VoiceScriptCatalog catalog(scriptsFile.fileName());
    const VoiceScriptEntry atStart = catalog.selectIdleScript(QDateTime(QDate(2026, 3, 12), QTime(12, 0)));
    const VoiceScriptEntry atEnd = catalog.selectIdleScript(QDateTime(QDate(2026, 3, 12), QTime(13, 0)));

    QCOMPARE(atStart.id, QStringLiteral("lunch_idle"));
    QCOMPARE(atEnd.id, QStringLiteral("fallback_idle"));
}

void VoiceScriptCatalogTest::loadsLegacyScriptsArrayAsIdleSource()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile scriptsFile(dir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"({
        "scripts": [
            {"id": "legacy_idle", "text": "legacy", "time_range": "default", "priority": 2}
        ]
    })");
    scriptsFile.close();

    VoiceScriptCatalog catalog(scriptsFile.fileName());
    const VoiceScriptEntry entry = catalog.selectIdleScript(QDateTime(QDate(2026, 3, 12), QTime(10, 0)));

    QCOMPARE(entry.id, QStringLiteral("legacy_idle"));
    QVERIFY(catalog.panicScripts().size() >= 1);
}

void VoiceScriptCatalogTest::loadsTopLevelArrayAsIdleSource()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile scriptsFile(dir.filePath(QStringLiteral("scripts.json")));
    QVERIFY(scriptsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    scriptsFile.write(R"([
        {"id": "array_idle", "text": "array", "time_range": "default", "priority": 2}
    ])");
    scriptsFile.close();

    VoiceScriptCatalog catalog(scriptsFile.fileName());
    const VoiceScriptEntry entry = catalog.selectIdleScript(QDateTime(QDate(2026, 3, 12), QTime(10, 0)));

    QCOMPARE(entry.id, QStringLiteral("array_idle"));
}

QTEST_MAIN(VoiceScriptCatalogTest)

#include "test_voice_script_catalog.moc"
