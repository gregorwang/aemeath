#include "runtime/voice_command_matcher.h"

#include <QHash>
#include <QStringList>

namespace {

const QHash<QString, QStringList> &commandPhrases()
{
    static const QHash<QString, QStringList> phrases = {
        { QStringLiteral("summon"), {
            QStringLiteral("出来"),
            QStringLiteral("出来吧"),
            QStringLiteral("召唤"),
            QStringLiteral("现身"),
            QStringLiteral("过来"),
            QStringLiteral("出来一下"),
            QStringLiteral("出现"),
        } },
        { QStringLiteral("screen_commentary"), {
            QStringLiteral("看屏幕"),
            QStringLiteral("看看屏幕"),
            QStringLiteral("你在看什么"),
            QStringLiteral("屏幕上是什么"),
            QStringLiteral("解读屏幕"),
            QStringLiteral("看一下屏幕"),
        } },
        { QStringLiteral("hide"), {
            QStringLiteral("隐藏"),
            QStringLiteral("躲起来"),
            QStringLiteral("退下"),
            QStringLiteral("回去"),
            QStringLiteral("消失"),
        } },
        { QStringLiteral("toggle_visibility"), {
            QStringLiteral("显示隐藏"),
            QStringLiteral("切换显示"),
            QStringLiteral("切换可见"),
            QStringLiteral("切换状态"),
        } },
        { QStringLiteral("status"), {
            QStringLiteral("状态"),
            QStringLiteral("报告状态"),
            QStringLiteral("查看状态"),
        } },
        { QStringLiteral("open_settings"), {
            QStringLiteral("打开设置"),
            QStringLiteral("进入设置"),
            QStringLiteral("显示设置"),
        } },
        { QStringLiteral("open_guide"), {
            QStringLiteral("打开使用指南"),
            QStringLiteral("查看使用指南"),
            QStringLiteral("打开指南"),
        } },
        { QStringLiteral("edit_scripts"), {
            QStringLiteral("编辑台词"),
            QStringLiteral("打开台词"),
            QStringLiteral("修改台词"),
        } },
        { QStringLiteral("reload_characters"), {
            QStringLiteral("重载角色"),
            QStringLiteral("刷新角色目录"),
            QStringLiteral("重新扫描角色"),
        } },
        { QStringLiteral("reload_scripts"), {
            QStringLiteral("重载台词"),
            QStringLiteral("刷新台词"),
            QStringLiteral("重新加载台词"),
        } },
        { QStringLiteral("copy_recent_logs"), {
            QStringLiteral("复制最近日志"),
            QStringLiteral("复制日志"),
            QStringLiteral("复制错误日志"),
        } },
        { QStringLiteral("check_updates"), {
            QStringLiteral("检查更新"),
            QStringLiteral("查看更新"),
            QStringLiteral("看看有没有更新"),
        } },
        { QStringLiteral("open_config"), {
            QStringLiteral("打开配置"),
            QStringLiteral("打开配置文件"),
            QStringLiteral("查看配置"),
        } },
        { QStringLiteral("open_data_dir"), {
            QStringLiteral("打开数据目录"),
            QStringLiteral("打开数据文件夹"),
            QStringLiteral("查看数据目录"),
        } },
        { QStringLiteral("open_logs"), {
            QStringLiteral("打开日志"),
            QStringLiteral("打开日志目录"),
            QStringLiteral("查看日志"),
        } },
        { QStringLiteral("feedback"), {
            QStringLiteral("反馈问题"),
            QStringLiteral("提交反馈"),
            QStringLiteral("报告问题"),
        } },
        { QStringLiteral("about"), {
            QStringLiteral("关于"),
            QStringLiteral("打开关于"),
            QStringLiteral("查看关于"),
        } },
        { QStringLiteral("dnd_on"), {
            QStringLiteral("开启请勿打扰"),
            QStringLiteral("进入请勿打扰"),
            QStringLiteral("不要打扰"),
        } },
        { QStringLiteral("dnd_off"), {
            QStringLiteral("关闭请勿打扰"),
            QStringLiteral("退出请勿打扰"),
            QStringLiteral("恢复自动模式"),
        } },
        { QStringLiteral("offline_mode_on"), {
            QStringLiteral("开启离线模式"),
            QStringLiteral("进入离线模式"),
            QStringLiteral("切到离线模式"),
        } },
        { QStringLiteral("offline_mode_off"), {
            QStringLiteral("关闭离线模式"),
            QStringLiteral("退出离线模式"),
            QStringLiteral("恢复联网"),
        } },
        { QStringLiteral("resident_mode_on"), {
            QStringLiteral("开启常驻模式"),
            QStringLiteral("进入常驻模式"),
        } },
        { QStringLiteral("resident_mode_off"), {
            QStringLiteral("关闭常驻模式"),
            QStringLiteral("退出常驻模式"),
        } },
        { QStringLiteral("camera_on"), {
            QStringLiteral("开启摄像头"),
            QStringLiteral("打开摄像头"),
        } },
        { QStringLiteral("camera_off"), {
            QStringLiteral("关闭摄像头"),
            QStringLiteral("停用摄像头"),
        } },
        { QStringLiteral("microphone_on"), {
            QStringLiteral("开启麦克风"),
            QStringLiteral("打开麦克风"),
        } },
        { QStringLiteral("microphone_off"), {
            QStringLiteral("关闭麦克风"),
            QStringLiteral("停用麦克风"),
        } },
        { QStringLiteral("wakeup_on"), {
            QStringLiteral("开启语音唤醒"),
            QStringLiteral("打开语音唤醒"),
            QStringLiteral("开启唤醒词"),
        } },
        { QStringLiteral("wakeup_off"), {
            QStringLiteral("关闭语音唤醒"),
            QStringLiteral("关闭唤醒词"),
            QStringLiteral("停用语音唤醒"),
        } },
        { QStringLiteral("eye_tracking_on"), {
            QStringLiteral("开启视线跟踪"),
            QStringLiteral("打开视线跟踪"),
            QStringLiteral("开启眼动跟踪"),
        } },
        { QStringLiteral("eye_tracking_off"), {
            QStringLiteral("关闭视线跟踪"),
            QStringLiteral("停用视线跟踪"),
            QStringLiteral("关闭眼动跟踪"),
        } },
        { QStringLiteral("periodic_scan_on"), {
            QStringLiteral("开启周期巡检"),
            QStringLiteral("开启摄像头巡检"),
        } },
        { QStringLiteral("periodic_scan_off"), {
            QStringLiteral("关闭周期巡检"),
            QStringLiteral("关闭摄像头巡检"),
        } },
        { QStringLiteral("audio_reactive_on"), {
            QStringLiteral("开启音频反应"),
            QStringLiteral("开启音频模式"),
        } },
        { QStringLiteral("audio_reactive_off"), {
            QStringLiteral("关闭音频反应"),
            QStringLiteral("关闭音频模式"),
        } },
        { QStringLiteral("voice_mode_continuous"), {
            QStringLiteral("切到连续唤醒"),
            QStringLiteral("开启连续唤醒模式"),
            QStringLiteral("切换到连续唤醒"),
        } },
        { QStringLiteral("voice_mode_push_to_talk"), {
            QStringLiteral("切到按键转写"),
            QStringLiteral("切换到按键转写"),
            QStringLiteral("开启按键转写模式"),
        } },
        { QStringLiteral("scripted_entrance_on"), {
            QStringLiteral("开启脚本式登场"),
            QStringLiteral("打开脚本式登场"),
        } },
        { QStringLiteral("scripted_entrance_off"), {
            QStringLiteral("关闭脚本式登场"),
            QStringLiteral("停用脚本式登场"),
        } },
        { QStringLiteral("fullscreen_pause_on"), {
            QStringLiteral("开启全屏暂停"),
            QStringLiteral("打开全屏暂停"),
        } },
        { QStringLiteral("fullscreen_pause_off"), {
            QStringLiteral("关闭全屏暂停"),
            QStringLiteral("停用全屏暂停"),
        } },
        { QStringLiteral("idle_invasion_on"), {
            QStringLiteral("开启空闲入侵"),
            QStringLiteral("打开空闲入侵"),
        } },
        { QStringLiteral("idle_invasion_off"), {
            QStringLiteral("关闭空闲入侵"),
            QStringLiteral("停用空闲入侵"),
        } },
        { QStringLiteral("auto_commentary_on"), {
            QStringLiteral("开启自动评论"),
            QStringLiteral("打开自动评论"),
        } },
        { QStringLiteral("auto_commentary_off"), {
            QStringLiteral("关闭自动评论"),
            QStringLiteral("停用自动评论"),
        } },
        { QStringLiteral("peek"), {
            QStringLiteral("探头"),
            QStringLiteral("出来看一眼"),
            QStringLiteral("偷偷看一眼"),
        } },
        { QStringLiteral("flee"), {
            QStringLiteral("撤退"),
            QStringLiteral("快跑"),
            QStringLiteral("赶紧躲起来"),
        } },
        { QStringLiteral("scripted_trajectory_debug"), {
            QStringLiteral("轨迹登场"),
            QStringLiteral("调试轨迹登场"),
            QStringLiteral("播放登场轨迹"),
        } },
        { QStringLiteral("camera_scan_debug"), {
            QStringLiteral("调试摄像头巡检"),
            QStringLiteral("摄像头巡检"),
            QStringLiteral("看我一眼"),
        } },
        { QStringLiteral("invasion_debug"), {
            QStringLiteral("空闲入侵调试"),
            QStringLiteral("调试空闲入侵"),
            QStringLiteral("开始空闲入侵"),
        } },
        { QStringLiteral("sad_comfort_debug"), {
            QStringLiteral("调试悲伤安慰"),
            QStringLiteral("测试悲伤安慰"),
            QStringLiteral("触发悲伤安慰"),
        } },
        { QStringLiteral("no_face_debug"), {
            QStringLiteral("调试无人脸提醒"),
            QStringLiteral("测试无人脸提醒"),
            QStringLiteral("触发无人脸提醒"),
            QStringLiteral("调试无人脸"),
        } },
    };
    return phrases;
}

int levenshteinDistance(const QString &left, const QString &right)
{
    const int n = left.size();
    const int m = right.size();
    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }

