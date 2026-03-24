#include <QtTest>

#include "runtime/character_switch_matcher.h"

class CharacterSwitchMatcherTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void matchesStrongIntentByName();
    void matchesWeakIntentByName();
    void matchesAlias();
    void ignoresTranscriptWithoutSwitchIntent();
};

void CharacterSwitchMatcherTest::matchesStrongIntentByName()
{
    const QVector<CharacterManifest> manifests = {
        CharacterManifest{ QStringLiteral("default"), QStringLiteral("默认角色"), QStringLiteral("root/default") },
        CharacterManifest{ QStringLiteral("beta"), QStringLiteral("测试助手"), QStringLiteral("root/beta") },
    };

    const CharacterManifest match =
        CharacterSwitchMatcher::match(QStringLiteral("请切换角色到测试助手"), manifests);

    QVERIFY(match.isValid());
    QCOMPARE(match.id, QStringLiteral("beta"));
}

void CharacterSwitchMatcherTest::matchesWeakIntentByName()
{
    const QVector<CharacterManifest> manifests = {
        CharacterManifest{ QStringLiteral("default"), QStringLiteral("默认角色"), QStringLiteral("root/default") },
        CharacterManifest{ QStringLiteral("aemeath"), QStringLiteral("小爱同学"), QStringLiteral("root/aemeath") },
    };

    const CharacterManifest match =
        CharacterSwitchMatcher::match(QStringLiteral("换成小爱同学"), manifests);

    QVERIFY(match.isValid());
    QCOMPARE(match.id, QStringLiteral("aemeath"));
}

void CharacterSwitchMatcherTest::matchesAlias()
{
    CharacterManifest manifest;
    manifest.id = QStringLiteral("aemeath");
    manifest.name = QStringLiteral("Aemeath");
    manifest.rootDir = QStringLiteral("root/aemeath");
    manifest.aliases = { QStringLiteral("小爱同学"), QStringLiteral("桌宠") };

    const QVector<CharacterManifest> manifests = { manifest };
    const CharacterManifest match =
        CharacterSwitchMatcher::match(QStringLiteral("切换角色到桌宠"), manifests);

    QVERIFY(match.isValid());
    QCOMPARE(match.id, QStringLiteral("aemeath"));
}

void CharacterSwitchMatcherTest::ignoresTranscriptWithoutSwitchIntent()
{
    const QVector<CharacterManifest> manifests = {
        CharacterManifest{ QStringLiteral("default"), QStringLiteral("默认角色"), QStringLiteral("root/default") },
    };

    const CharacterManifest match =
        CharacterSwitchMatcher::match(QStringLiteral("默认角色你好"), manifests);

    QVERIFY(!match.isValid());
}

QTEST_MAIN(CharacterSwitchMatcherTest)

#include "test_character_switch_matcher.moc"
