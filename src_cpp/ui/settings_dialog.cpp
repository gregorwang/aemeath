#include "ui/settings_dialog.h"

#include <QEventLoop>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>
#include <QStringList>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "services/openai_compatible_client.h"

namespace {

struct EndpointPreset
{
    const char *label;
    const char *provider;
    const char *baseUrl;
    const char *model;
};

QString normalizeBaseUrl(QString value)
{
    value = value.trimmed();
    if (value.endsWith(QStringLiteral("/"))) {
        value.chop(1);
    }
    return value;
}

constexpr EndpointPreset kEndpointPresets[] = {
    { "自定义(不改)", "", "", "" },
    { "OpenAI 官方", "openai", "https://api.openai.com/v1", "gpt-5-mini" },
    { "xAI 官方", "xai", "https://api.x.ai", "grok-4.1" },
    { "DeepSeek 官方", "deepseek", "https://api.deepseek.com/v1", "deepseek-chat" },
    { "Kimi 官方", "kimi", "https://api.moonshot.cn/v1", "kimi-k2.5" },
    { "智谱官方", "zhipu", "https://open.bigmodel.cn/api/paas/v4", "glm-5" },
    { "豆包方舟", "doubao", "https://ark.cn-beijing.volces.com/api/v3", "ep-XXXXXXX" },
};

}

SettingsDialog::SettingsDialog(
    const AppConfig &config,
    const QVector<CharacterManifest> &availableCharacters,
    QWidget *parent)
    : QDialog(parent)
    , m_resultConfig(config)
{
    setWindowTitle(QStringLiteral("CyberCompanionCpp 设置"));
    setModal(true);
    resize(560, 420);
    buildUi();
    for (const CharacterManifest &manifest : availableCharacters) {
        if (!manifest.isValid()) {
            continue;
        }
        m_activeCharacterComboBox->addItem(
            manifest.name.isEmpty() ? manifest.id : manifest.name,
            manifest.id);
    }
    loadFromConfig(config);
}

AppConfig SettingsDialog::editedConfig() const
{
    return m_resultConfig;
}

void SettingsDialog::accept()
{
    m_resultConfig.debugMode = m_debugModeCheckBox->isChecked();
    m_resultConfig.offlineMode = m_offlineModeCheckBox->isChecked();
    m_resultConfig.startMinimized = m_startMinimizedCheckBox->isChecked();
    m_resultConfig.autoStartOnLogin = m_autoStartOnLoginCheckBox->isChecked();
    if (m_activeCharacterComboBox->count() > 0) {
        m_resultConfig.activeCharacterId = m_activeCharacterComboBox->currentData().toString().trimmed();
    }
    m_resultConfig.preferredPosition = m_preferredPositionComboBox->currentData().toString().trimmed();
    m_resultConfig.appearanceAsciiWidth = m_asciiWidthSpinBox->value();
    m_resultConfig.appearanceFontSizePx = m_fontSizeSpinBox->value();
    m_resultConfig.fullScreenPause = m_fullScreenPauseCheckBox->isChecked();
    m_resultConfig.residentMode = m_residentModeCheckBox->isChecked();
    m_resultConfig.idleThresholdSeconds = m_idleThresholdSpinBox->value();
    m_resultConfig.idleJitterMinSeconds = m_idleJitterMinSpinBox->value();
    m_resultConfig.idleJitterMaxSeconds = qMax(m_idleJitterMinSpinBox->value(), m_idleJitterMaxSpinBox->value());
    m_resultConfig.autoDismissSeconds = m_autoDismissSpinBox->value();
    m_resultConfig.cameraEnabled = m_cameraEnabledCheckBox->isChecked();
    m_resultConfig.cameraConsentGranted = m_cameraConsentGrantedCheckBox->isChecked();
    m_resultConfig.cameraIndex = m_cameraIndexSpinBox->value();
    m_resultConfig.cameraTargetFps = m_cameraTargetFpsSpinBox->value();
    m_resultConfig.eyeTrackingEnabled = m_eyeTrackingEnabledCheckBox->isChecked();
    m_resultConfig.periodicScanEnabled = m_periodicScanEnabledCheckBox->isChecked();
    m_resultConfig.periodicScanIntervalMinutes = m_periodicScanIntervalSpinBox->value();
    m_resultConfig.scriptedEntranceEnabled = m_scriptedEntranceEnabledCheckBox->isChecked();
    m_resultConfig.scriptedTrajectoryPath = m_scriptedTrajectoryPathEdit->text().trimmed();
    m_resultConfig.voiceScriptsPath = m_voiceScriptsPathEdit->text().trimmed();
    m_resultConfig.ttsProvider = m_ttsProviderComboBox->currentData().toString().trimmed();
    m_resultConfig.ttsVoice = m_ttsVoiceEdit->text().trimmed();
    m_resultConfig.ttsRate = m_ttsRateEdit->text().trimmed();
    m_resultConfig.audioVolume = m_audioVolumeSpinBox->value();
    m_resultConfig.audioCacheEnabled = m_audioCacheEnabledCheckBox->isChecked();
    m_resultConfig.microphoneEnabled = m_microphoneEnabledCheckBox->isChecked();
    m_resultConfig.voiceInputMode = m_voiceInputModeComboBox->currentData().toString().trimmed();
    m_resultConfig.wakeupEnabled = m_wakeupEnabledCheckBox->isChecked();
    m_resultConfig.wakeupLanguage = m_wakeupLanguageEdit->text().trimmed();
    m_resultConfig.asrProvider = m_asrProviderComboBox->currentData().toString().trimmed();
    m_resultConfig.asrApiKey = m_asrApiKeyEdit->text().trimmed();
    m_resultConfig.asrModel = m_asrModelEdit->text().trimmed();
    m_resultConfig.asrBaseUrl = normalizeBaseUrl(m_asrBaseUrlEdit->text());
    m_resultConfig.asrTemperature = m_asrTemperatureSpinBox->value();
    m_resultConfig.asrPrompt = m_asrPromptEdit->text().trimmed();
    QStringList wakeupPhrases;
    for (const QString &phrase : m_wakeupPhrasesEdit->text().split(',', Qt::SkipEmptyParts)) {
        const QString trimmed = phrase.trimmed();
        if (!trimmed.isEmpty()) {
            wakeupPhrases.push_back(trimmed);
        }
    }
    if (!wakeupPhrases.isEmpty()) {
        m_resultConfig.wakeupPhrases = wakeupPhrases;
    }
    m_resultConfig.idleInvasion.enabled = m_idleInvasionEnabledCheckBox->isChecked();
    m_resultConfig.idleInvasion.startDelayMs = m_idleInvasionStartDelaySpinBox->value() * 1000;
    m_resultConfig.idleInvasion.initialSpawnIntervalMs = m_idleInvasionInitialIntervalSpinBox->value() * 1000;
    m_resultConfig.idleInvasion.minSpawnIntervalMs = m_idleInvasionMinIntervalSpinBox->value() * 1000;
    m_resultConfig.idleInvasion.maxInvaders = m_idleInvasionMaxInvadersSpinBox->value();
    m_resultConfig.idleInvasion.scale = m_idleInvasionScaleSpinBox->value();
    m_resultConfig.idleInvasion.cellPadding = m_idleInvasionCellPaddingSpinBox->value();
    m_resultConfig.idleInvasion.retreatStyle = m_idleInvasionRetreatStyleComboBox->currentData().toString();
    const QStringList invasionGifs = m_idleInvasionGifsEdit->text().split(',', Qt::SkipEmptyParts);
    QStringList normalizedGifs;
    for (const QString &gif : invasionGifs) {
        const QString trimmed = gif.trimmed();
        if (!trimmed.isEmpty()) {
            normalizedGifs.push_back(trimmed);
        }
    }
    if (!normalizedGifs.isEmpty()) {
        m_resultConfig.idleInvasion.participatingGifs = normalizedGifs;
    }
    m_resultConfig.audioOutputReactive = m_audioOutputReactiveCheckBox->isChecked();
    m_resultConfig.audioOutputPollIntervalMs = m_audioOutputPollIntervalSpinBox->value();
    m_resultConfig.audioMonitorIgnoreCurrentProcessAudio = m_ignoreCurrentProcessCheckBox->isChecked();
    m_resultConfig.audioMonitorPreferMediaSessions = m_preferMediaSessionsCheckBox->isChecked();
    m_resultConfig.audioMonitorIncludeMasterPeakFallback = m_includeMasterPeakFallbackCheckBox->isChecked();
    m_resultConfig.llmProvider = m_providerComboBox->currentText().trimmed();
    m_resultConfig.llmModel = m_modelEdit->text().trimmed();
    m_resultConfig.llmBaseUrl = normalizeBaseUrl(m_baseUrlEdit->text());
    m_resultConfig.llmApiKey = m_apiKeyEdit->text().trimmed();
    m_resultConfig.commentarySystemPrompt = m_commentarySystemPromptEdit->text().trimmed();
    m_resultConfig.commentaryUserPrompt = m_commentaryUserPromptEdit->text().trimmed();
    m_resultConfig.commentaryNoImagePrompt = m_commentaryNoImagePromptEdit->text().trimmed();
    m_resultConfig.commentaryMaxTokens = m_commentaryMaxTokensSpinBox->value();
    m_resultConfig.commentaryTemperature = m_commentaryTemperatureSpinBox->value();
    m_resultConfig.commentaryStreamingEnabled = m_commentaryStreamingEnabledCheckBox->isChecked();
    m_resultConfig.commentaryOcrFallbackEnabled = m_commentaryOcrFallbackEnabledCheckBox->isChecked();
    m_resultConfig.commentaryStreamChunkChars = m_commentaryStreamChunkCharsSpinBox->value();
    m_resultConfig.commentaryMaxResponseChars = m_commentaryMaxResponseCharsSpinBox->value();
    m_resultConfig.commentaryPreambleText = m_commentaryPreambleTextEdit->text().trimmed();
    m_resultConfig.screenCommentaryAutoEnabled = m_screenCommentaryAutoEnabledCheckBox->isChecked();
    m_resultConfig.screenCommentaryAutoIntervalMinutes = m_screenCommentaryAutoIntervalSpinBox->value();

    QDialog::accept();
}

