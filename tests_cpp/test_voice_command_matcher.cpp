#include <QtTest>

#include "runtime/voice_command_matcher.h"

class VoiceCommandMatcherTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void normalizeTextRemovesPunctuation();
    void matchesScreenCommentaryPhrase();
    void matchesSummonPhrase();
    void matchesPeekPhrase();
    void matchesOpenSettingsPhrase();
    void matchesOpenGuidePhrase();
    void matchesEditScriptsPhrase();
    void matchesCameraScanDebugPhrase();
    void matchesCopyLogsPhrase();
    void matchesCheckUpdatesPhrase();
    void matchesOpenConfigPhrase();
    void matchesOpenDataDirPhrase();
    void matchesOpenLogsPhrase();
    void matchesFeedbackPhrase();
    void matchesAboutPhrase();
    void matchesDndOnPhrase();
    void matchesOfflineModePhrase();
    void matchesWakeupOffPhrase();
    void matchesEyeTrackingOnPhrase();
    void matchesPeriodicScanOffPhrase();
    void matchesAudioReactiveOnPhrase();
    void matchesVoiceModeContinuousPhrase();
    void matchesScriptedEntranceOnPhrase();
    void matchesFullscreenPauseOffPhrase();
    void matchesIdleInvasionOnPhrase();
    void matchesAutoCommentaryOffPhrase();
    void matchesReloadCharactersPhrase();
    void matchesReloadScriptsPhrase();
    void detectsScreenIntent();
    void returnsEmptyForUnknownCommand();
};

void VoiceCommandMatcherTest::normalizeTextRemovesPunctuation()
{
    QCOMPARE(
        VoiceCommandMatcher::normalizeText(QStringLiteral("  看，看 屏幕！ ")),
        QStringLiteral("看看屏幕"));
}

void VoiceCommandMatcherTest::matchesScreenCommentaryPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我看看屏幕上是什么"));
    QCOMPARE(match.action, QStringLiteral("screen_commentary"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesSummonPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("请你现身一下"));
    QCOMPARE(match.action, QStringLiteral("summon"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesPeekPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("你先出来看一眼"));
    QCOMPARE(match.action, QStringLiteral("peek"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesOpenSettingsPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我打开设置"));
    QCOMPARE(match.action, QStringLiteral("open_settings"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesOpenGuidePhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在打开使用指南"));
    QCOMPARE(match.action, QStringLiteral("open_guide"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesEditScriptsPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我编辑台词"));
    QCOMPARE(match.action, QStringLiteral("edit_scripts"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesCameraScanDebugPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在调试摄像头巡检"));
    QCOMPARE(match.action, QStringLiteral("camera_scan_debug"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesCopyLogsPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我复制最近日志"));
    QCOMPARE(match.action, QStringLiteral("copy_recent_logs"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesCheckUpdatesPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在检查更新"));
    QCOMPARE(match.action, QStringLiteral("check_updates"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesOpenConfigPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在打开配置文件"));
    QCOMPARE(match.action, QStringLiteral("open_config"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesOpenDataDirPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我打开数据目录"));
    QCOMPARE(match.action, QStringLiteral("open_data_dir"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesOpenLogsPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在打开日志目录"));
    QCOMPARE(match.action, QStringLiteral("open_logs"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesFeedbackPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我反馈问题"));
    QCOMPARE(match.action, QStringLiteral("feedback"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesAboutPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在打开关于"));
    QCOMPARE(match.action, QStringLiteral("about"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesDndOnPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("请帮我开启请勿打扰"));
    QCOMPARE(match.action, QStringLiteral("dnd_on"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesOfflineModePhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("请帮我开启离线模式"));
    QCOMPARE(match.action, QStringLiteral("offline_mode_on"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesWakeupOffPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我关闭语音唤醒"));
    QCOMPARE(match.action, QStringLiteral("wakeup_off"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesEyeTrackingOnPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在开启视线跟踪"));
    QCOMPARE(match.action, QStringLiteral("eye_tracking_on"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesPeriodicScanOffPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我关闭周期巡检"));
    QCOMPARE(match.action, QStringLiteral("periodic_scan_off"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesAudioReactiveOnPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在开启音频反应"));
    QCOMPARE(match.action, QStringLiteral("audio_reactive_on"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesVoiceModeContinuousPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我切到连续唤醒"));
    QCOMPARE(match.action, QStringLiteral("voice_mode_continuous"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesScriptedEntranceOnPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在开启脚本式登场"));
    QCOMPARE(match.action, QStringLiteral("scripted_entrance_on"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesFullscreenPauseOffPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我关闭全屏暂停"));
    QCOMPARE(match.action, QStringLiteral("fullscreen_pause_off"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesIdleInvasionOnPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在开启空闲入侵"));
    QCOMPARE(match.action, QStringLiteral("idle_invasion_on"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesAutoCommentaryOffPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在关闭自动评论"));
    QCOMPARE(match.action, QStringLiteral("auto_commentary_off"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesReloadCharactersPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("帮我重载角色"));
    QCOMPARE(match.action, QStringLiteral("reload_characters"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::matchesReloadScriptsPhrase()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("现在重载台词"));
    QCOMPARE(match.action, QStringLiteral("reload_scripts"));
    QVERIFY(match.score >= 100);
}

void VoiceCommandMatcherTest::detectsScreenIntent()
{
    QVERIFY(VoiceCommandMatcher::containsScreenIntent(QStringLiteral("小爱同学看看屏幕")));
    QVERIFY(!VoiceCommandMatcher::containsScreenIntent(QStringLiteral("小爱同学出来")));
}

void VoiceCommandMatcherTest::returnsEmptyForUnknownCommand()
{
    const VoiceCommandMatch match = VoiceCommandMatcher::match(QStringLiteral("今天天气怎么样"));
    QVERIFY(match.action.isEmpty());
}

QTEST_MAIN(VoiceCommandMatcherTest)

#include "test_voice_command_matcher.moc"