    QVector<int> prev(m + 1);
    QVector<int> curr(m + 1);
    for (int j = 0; j <= m; ++j) {
        prev[j] = j;
    }

    for (int i = 1; i <= n; ++i) {
        curr[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int cost = left.at(i - 1) == right.at(j - 1) ? 0 : 1;
            curr[j] = qMin(qMin(
                curr[j - 1] + 1,
                prev[j] + 1),
                prev[j - 1] + cost);
        }
        prev = curr;
    }
    return prev[m];
}

}

QString VoiceCommandMatcher::normalizeText(const QString &text)
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

VoiceCommandMatch VoiceCommandMatcher::match(const QString &transcript, int minScore)
{
    const QString normalizedTranscript = normalizeText(transcript);
    if (normalizedTranscript.isEmpty()) {
        return {};
    }

    VoiceCommandMatch bestContainsMatch;
    int bestContainsPhraseLength = -1;
    for (auto it = commandPhrases().cbegin(); it != commandPhrases().cend(); ++it) {
        for (const QString &phrase : it.value()) {
            const QString normalizedPhrase = normalizeText(phrase);
            if (!normalizedPhrase.isEmpty() && normalizedTranscript.contains(normalizedPhrase)) {
                if (normalizedPhrase.size() > bestContainsPhraseLength) {
                    bestContainsPhraseLength = normalizedPhrase.size();
                    bestContainsMatch = { it.key(), 100, phrase, transcript };
                }
            }
        }
    }
    if (bestContainsPhraseLength >= 0) {
        return bestContainsMatch;
    }

    VoiceCommandMatch bestMatch;
    for (auto it = commandPhrases().cbegin(); it != commandPhrases().cend(); ++it) {
        for (const QString &phrase : it.value()) {
            const int score = similarityScore(normalizedTranscript, normalizeText(phrase));
            if (score > bestMatch.score) {
                bestMatch = { it.key(), score, phrase, transcript };
            }
        }
    }

    if (bestMatch.score < minScore) {
        return {};
    }
    return bestMatch;
}

bool VoiceCommandMatcher::containsScreenIntent(const QString &transcript)
{
    const QString normalizedTranscript = normalizeText(transcript);
    if (normalizedTranscript.isEmpty()) {
        return false;
    }
    const QStringList phrases = commandPhrases().value(QStringLiteral("screen_commentary"));
    for (const QString &phrase : phrases) {
        const QString normalizedPhrase = normalizeText(phrase);
        if (!normalizedPhrase.isEmpty() && normalizedTranscript.contains(normalizedPhrase)) {
            return true;
        }
    }
    return false;
}

int VoiceCommandMatcher::similarityScore(const QString &left, const QString &right)
{
    if (left.isEmpty() || right.isEmpty()) {
        return -1;
    }
    const int distance = levenshteinDistance(left, right);
    const int maxLength = qMax(left.size(), right.size());
    return qMax(0, 100 - static_cast<int>((100.0 * distance) / qMax(1, maxLength)));
}