void SettingsDialog::buildUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    auto *generalTab = new QWidget(this);
    auto *generalLayout = new QFormLayout(generalTab);

    m_debugModeCheckBox = new QCheckBox(QStringLiteral("启用调试模式"), generalTab);
    m_debugModeCheckBox->setObjectName(QStringLiteral("debugModeCheckBox"));
    generalLayout->addRow(QString(), m_debugModeCheckBox);

    m_offlineModeCheckBox = new QCheckBox(QStringLiteral("离线模式（禁用远程 AI）"), generalTab);
    m_offlineModeCheckBox->setObjectName(QStringLiteral("offlineModeCheckBox"));
    generalLayout->addRow(QString(), m_offlineModeCheckBox);

    m_startMinimizedCheckBox = new QCheckBox(QStringLiteral("启动时最小化到托盘"), generalTab);
    m_startMinimizedCheckBox->setObjectName(QStringLiteral("startMinimizedCheckBox"));
    generalLayout->addRow(QString(), m_startMinimizedCheckBox);

    m_autoStartOnLoginCheckBox = new QCheckBox(QStringLiteral("开机自动启动"), generalTab);
    m_autoStartOnLoginCheckBox->setObjectName(QStringLiteral("autoStartOnLoginCheckBox"));
    generalLayout->addRow(QString(), m_autoStartOnLoginCheckBox);

    m_activeCharacterComboBox = new QComboBox(generalTab);
    m_activeCharacterComboBox->setObjectName(QStringLiteral("activeCharacterComboBox"));
    m_activeCharacterComboBox->setEnabled(false);
    generalLayout->addRow(QStringLiteral("活动角色"), m_activeCharacterComboBox);

    m_preferredPositionComboBox = new QComboBox(generalTab);
    m_preferredPositionComboBox->setObjectName(QStringLiteral("preferredPositionComboBox"));
    m_preferredPositionComboBox->addItem(QStringLiteral("自动（默认靠右）"), QStringLiteral("auto"));
    m_preferredPositionComboBox->addItem(QStringLiteral("左侧"), QStringLiteral("left"));
    m_preferredPositionComboBox->addItem(QStringLiteral("右侧"), QStringLiteral("right"));
    generalLayout->addRow(QStringLiteral("默认停靠位置"), m_preferredPositionComboBox);

    m_asciiWidthSpinBox = new QSpinBox(generalTab);
    m_asciiWidthSpinBox->setObjectName(QStringLiteral("asciiWidthSpinBox"));
    m_asciiWidthSpinBox->setRange(20, 200);
    generalLayout->addRow(QStringLiteral("外观宽度（字符列）"), m_asciiWidthSpinBox);

    m_fontSizeSpinBox = new QSpinBox(generalTab);
    m_fontSizeSpinBox->setObjectName(QStringLiteral("fontSizeSpinBox"));
    m_fontSizeSpinBox->setRange(6, 32);
    m_fontSizeSpinBox->setSuffix(QStringLiteral(" px"));
    generalLayout->addRow(QStringLiteral("外观字号"), m_fontSizeSpinBox);

    m_fullScreenPauseCheckBox = new QCheckBox(QStringLiteral("全屏应用时暂停"), generalTab);
    m_fullScreenPauseCheckBox->setObjectName(QStringLiteral("fullScreenPauseCheckBox"));
    generalLayout->addRow(QString(), m_fullScreenPauseCheckBox);

    m_residentModeCheckBox = new QCheckBox(QStringLiteral("角色常驻模式（全屏时自动隐藏）"), generalTab);
    m_residentModeCheckBox->setObjectName(QStringLiteral("residentModeCheckBox"));
    generalLayout->addRow(QString(), m_residentModeCheckBox);

    m_idleThresholdSpinBox = new QSpinBox(generalTab);
    m_idleThresholdSpinBox->setObjectName(QStringLiteral("idleThresholdSpinBox"));
    m_idleThresholdSpinBox->setRange(10, 3600);
    m_idleThresholdSpinBox->setSuffix(QStringLiteral(" 秒"));
    generalLayout->addRow(QStringLiteral("空闲触发阈值"), m_idleThresholdSpinBox);

    m_idleJitterMinSpinBox = new QSpinBox(generalTab);
    m_idleJitterMinSpinBox->setObjectName(QStringLiteral("idleJitterMinSpinBox"));
    m_idleJitterMinSpinBox->setRange(-600, 600);
    m_idleJitterMinSpinBox->setSuffix(QStringLiteral(" 秒"));
    generalLayout->addRow(QStringLiteral("空闲抖动最小值"), m_idleJitterMinSpinBox);

    m_idleJitterMaxSpinBox = new QSpinBox(generalTab);
    m_idleJitterMaxSpinBox->setObjectName(QStringLiteral("idleJitterMaxSpinBox"));
    m_idleJitterMaxSpinBox->setRange(-600, 600);
    m_idleJitterMaxSpinBox->setSuffix(QStringLiteral(" 秒"));
    generalLayout->addRow(QStringLiteral("空闲抖动最大值"), m_idleJitterMaxSpinBox);

    m_autoDismissSpinBox = new QSpinBox(generalTab);
    m_autoDismissSpinBox->setObjectName(QStringLiteral("autoDismissSpinBox"));
    m_autoDismissSpinBox->setRange(1, 300);
    m_autoDismissSpinBox->setSuffix(QStringLiteral(" 秒"));
    generalLayout->addRow(QStringLiteral("自动隐藏时长"), m_autoDismissSpinBox);

    m_cameraEnabledCheckBox = new QCheckBox(QStringLiteral("启用摄像头"), generalTab);
    m_cameraEnabledCheckBox->setObjectName(QStringLiteral("cameraEnabledCheckBox"));
    generalLayout->addRow(QString(), m_cameraEnabledCheckBox);

    m_cameraConsentGrantedCheckBox = new QCheckBox(QStringLiteral("允许程序访问摄像头"), generalTab);
    m_cameraConsentGrantedCheckBox->setObjectName(QStringLiteral("cameraConsentGrantedCheckBox"));
    generalLayout->addRow(QString(), m_cameraConsentGrantedCheckBox);

    m_cameraIndexSpinBox = new QSpinBox(generalTab);
    m_cameraIndexSpinBox->setObjectName(QStringLiteral("cameraIndexSpinBox"));
    m_cameraIndexSpinBox->setRange(0, 8);
    generalLayout->addRow(QStringLiteral("摄像头设备编号"), m_cameraIndexSpinBox);

    m_cameraTargetFpsSpinBox = new QSpinBox(generalTab);
    m_cameraTargetFpsSpinBox->setObjectName(QStringLiteral("cameraTargetFpsSpinBox"));
    m_cameraTargetFpsSpinBox->setRange(1, 30);
    m_cameraTargetFpsSpinBox->setSuffix(QStringLiteral(" FPS"));
    generalLayout->addRow(QStringLiteral("摄像头目标帧率"), m_cameraTargetFpsSpinBox);

    m_eyeTrackingEnabledCheckBox = new QCheckBox(QStringLiteral("启用视线跟踪"), generalTab);
    m_eyeTrackingEnabledCheckBox->setObjectName(QStringLiteral("eyeTrackingEnabledCheckBox"));
    generalLayout->addRow(QString(), m_eyeTrackingEnabledCheckBox);

    m_periodicScanEnabledCheckBox = new QCheckBox(QStringLiteral("启用周期性摄像头巡检"), generalTab);
    m_periodicScanEnabledCheckBox->setObjectName(QStringLiteral("periodicScanEnabledCheckBox"));
    generalLayout->addRow(QString(), m_periodicScanEnabledCheckBox);

    m_periodicScanIntervalSpinBox = new QSpinBox(generalTab);
    m_periodicScanIntervalSpinBox->setObjectName(QStringLiteral("periodicScanIntervalSpinBox"));
    m_periodicScanIntervalSpinBox->setRange(5, 240);
    m_periodicScanIntervalSpinBox->setSuffix(QStringLiteral(" 分钟"));
    generalLayout->addRow(QStringLiteral("巡检间隔"), m_periodicScanIntervalSpinBox);

    m_scriptedEntranceEnabledCheckBox = new QCheckBox(QStringLiteral("启用脚本式登场"), generalTab);
    m_scriptedEntranceEnabledCheckBox->setObjectName(QStringLiteral("scriptedEntranceEnabledCheckBox"));
    generalLayout->addRow(QString(), m_scriptedEntranceEnabledCheckBox);

    m_scriptedTrajectoryPathEdit = new QLineEdit(generalTab);
    m_scriptedTrajectoryPathEdit->setObjectName(QStringLiteral("scriptedTrajectoryPathEdit"));
    m_scriptedTrajectoryPathEdit->setPlaceholderText(QStringLiteral("留空则回退 recorded_paths/trajectory_1771029879_qt_animation.json"));
    generalLayout->addRow(QStringLiteral("脚本式轨迹路径"), m_scriptedTrajectoryPathEdit);

    m_voiceScriptsPathEdit = new QLineEdit(generalTab);
    m_voiceScriptsPathEdit->setObjectName(QStringLiteral("voiceScriptsPathEdit"));
    m_voiceScriptsPathEdit->setPlaceholderText(QStringLiteral("留空则使用 characters/default/scripts.json"));
    generalLayout->addRow(QStringLiteral("台词脚本路径"), m_voiceScriptsPathEdit);

    m_ttsProviderComboBox = new QComboBox(generalTab);
    m_ttsProviderComboBox->setObjectName(QStringLiteral("ttsProviderComboBox"));
    m_ttsProviderComboBox->addItem(QStringLiteral("edge（当前原生支持）"), QStringLiteral("edge"));
    generalLayout->addRow(QStringLiteral("TTS Provider"), m_ttsProviderComboBox);

    m_ttsVoiceEdit = new QLineEdit(generalTab);
    m_ttsVoiceEdit->setObjectName(QStringLiteral("ttsVoiceEdit"));
    generalLayout->addRow(QStringLiteral("TTS Voice"), m_ttsVoiceEdit);

    m_ttsRateEdit = new QLineEdit(generalTab);
    m_ttsRateEdit->setObjectName(QStringLiteral("ttsRateEdit"));
    generalLayout->addRow(QStringLiteral("TTS Rate"), m_ttsRateEdit);

    m_audioVolumeSpinBox = new QDoubleSpinBox(generalTab);
    m_audioVolumeSpinBox->setObjectName(QStringLiteral("audioVolumeSpinBox"));
    m_audioVolumeSpinBox->setRange(0.0, 1.0);
    m_audioVolumeSpinBox->setSingleStep(0.05);
    m_audioVolumeSpinBox->setDecimals(2);
    generalLayout->addRow(QStringLiteral("音量"), m_audioVolumeSpinBox);

    m_audioCacheEnabledCheckBox = new QCheckBox(QStringLiteral("启用 TTS 音频缓存"), generalTab);
    m_audioCacheEnabledCheckBox->setObjectName(QStringLiteral("audioCacheEnabledCheckBox"));
    generalLayout->addRow(QString(), m_audioCacheEnabledCheckBox);

    m_microphoneEnabledCheckBox = new QCheckBox(QStringLiteral("启用麦克风"), generalTab);
    m_microphoneEnabledCheckBox->setObjectName(QStringLiteral("microphoneEnabledCheckBox"));
    generalLayout->addRow(QString(), m_microphoneEnabledCheckBox);

    m_voiceInputModeComboBox = new QComboBox(generalTab);
    m_voiceInputModeComboBox->setObjectName(QStringLiteral("voiceInputModeComboBox"));
    m_voiceInputModeComboBox->addItem(QStringLiteral("push_to_talk（Ctrl+B 单次转写）"), QStringLiteral("push_to_talk"));
    m_voiceInputModeComboBox->addItem(QStringLiteral("continuous（连续唤醒）"), QStringLiteral("continuous"));
    generalLayout->addRow(QStringLiteral("语音输入模式"), m_voiceInputModeComboBox);

    m_wakeupEnabledCheckBox = new QCheckBox(QStringLiteral("启用语音唤醒词"), generalTab);
    m_wakeupEnabledCheckBox->setObjectName(QStringLiteral("wakeupEnabledCheckBox"));
    generalLayout->addRow(QString(), m_wakeupEnabledCheckBox);

    m_wakeupPhrasesEdit = new QLineEdit(generalTab);
    m_wakeupPhrasesEdit->setObjectName(QStringLiteral("wakeupPhrasesEdit"));
    m_wakeupPhrasesEdit->setPlaceholderText(QStringLiteral("多个唤醒词请用英文逗号分隔"));
    generalLayout->addRow(QStringLiteral("唤醒词"), m_wakeupPhrasesEdit);

    m_wakeupLanguageEdit = new QLineEdit(generalTab);
    m_wakeupLanguageEdit->setObjectName(QStringLiteral("wakeupLanguageEdit"));
    generalLayout->addRow(QStringLiteral("识别语言"), m_wakeupLanguageEdit);

    m_asrProviderComboBox = new QComboBox(generalTab);
    m_asrProviderComboBox->setObjectName(QStringLiteral("asrProviderComboBox"));
    m_asrProviderComboBox->addItem(QStringLiteral("zhipu_asr"), QStringLiteral("zhipu_asr"));
    m_asrProviderComboBox->addItem(QStringLiteral("openai_whisper"), QStringLiteral("openai_whisper"));
    m_asrProviderComboBox->addItem(QStringLiteral("xai_realtime"), QStringLiteral("xai_realtime"));
    generalLayout->addRow(QStringLiteral("ASR Provider"), m_asrProviderComboBox);

    m_asrApiKeyEdit = new QLineEdit(generalTab);
    m_asrApiKeyEdit->setObjectName(QStringLiteral("asrApiKeyEdit"));
    m_asrApiKeyEdit->setEchoMode(QLineEdit::Password);
    generalLayout->addRow(QStringLiteral("ASR API Key"), m_asrApiKeyEdit);

    m_asrModelEdit = new QLineEdit(generalTab);
    m_asrModelEdit->setObjectName(QStringLiteral("asrModelEdit"));
    generalLayout->addRow(QStringLiteral("ASR 模型"), m_asrModelEdit);

    m_asrBaseUrlEdit = new QLineEdit(generalTab);
    m_asrBaseUrlEdit->setObjectName(QStringLiteral("asrBaseUrlEdit"));
    generalLayout->addRow(QStringLiteral("ASR Base URL"), m_asrBaseUrlEdit);

    m_asrTemperatureSpinBox = new QDoubleSpinBox(generalTab);
    m_asrTemperatureSpinBox->setObjectName(QStringLiteral("asrTemperatureSpinBox"));
    m_asrTemperatureSpinBox->setRange(0.0, 1.0);
    m_asrTemperatureSpinBox->setDecimals(2);
    m_asrTemperatureSpinBox->setSingleStep(0.1);
    generalLayout->addRow(QStringLiteral("ASR 温度"), m_asrTemperatureSpinBox);

    m_asrPromptEdit = new QLineEdit(generalTab);
    m_asrPromptEdit->setObjectName(QStringLiteral("asrPromptEdit"));
    generalLayout->addRow(QStringLiteral("ASR Prompt"), m_asrPromptEdit);

    m_idleInvasionEnabledCheckBox = new QCheckBox(QStringLiteral("启用空闲入侵"), generalTab);
    m_idleInvasionEnabledCheckBox->setObjectName(QStringLiteral("idleInvasionEnabledCheckBox"));
    generalLayout->addRow(QString(), m_idleInvasionEnabledCheckBox);

    m_idleInvasionStartDelaySpinBox = new QSpinBox(generalTab);
    m_idleInvasionStartDelaySpinBox->setObjectName(QStringLiteral("idleInvasionStartDelaySpinBox"));
    m_idleInvasionStartDelaySpinBox->setRange(5, 3600);
    m_idleInvasionStartDelaySpinBox->setSuffix(QStringLiteral(" s"));
    generalLayout->addRow(QStringLiteral("入侵启动延迟"), m_idleInvasionStartDelaySpinBox);

    m_idleInvasionInitialIntervalSpinBox = new QSpinBox(generalTab);
    m_idleInvasionInitialIntervalSpinBox->setObjectName(QStringLiteral("idleInvasionInitialIntervalSpinBox"));
    m_idleInvasionInitialIntervalSpinBox->setRange(1, 3600);
    m_idleInvasionInitialIntervalSpinBox->setSuffix(QStringLiteral(" s"));
    generalLayout->addRow(QStringLiteral("初始生成间隔"), m_idleInvasionInitialIntervalSpinBox);

    m_idleInvasionMinIntervalSpinBox = new QSpinBox(generalTab);
    m_idleInvasionMinIntervalSpinBox->setObjectName(QStringLiteral("idleInvasionMinIntervalSpinBox"));
    m_idleInvasionMinIntervalSpinBox->setRange(1, 3600);
    m_idleInvasionMinIntervalSpinBox->setSuffix(QStringLiteral(" s"));
    generalLayout->addRow(QStringLiteral("最小生成间隔"), m_idleInvasionMinIntervalSpinBox);

    m_idleInvasionMaxInvadersSpinBox = new QSpinBox(generalTab);
    m_idleInvasionMaxInvadersSpinBox->setObjectName(QStringLiteral("idleInvasionMaxInvadersSpinBox"));
    m_idleInvasionMaxInvadersSpinBox->setRange(1, 200);
    generalLayout->addRow(QStringLiteral("最大入侵者数量"), m_idleInvasionMaxInvadersSpinBox);

    m_idleInvasionScaleSpinBox = new QDoubleSpinBox(generalTab);
    m_idleInvasionScaleSpinBox->setObjectName(QStringLiteral("idleInvasionScaleSpinBox"));
    m_idleInvasionScaleSpinBox->setRange(0.1, 3.0);
    m_idleInvasionScaleSpinBox->setSingleStep(0.05);
    m_idleInvasionScaleSpinBox->setDecimals(2);
    generalLayout->addRow(QStringLiteral("入侵者缩放"), m_idleInvasionScaleSpinBox);

    m_idleInvasionCellPaddingSpinBox = new QSpinBox(generalTab);
    m_idleInvasionCellPaddingSpinBox->setObjectName(QStringLiteral("idleInvasionCellPaddingSpinBox"));
    m_idleInvasionCellPaddingSpinBox->setRange(0, 200);
    generalLayout->addRow(QStringLiteral("网格间距"), m_idleInvasionCellPaddingSpinBox);

    m_idleInvasionGifsEdit = new QLineEdit(generalTab);
    m_idleInvasionGifsEdit->setObjectName(QStringLiteral("idleInvasionGifsEdit"));
    generalLayout->addRow(QStringLiteral("参与 GIF"), m_idleInvasionGifsEdit);

    m_idleInvasionRetreatStyleComboBox = new QComboBox(generalTab);
    m_idleInvasionRetreatStyleComboBox->setObjectName(QStringLiteral("idleInvasionRetreatStyleComboBox"));
    m_idleInvasionRetreatStyleComboBox->addItem(QStringLiteral("散开"), QStringLiteral("scatter"));
    m_idleInvasionRetreatStyleComboBox->addItem(QStringLiteral("波纹"), QStringLiteral("ripple"));
    m_idleInvasionRetreatStyleComboBox->addItem(QStringLiteral("瞬间"), QStringLiteral("instant"));
    generalLayout->addRow(QStringLiteral("撤退风格"), m_idleInvasionRetreatStyleComboBox);

    auto *audioHint = new QLabel(
        QStringLiteral("关闭音频输出反应后，原生 MEDIA_PLAYING 模式不会响应系统媒体音频。"),
        generalTab);
    audioHint->setWordWrap(true);
    generalLayout->addRow(audioHint);

    m_audioOutputReactiveCheckBox = new QCheckBox(QStringLiteral("启用系统音频输出反应"), generalTab);
    m_audioOutputReactiveCheckBox->setObjectName(QStringLiteral("audioOutputReactiveCheckBox"));
    generalLayout->addRow(QString(), m_audioOutputReactiveCheckBox);

    m_audioOutputPollIntervalSpinBox = new QSpinBox(generalTab);
    m_audioOutputPollIntervalSpinBox->setObjectName(QStringLiteral("audioOutputPollIntervalSpinBox"));
    m_audioOutputPollIntervalSpinBox->setRange(100, 5000);
    m_audioOutputPollIntervalSpinBox->setSingleStep(100);
    m_audioOutputPollIntervalSpinBox->setSuffix(QStringLiteral(" ms"));
    generalLayout->addRow(QStringLiteral("音频轮询间隔"), m_audioOutputPollIntervalSpinBox);

    m_ignoreCurrentProcessCheckBox = new QCheckBox(QStringLiteral("忽略本进程音频"), generalTab);
    m_ignoreCurrentProcessCheckBox->setObjectName(QStringLiteral("ignoreCurrentProcessCheckBox"));
    generalLayout->addRow(QString(), m_ignoreCurrentProcessCheckBox);

    m_preferMediaSessionsCheckBox = new QCheckBox(QStringLiteral("优先只识别媒体会话"), generalTab);
    m_preferMediaSessionsCheckBox->setObjectName(QStringLiteral("preferMediaSessionsCheckBox"));
    generalLayout->addRow(QString(), m_preferMediaSessionsCheckBox);

    m_includeMasterPeakFallbackCheckBox = new QCheckBox(QStringLiteral("启用主设备峰值回退"), generalTab);
    m_includeMasterPeakFallbackCheckBox->setObjectName(QStringLiteral("includeMasterPeakFallbackCheckBox"));
    generalLayout->addRow(QString(), m_includeMasterPeakFallbackCheckBox);

    auto *aiTab = new QWidget(this);
    auto *aiLayout = new QFormLayout(aiTab);

    m_endpointPresetComboBox = new QComboBox(aiTab);
    m_endpointPresetComboBox->setObjectName(QStringLiteral("endpointPresetComboBox"));
    for (const EndpointPreset &preset : kEndpointPresets) {
        m_endpointPresetComboBox->addItem(QString::fromUtf8(preset.label));
    }
    aiLayout->addRow(QStringLiteral("官方端点预设"), m_endpointPresetComboBox);

    m_providerComboBox = new QComboBox(aiTab);
    m_providerComboBox->setObjectName(QStringLiteral("providerComboBox"));
    m_providerComboBox->setEditable(true);
    m_providerComboBox->addItems(QStringList{
        QStringLiteral("openai"),
        QStringLiteral("xai"),
        QStringLiteral("deepseek"),
        QStringLiteral("kimi"),
        QStringLiteral("zhipu"),
        QStringLiteral("doubao"),
        QStringLiteral("none"),
    });
    aiLayout->addRow(QStringLiteral("LLM Provider"), m_providerComboBox);

    m_modelEdit = new QLineEdit(aiTab);
    m_modelEdit->setObjectName(QStringLiteral("modelEdit"));
    aiLayout->addRow(QStringLiteral("模型"), m_modelEdit);

    m_baseUrlEdit = new QLineEdit(aiTab);
    m_baseUrlEdit->setObjectName(QStringLiteral("baseUrlEdit"));
    aiLayout->addRow(QStringLiteral("Base URL"), m_baseUrlEdit);

    m_apiKeyEdit = new QLineEdit(aiTab);
    m_apiKeyEdit->setObjectName(QStringLiteral("apiKeyEdit"));
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    aiLayout->addRow(QStringLiteral("API Key"), m_apiKeyEdit);

    m_testApiButton = new QPushButton(QStringLiteral("测试 API 连接"), aiTab);
    m_testApiButton->setObjectName(QStringLiteral("testApiButton"));
    aiLayout->addRow(QString(), m_testApiButton);

    m_commentarySystemPromptEdit = new QLineEdit(aiTab);
    m_commentarySystemPromptEdit->setObjectName(QStringLiteral("commentarySystemPromptEdit"));
    aiLayout->addRow(QStringLiteral("评论 System Prompt"), m_commentarySystemPromptEdit);

    m_commentaryUserPromptEdit = new QLineEdit(aiTab);
    m_commentaryUserPromptEdit->setObjectName(QStringLiteral("commentaryUserPromptEdit"));
    aiLayout->addRow(QStringLiteral("评论 User Prompt"), m_commentaryUserPromptEdit);

    m_commentaryNoImagePromptEdit = new QLineEdit(aiTab);
    m_commentaryNoImagePromptEdit->setObjectName(QStringLiteral("commentaryNoImagePromptEdit"));
    aiLayout->addRow(QStringLiteral("无图回退 Prompt"), m_commentaryNoImagePromptEdit);

    m_commentaryMaxTokensSpinBox = new QSpinBox(aiTab);
    m_commentaryMaxTokensSpinBox->setObjectName(QStringLiteral("commentaryMaxTokensSpinBox"));
    m_commentaryMaxTokensSpinBox->setRange(1, 512);
    aiLayout->addRow(QStringLiteral("评论 Max Tokens"), m_commentaryMaxTokensSpinBox);

    m_commentaryTemperatureSpinBox = new QDoubleSpinBox(aiTab);
    m_commentaryTemperatureSpinBox->setObjectName(QStringLiteral("commentaryTemperatureSpinBox"));
    m_commentaryTemperatureSpinBox->setRange(0.0, 2.0);
    m_commentaryTemperatureSpinBox->setSingleStep(0.1);
    m_commentaryTemperatureSpinBox->setDecimals(2);
    aiLayout->addRow(QStringLiteral("评论 Temperature"), m_commentaryTemperatureSpinBox);

    m_commentaryStreamingEnabledCheckBox = new QCheckBox(QStringLiteral("启用评论分块播报"), aiTab);
    m_commentaryStreamingEnabledCheckBox->setObjectName(QStringLiteral("commentaryStreamingEnabledCheckBox"));
    aiLayout->addRow(QString(), m_commentaryStreamingEnabledCheckBox);

    m_commentaryOcrFallbackEnabledCheckBox = new QCheckBox(QStringLiteral("抓屏失败时附带前台窗口上下文"), aiTab);
    m_commentaryOcrFallbackEnabledCheckBox->setObjectName(QStringLiteral("commentaryOcrFallbackEnabledCheckBox"));
    aiLayout->addRow(QString(), m_commentaryOcrFallbackEnabledCheckBox);

    m_commentaryStreamChunkCharsSpinBox = new QSpinBox(aiTab);
    m_commentaryStreamChunkCharsSpinBox->setObjectName(QStringLiteral("commentaryStreamChunkCharsSpinBox"));
    m_commentaryStreamChunkCharsSpinBox->setRange(8, 80);
    aiLayout->addRow(QStringLiteral("评论分块字符数"), m_commentaryStreamChunkCharsSpinBox);

    m_commentaryMaxResponseCharsSpinBox = new QSpinBox(aiTab);
    m_commentaryMaxResponseCharsSpinBox->setObjectName(QStringLiteral("commentaryMaxResponseCharsSpinBox"));
    m_commentaryMaxResponseCharsSpinBox->setRange(20, 300);
    aiLayout->addRow(QStringLiteral("评论最大播报字符数"), m_commentaryMaxResponseCharsSpinBox);

    m_commentaryPreambleTextEdit = new QLineEdit(aiTab);
    m_commentaryPreambleTextEdit->setObjectName(QStringLiteral("commentaryPreambleTextEdit"));
    aiLayout->addRow(QStringLiteral("评论前导语"), m_commentaryPreambleTextEdit);

    m_screenCommentaryAutoEnabledCheckBox = new QCheckBox(QStringLiteral("启用自动屏幕评论"), aiTab);
    m_screenCommentaryAutoEnabledCheckBox->setObjectName(QStringLiteral("screenCommentaryAutoEnabledCheckBox"));
    aiLayout->addRow(QString(), m_screenCommentaryAutoEnabledCheckBox);

    m_screenCommentaryAutoIntervalSpinBox = new QSpinBox(aiTab);
    m_screenCommentaryAutoIntervalSpinBox->setObjectName(QStringLiteral("screenCommentaryAutoIntervalSpinBox"));
    m_screenCommentaryAutoIntervalSpinBox->setRange(1, 240);
    m_screenCommentaryAutoIntervalSpinBox->setSuffix(QStringLiteral(" 分钟"));
    aiLayout->addRow(QStringLiteral("自动评论间隔"), m_screenCommentaryAutoIntervalSpinBox);

    tabs->addTab(generalTab, QStringLiteral("基础"));
    tabs->addTab(aiTab, QStringLiteral("AI"));
    rootLayout->addWidget(tabs);

    connect(m_cameraEnabledCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_cameraConsentGrantedCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_periodicScanEnabledCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_microphoneEnabledCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_wakeupEnabledCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_offlineModeCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_voiceInputModeComboBox, &QComboBox::currentIndexChanged, this, [this](int) { syncControlState(); });
    connect(m_audioOutputReactiveCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_commentaryStreamingEnabledCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_screenCommentaryAutoEnabledCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_scriptedEntranceEnabledCheckBox, &QCheckBox::toggled, this, [this] { syncControlState(); });
    connect(m_testApiButton, &QPushButton::clicked, this, [this] { testApiConnection(); });
    connect(m_endpointPresetComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index <= 0) {
            return;
        }
        const EndpointPreset &preset = kEndpointPresets[index];
        m_providerComboBox->setCurrentText(QString::fromUtf8(preset.provider));
        m_modelEdit->setText(QString::fromUtf8(preset.model));
        m_baseUrlEdit->setText(QString::fromUtf8(preset.baseUrl));
    });

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    rootLayout->addWidget(m_buttonBox);
}

