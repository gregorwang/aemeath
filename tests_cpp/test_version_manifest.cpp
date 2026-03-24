#include <QtTest>

#include "runtime/version_manifest.h"

class VersionManifestTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void loadsLocalManifest();
    void parsesGitHubReleasePayload();
    void comparesVersionsNumerically();
};

void VersionManifestTest::loadsLocalManifest()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFile file(tempDir.filePath(QStringLiteral("version.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(R"({
        "version": "1.2.3",
        "update_url": "https://example.com/releases/latest",
        "build_date": "2026-03-24",
        "python_version": "3.12",
        "commit_hash": "abc123",
        "phase": "Phase X"
    })");
    file.close();

    const LocalVersionManifest manifest = VersionManifest::loadLocal(file.fileName());
    QCOMPARE(manifest.version, QStringLiteral("1.2.3"));
    QCOMPARE(manifest.updateUrl, QStringLiteral("https://example.com/releases/latest"));
    QCOMPARE(manifest.buildDate, QStringLiteral("2026-03-24"));
    QCOMPARE(manifest.pythonVersion, QStringLiteral("3.12"));
    QCOMPARE(manifest.commitHash, QStringLiteral("abc123"));
    QCOMPARE(manifest.phase, QStringLiteral("Phase X"));
    QVERIFY(manifest.isValid());
}

void VersionManifestTest::parsesGitHubReleasePayload()
{
    const QByteArray payload = R"({
        "tag_name": "v1.3.0",
        "name": "release-1.3.0",
        "html_url": "https://github.com/example/project/releases/tag/v1.3.0"
    })";

    const RemoteReleaseInfo release = VersionManifest::parseRemoteReleasePayload(payload);
    QCOMPARE(release.version, QStringLiteral("1.3.0"));
    QCOMPARE(release.releaseName, QStringLiteral("release-1.3.0"));
    QCOMPARE(release.htmlUrl, QStringLiteral("https://github.com/example/project/releases/tag/v1.3.0"));
    QVERIFY(release.isValid());
}

void VersionManifestTest::comparesVersionsNumerically()
{
    QCOMPARE(VersionManifest::compareVersions(QStringLiteral("1.2.3"), QStringLiteral("1.2.3")), 0);
    QVERIFY(VersionManifest::compareVersions(QStringLiteral("v1.2.3"), QStringLiteral("1.2.4")) < 0);
    QVERIFY(VersionManifest::compareVersions(QStringLiteral("1.10.0"), QStringLiteral("1.2.9")) > 0);
}

QTEST_MAIN(VersionManifestTest)

#include "test_version_manifest.moc"
