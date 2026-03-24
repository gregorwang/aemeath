#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/character_manifest_catalog.h"

class CharacterManifestCatalogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scanCharactersDirectory();
    void findByIdReturnsMatchingManifest();
};

void CharacterManifestCatalogTest::scanCharactersDirectory()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("default")));
    QVERIFY(root.mkpath(QStringLiteral("beta")));

    QFile defaultManifest(root.filePath(QStringLiteral("default/manifest.json")));
    QVERIFY(defaultManifest.open(QIODevice::WriteOnly | QIODevice::Text));
    defaultManifest.write(
        "{"
        "\"id\":\"default\","
        "\"name\":\"Default Companion\","
        "\"aliases\":[\"小爱同学\",\"桌宠\"],"
        "\"default_voice\":\"voice-a\","
        "\"preview_image\":\"assets/sprites/aemeath.gif\""
        "}");
    defaultManifest.close();

    QFile defaultScripts(root.filePath(QStringLiteral("default/scripts.json")));
    QVERIFY(defaultScripts.open(QIODevice::WriteOnly | QIODevice::Text));
    defaultScripts.write("{\"idle_events\":[]}");
    defaultScripts.close();

    QFile betaManifest(root.filePath(QStringLiteral("beta/manifest.json")));
    QVERIFY(betaManifest.open(QIODevice::WriteOnly | QIODevice::Text));
    betaManifest.write("{\"id\":\"beta\",\"name\":\"Beta Companion\"}");
    betaManifest.close();

    CharacterManifestCatalog catalog(tempDir.path());
    const QVector<CharacterManifest> manifests = catalog.manifests();

    QCOMPARE(manifests.size(), 2);
    QCOMPARE(manifests.constFirst().id, QStringLiteral("default"));
    QCOMPARE(manifests.constFirst().defaultVoice, QStringLiteral("voice-a"));
    QCOMPARE(manifests.constFirst().aliases.size(), 2);
    QCOMPARE(manifests.constFirst().aliases.constFirst(), QStringLiteral("小爱同学"));
    QVERIFY(manifests.constFirst().scriptsPath.endsWith(QStringLiteral("default/scripts.json")));
    QVERIFY(manifests.constFirst().previewImagePath.endsWith(QStringLiteral("default/assets/sprites/aemeath.gif")));
}

void CharacterManifestCatalogTest::findByIdReturnsMatchingManifest()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("alpha")));
    QFile manifestFile(root.filePath(QStringLiteral("alpha/manifest.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
    manifestFile.write("{\"id\":\"Alpha\",\"name\":\"Alpha Companion\"}");
    manifestFile.close();

    CharacterManifestCatalog catalog(tempDir.path());
    const CharacterManifest manifest = catalog.findById(QStringLiteral("alpha"));
    QVERIFY(manifest.isValid());
    QCOMPARE(manifest.name, QStringLiteral("Alpha Companion"));
}

QTEST_MAIN(CharacterManifestCatalogTest)

#include "test_character_manifest_catalog.moc"