void SettingsDialog::loadFromConfig(const AppConfig &config)
{
    m_debugModeCheckBox->setChecked(config.debugMode);
    m_offlineModeCheckBox->setChecked(config.offlineMode);
    m_startMinimizedCheckBox->setChecked(config.startMinimized);
    m_autoStartOnLoginCheckBox->setChecked(config.autoStartOnLogin);
    if (m_activeCharacterComboBox->count() > 0) {
        m_activeCharacterComboBox->setEnabled(true);
        const int characterIndex = m_activeCharacterComboBox->findData(config.activeCharacterId);
        m_activeCharacterComboBox->setCurrentIndex(characterIndex >= 0 ? characterIndex : 0);
    }
    m_preferredPositionComboBox->setCurrentIndex(qMax(0, m_preferredPositionComboBox->findData(config.preferredPosition)));
    m_asciiWidthSpinBox->setValue(qBound(20, config.appearanceAsciiWidth, 200));
    m_fontSizeSpinBox->setValue(qBound(6, config.appearanceFontSizePx, 32));
    m_fullScreenPauseCheckBox->setChecked(config.fullScreenPause);
    m_residentModeCheckBox->setChecked(config.residentMode);
    m_idleThresholdSpinBox->setValue(qBound(10, config.idleThresholdSeconds, 3600));
    m_idleJitterMinSpinBox->setValue(qBound(-600, config.idleJitterMinSeconds, 600));
    m_idleJitterMaxSpinBox->setValue(qBound(-600, config.idleJitterMaxSeconds, 600));
    m_autoDismissSpinBox->setValue(qBound(1, config.autoDismissSeconds, 300));
    m_cameraEnabledCheckBox->setChecked(config.cameraEnabled);
    m_cameraConsentGrantedCheckBox->setChecked(config.cameraConsentGranted);
    m_cameraIndexSpinBox->setValue(qMax(0, config.cameraIndex));
    m_cameraTargetFpsSpinBox->setValue(qBound(1, config.cameraTargetFps, 30));
    m_eyeTrackingEnabledCheckBox->setChecked(config.eyeTrackingEnabled);
    m_periodicScanEnabledCheckBox->setChecked(config.periodicScanEnabled);
    m_periodicScanIntervalSpinBox->setValue(qBound(5, config.periodicScanIntervalMinutes, 240));
    m_scriptedEntranceEnabledCheckBox->setChecked(config.scriptedEntranceEnabled);
    m_scriptedTrajectoryPathEdit->setText(config.scriptedTrajectoryPath);
    m_voiceScriptsPathEdit->setText(config.voiceScriptsPath);
    m_ttsProviderComboBox->setCurrentIndex(qMax(0, m_ttsProviderComboBox->findData(config.ttsProvider)));
    m_ttsVoiceEdit->setText(config.ttsVoice);
    m_ttsRateEdit->setText(config.ttsRate);
    m_audioVolumeSpinBox->setValue(qBound(0.0, config.audioVolume, 1.0));
    m_audioCacheEnabledCheckBox->setChecked(config.audioCacheEnabled);
    m_microphoneEnabledCheckBox->setChecked(config.microphoneEnabled);
    m_voiceInputModeComboBox->setCurrentIndex(qMax(0, m_voiceInputModeComboBox->findData(config.voiceInputMode)));
    m_wakeupEnabledCheckBox->setChecked(config.wakeupEnabled);
    m_wakeupPhrasesEdit->setText(config.wakeupPhrases.join(QStringLiteral(", ")));
    m_wakeupLanguageEdit->setText(config.wakeupLanguage);
    m_asrProviderComboBox->setCurrentIndex(qMax(0, m_asrProviderComboBox->findData(config.asrProvider)));
    m_asrApiKeyEdit->setText(config.asrApiKey);
    m_asrModelEdit->setText(config.asrModel);
    m_asrBaseUrlEdit->setText(config.asrBaseUrl);
    m_asrTemperatureSpinBox->setValue(config.asrTemperature);
    m_asrPromptEdit->setText(config.asrPrompt);
    m_idleInvasionEnabledCheckBox->setChecked(config.idleInvasion.enabled);
    m_idleInvasionStartDelaySpinBox->setValue(qMax(5, config.idleInvasion.startDelayMs / 1000));
    m_idleInvasionInitialIntervalSpinBox->setValue(qMax(1, config.idleInvasion.initialSpawnIntervalMs / 1000));
    m_idleInvasionMinIntervalSpinBox->setValue(qMax(1, config.idleInvasion.minSpawnIntervalMs / 1000));
    m_idleInvasionMaxInvadersSpinBox->setValue(config.idleInvasion.maxInvaders);
    m_idleInvasionScaleSpinBox->setValue(config.idleInvasion.scale);
    m_idleInvasionCellPaddingSpinBox->setValue(config.idleInvasion.cellPadding);
    m_idleInvasionGifsEdit->setText(config.idleInvasion.participatingGifs.join(QStringLiteral(", ")));
    const int retreatStyleIndex = qMax(0, m_idleInvasionRetreatStyleComboBox->findData(config.idleInvasion.retreatStyle));
    m_idleInvasionRetreatStyleComboBox->setCurrentIndex(retreatStyleIndex);
    m_audioOutputReactiveCheckBox->setChecked(config.audioOutputReactive);
    m_audioOutputPollIntervalSpinBox->setValue(config.audioOutputPollIntervalMs);
    m_ignoreCurrentProcessCheckBox->setChecked(config.audioMonitorIgnoreCurrentProcessAudio);
    m_preferMediaSessionsCheckBox->setChecked(config.audioMonitorPreferMediaSessions);
    m_includeMasterPeakFallbackCheckBox->setChecked(config.audioMonitorIncludeMasterPeakFallback);
    m_endpointPresetComboBox->setCurrentIndex(0);
    m_providerComboBox->setCurrentText(config.llmProvider);
    m_modelEdit->setText(config.llmModel);
    m_baseUrlEdit->setText(config.llmBaseUrl);
    m_apiKeyEdit->setText(config.llmApiKey);
    m_commentarySystemPromptEdit->setText(config.commentarySystemPrompt);
    m_commentaryUserPromptEdit->setText(config.commentaryUserPrompt);
    m_commentaryNoImagePromptEdit->setText(config.commentaryNoImagePrompt);
    m_commentaryMaxTokensSpinBox->setValue(config.commentaryMaxTokens);
    m_commentaryTemperatureSpinBox->setValue(config.commentaryTemperature);
    m_commentaryStreamingEnabledCheckBox->setChecked(config.commentaryStreamingEnabled);
    m_commentaryOcrFallbackEnabledCheckBox->setChecked(config.commentaryOcrFallbackEnabled);
    m_commentaryStreamChunkCharsSpinBox->setValue(qBound(8, config.commentaryStreamChunkChars, 80));
    m_commentaryMaxResponseCharsSpinBox->setValue(qBound(20, config.commentaryMaxResponseChars, 300));
    m_commentaryPreambleTextEdit->setText(config.commentaryPreambleText);
    m_screenCommentaryAutoEnabledCheckBox->setChecked(config.screenCommentaryAutoEnabled);
    m_screenCommentaryAutoIntervalSpinBox->setValue(qBound(1, config.screenCommentaryAutoIntervalMinutes, 240));
    syncControlState();
}

