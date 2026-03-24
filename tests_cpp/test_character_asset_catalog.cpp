#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <QtTest>

#include "runtime/app_paths.h"
#include "runtime/character_asset_catalog.h"

class CharacterAssetCatalogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void registerAndResolveGif();
    void normalizeAliasesToCanonicalState();
    void scanCharacterDirectoryOverridesDefaultMappings();
    void appPathUrlsMatchExpectedProjectEndpoints();
    void recentLogTailReturnsLastRequestedLines();
};

void CharacterAssetCatalogTest::registerAndResolveGif()
{
    CharacterAssetCatalog catalog;
    catalog.registerGif(CharacterAssetCatalog::StateKey::Engaged, QStringLiteral("C:/tmp/state6.gif"));

    QVERIFY(catalog.hasGif(CharacterAssetCatalog::StateKey::Engaged));
    QCOMPARE(
        catalog.gifFor(CharacterAssetCatalog::StateKey::Engaged),
        QStringLiteral("C:/tmp/state6.gif"));
}

void CharacterAssetCatalogTest::normalizeAliasesToCanonicalState()
{
    CharacterAssetCatalog catalog;
    catalog.registerGif(QStringLiteral("state5"), QStringLiteral("C:/tmp/state5.gif"));
    catalog.registerGif(QStringLiteral("state8"), QStringLiteral("C:/tmp/aemeath.gif"));

    QCOMPARE(catalog.normalizeStateName(QStringLiteral("peeking")), QStringLiteral("state5"));
    QCOMPARE(catalog.normalizeStateName(QStringLiteral("hover")), QStringLiteral("state5"));
    QCOMPARE(catalog.gifForStateName(QStringLiteral("thinking")), QStringLiteral("C:/tmp/state5.gif"));
    QCOMPARE(catalog.normalizeStateName(QStringLiteral("aemeath")), QStringLiteral("state8"));
    QCOMPARE(catalog.gifForStateName(QStringLiteral("main")), QStringLiteral("C:/tmp/aemeath.gif"));
}

void CharacterAssetCatalogTest::scanCharacterDirectoryOverridesDefaultMappings()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("assets/sprites")));

    QFile idleFile(root.filePath(QStringLiteral("assets/sprites/idle.png")));
    QVERIFY(idleFile.open(QIODevice::WriteOnly));
    idleFile.write("png");
    idleFile.close();

    QFile panicFile(root.filePath(QStringLiteral("assets/sprites/panic.gif")));
    QVERIFY(panicFile.open(QIODevice::WriteOnly));
    panicFile.write("gif");
    panicFile.close();

    CharacterAssetCatalog catalog;
    catalog.registerGif(QStringLiteral("state1"), QStringLiteral("C:/tmp/default-state1.gif"));
    catalog.scanCharacterDirectory(root.absolutePath(), root.filePath(QStringLiteral("assets/sprites/idle.png")));

    QCOMPARE(QFileInfo(catalog.gifForStateName(QStringLiteral("idle"))).absoluteFilePath(), QFileInfo(idleFile).absoluteFilePath());
    QCOMPARE(QFileInfo(catalog.gifForStateName(QStringLiteral("fleeing"))).absoluteFilePath(), QFileInfo(panicFile).absoluteFilePath());
    QCOMPARE(QFileInfo(catalog.gifForStateName(QStringLiteral("main"))).absoluteFilePath(), QFileInfo(idleFile).absoluteFilePath());
}

void CharacterAssetCatalogTest::appPathUrlsMatchExpectedProjectEndpoints()
{
    QCOMPARE(
        AppPaths::quickStartGuideUrl().toString(),
        QStringLiteral("https://github.com/gregorwang/aemeath#-快速上手"));
    QCOMPARE(
        AppPaths::feedbackIssueUrl().toString(),
        QStringLiteral("https://github.com/gregorwang/aemeath/issues/new"));
}

void CharacterAssetCatalogTest::recentLogTailReturnsLastRequestedLines()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qputenv("LOCALAPPDATA", tempDir.path().toUtf8());
    QFile logFile(AppPaths::logFilePath());
    QVERIFY(logFile.open(QIODevice::WriteOnly | QIODevice::Text));
    logFile.write("line1\nline2\nline3\nline4\n");
    logFile.close();

    QCOMPARE(AppPaths::recentLogTail(2), QStringLiteral("line3\nline4"));
    QCOMPARE(AppPaths::recentLogTail(10), QStringLiteral("line1\nline2\nline3\nline4"));

    qunsetenv("LOCALAPPDATA");
}

QTEST_MAIN(CharacterAssetCatalogTest)

#include "test_character_asset_catalog.moc"