void SettingsDialog::syncControlState()
{
    const bool offlineMode = m_offlineModeCheckBox->isChecked();
    const bool cameraConfigured = m_cameraEnabledCheckBox->isChecked() && m_cameraConsentGrantedCheckBox->isChecked();
    m_cameraIndexSpinBox->setEnabled(cameraConfigured);
    m_cameraTargetFpsSpinBox->setEnabled(cameraConfigured);
    m_eyeTrackingEnabledCheckBox->setEnabled(cameraConfigured);
    m_periodicScanEnabledCheckBox->setEnabled(cameraConfigured);
    m_periodicScanIntervalSpinBox->setEnabled(cameraConfigured && m_periodicScanEnabledCheckBox->isChecked());

    const bool scriptedEnabled = m_scriptedEntranceEnabledCheckBox->isChecked();
    m_scriptedTrajectoryPathEdit->setEnabled(scriptedEnabled);

    const bool microphoneEnabled = m_microphoneEnabledCheckBox->isChecked();
    const bool remoteVoiceEnabled = microphoneEnabled && !offlineMode;
    m_voiceInputModeComboBox->setEnabled(remoteVoiceEnabled);
    m_asrProviderComboBox->setEnabled(remoteVoiceEnabled);
    m_asrApiKeyEdit->setEnabled(remoteVoiceEnabled);
    m_asrModelEdit->setEnabled(remoteVoiceEnabled);
    m_asrBaseUrlEdit->setEnabled(remoteVoiceEnabled);
    m_asrTemperatureSpinBox->setEnabled(remoteVoiceEnabled);
    m_asrPromptEdit->setEnabled(remoteVoiceEnabled);
    const bool wakeupControlsEnabled =
        remoteVoiceEnabled
        && m_voiceInputModeComboBox->currentData().toString() == QStringLiteral("continuous");
    m_wakeupEnabledCheckBox->setEnabled(wakeupControlsEnabled);
    m_wakeupPhrasesEdit->setEnabled(wakeupControlsEnabled && m_wakeupEnabledCheckBox->isChecked());
    m_wakeupLanguageEdit->setEnabled(wakeupControlsEnabled && m_wakeupEnabledCheckBox->isChecked());

    const bool audioReactive = m_audioOutputReactiveCheckBox->isChecked();
    m_audioOutputPollIntervalSpinBox->setEnabled(audioReactive);
    m_ignoreCurrentProcessCheckBox->setEnabled(audioReactive);
    m_preferMediaSessionsCheckBox->setEnabled(audioReactive);
    m_includeMasterPeakFallbackCheckBox->setEnabled(audioReactive);

    m_providerComboBox->setEnabled(!offlineMode);
    m_modelEdit->setEnabled(!offlineMode);
    m_baseUrlEdit->setEnabled(!offlineMode);
    m_apiKeyEdit->setEnabled(!offlineMode);
    m_testApiButton->setEnabled(!offlineMode);
    m_commentarySystemPromptEdit->setEnabled(!offlineMode);
    m_commentaryUserPromptEdit->setEnabled(!offlineMode);
    m_commentaryNoImagePromptEdit->setEnabled(!offlineMode);
    m_commentaryMaxTokensSpinBox->setEnabled(!offlineMode);
    m_commentaryTemperatureSpinBox->setEnabled(!offlineMode);
    m_commentaryStreamingEnabledCheckBox->setEnabled(!offlineMode);
    m_commentaryOcrFallbackEnabledCheckBox->setEnabled(!offlineMode);
    m_commentaryStreamChunkCharsSpinBox->setEnabled(!offlineMode && m_commentaryStreamingEnabledCheckBox->isChecked());
    m_commentaryMaxResponseCharsSpinBox->setEnabled(!offlineMode);
    m_commentaryPreambleTextEdit->setEnabled(!offlineMode);
    m_screenCommentaryAutoEnabledCheckBox->setEnabled(!offlineMode);
    m_screenCommentaryAutoIntervalSpinBox->setEnabled(!offlineMode && m_screenCommentaryAutoEnabledCheckBox->isChecked());
}

void SettingsDialog::testApiConnection()
{
    if (m_offlineModeCheckBox->isChecked()) {
        QMessageBox::information(this, QStringLiteral("测试 API 连接"), QStringLiteral("当前处于离线模式，已跳过远程连通性测试。"));
        return;
    }

    const QString provider = m_providerComboBox->currentText().trimmed();
    const QString model = m_modelEdit->text().trimmed();
    const QString baseUrl = OpenAiCompatibleClient::normalizeBaseUrl(m_baseUrlEdit->text());
    const QString apiKey = OpenAiCompatibleClient::resolveApiKey(provider, m_apiKeyEdit->text());
    if (provider.isEmpty() || model.isEmpty() || baseUrl.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("测试 API 连接"), QStringLiteral("请先填写 provider、model 和 base URL。"));
        return;
    }
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("测试 API 连接"), QStringLiteral("当前没有可用的 API Key。"));
        return;
    }

    const QUrl url(baseUrl + QStringLiteral("/chat/completions"));
    if (!url.isValid()) {
        QMessageBox::warning(this, QStringLiteral("测试 API 连接"), QStringLiteral("Base URL 无效。"));
        return;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(8000);

    QJsonObject message;
    message.insert(QStringLiteral("role"), QStringLiteral("user"));
    message.insert(QStringLiteral("content"), QStringLiteral("请只回复 ok"));

    QJsonArray messages;
    messages.append(message);

    QJsonObject payload;
    payload.insert(QStringLiteral("model"), model);
    payload.insert(QStringLiteral("messages"), messages);
    payload.insert(QStringLiteral("max_tokens"), 8);
    payload.insert(QStringLiteral("temperature"), 0.0);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QNetworkReply *reply = manager.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(8000);
    loop.exec();

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else if (reply->isRunning()) {
        reply->abort();
    }

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorString = reply->errorString();
    reply->deleteLater();

    if (error != QNetworkReply::NoError) {
        QMessageBox::warning(
            this,
            QStringLiteral("测试 API 连接"),
            OpenAiCompatibleClient::buildFailureMessage(statusCode, errorString, responseBody));
        return;
    }

    const QString responseText = OpenAiCompatibleClient::extractResponseText(responseBody).trimmed();
    QMessageBox::information(
        this,
        QStringLiteral("测试 API 连接"),
        responseText.isEmpty()
            ? QStringLiteral("连接成功，但响应中没有可提取的文本。")
            : QStringLiteral("连接成功：%1").arg(responseText.left(80)));
}
