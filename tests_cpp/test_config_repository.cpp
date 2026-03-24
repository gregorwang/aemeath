#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "runtime/config_repository.h"

class ConfigRepositoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void saveAndLoadRoundTrip();
    void bootstrapFromLegacyConfig();
    void migratesLegacyGoogleAsrProvider();
    void migratesLegacyNonEdgeTtsProvider();
    void saveFailureReturnsErrorMessage();
};

void ConfigRepositoryTest::saveAndLoadRoundTrip()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    ConfigRepository repository(configPath);

    AppConfig config;
    config.version = QStringLiteral("9.9.9");
    config.debugMode = false;
    config.firstRun = false;
    config.startMinimized = true;
    config.autoStartOnLogin = true;
    config.activeCharacterId = QStringLiteral("default");
    config.preferredPosition = QStringLiteral("left");
    config.appearanceAsciiWidth = 72;
    config.appearanceFontSizePx = 10;
    config.lastVisible = false;
    config.offlineMode = true;
    config.fullScreenPause = false;
    config.residentMode = true;
    config.idleThresholdSeconds = 240;
    config.idleJitterMinSeconds = -15;
    config.idleJitterMaxSeconds = 45;
    config.autoDismissSeconds = 18;
    config.cameraEnabled = true;
    config.cameraConsentGranted = true;
    config.cameraIndex = 2;
    config.cameraTargetFps = 12;
    config.eyeTrackingEnabled = false;
    config.periodicScanEnabled = false;
    config.periodicScanIntervalMinutes = 45;
    config.scriptedEntranceEnabled = true;
    config.scriptedTrajectoryPath = QStringLiteral("recorded_paths");
    config.audioOutputReactive = false;
    config.audioOutputPollIntervalMs = 750;
    config.audioMonitorIgnoreCurrentProcessAudio = false;
    config.audioMonitorPreferMediaSessions = false;
    config.audioMonitorIncludeMasterPeakFallback = true;
    config.voiceScriptsPath = QStringLiteral("characters/default/scripts.json");
    config.ttsProvider = QStringLiteral("edge");
    config.ttsVoice = QStringLiteral("zh-CN-YunxiNeural");
    config.ttsRate = QStringLiteral("+15%");
    config.audioVolume = 0.55;
    config.audioCacheEnabled = false;
    config.microphoneEnabled = true;
    config.voiceInputMode = QStringLiteral("push_to_talk");
    config.asrProvider = QStringLiteral("zhipu_asr");
    config.asrApiKey = QStringLiteral("asr-key");
    config.asrModel = QStringLiteral("glm-asr-2512");
    config.asrBaseUrl = QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions");
    config.asrTemperature = 0.2;
    config.asrPrompt = QStringLiteral("voice context");
    config.wakeupEnabled = true;
    config.wakeupPhrases = { QStringLiteral("小爱同学"), QStringLiteral("看看屏幕") };
    config.wakeupLanguage = QStringLiteral("zh-CN");
    config.idleInvasion.enabled = false;
    config.idleInvasion.startDelayMs = 120000;
    config.idleInvasion.initialSpawnIntervalMs = 9000;
    config.idleInvasion.minSpawnIntervalMs = 1500;
    config.idleInvasion.maxInvaders = 33;
    config.idleInvasion.scale = 0.65;
    config.idleInvasion.cellPadding = 12;
    config.idleInvasion.participatingGifs = { QStringLiteral("state1.gif"), QStringLiteral("state5.gif") };
    config.idleInvasion.retreatStyle = QStringLiteral("ripple");
    config.windowX = 120;
    config.windowY = 280;
    config.llmProvider = QStringLiteral("xai");
    config.llmModel = QStringLiteral("grok-4-latest");
    config.llmApiKey = QStringLiteral("test-key");
    config.llmBaseUrl = QStringLiteral("https://api.x.ai");
    config.commentarySystemPrompt = QStringLiteral("system prompt");
    config.commentaryUserPrompt = QStringLiteral("user prompt");
    config.commentaryNoImagePrompt = QStringLiteral("no image prompt");
    config.commentaryMaxTokens = 88;
    config.commentaryTemperature = 0.35;
    config.commentaryStreamingEnabled = false;
    config.commentaryOcrFallbackEnabled = true;
    config.commentaryStreamChunkChars = 19;
    config.commentaryMaxResponseChars = 76;
    config.commentaryPreambleText = QStringLiteral("让我先看看。");
    config.screenCommentaryAutoEnabled = true;
    config.screenCommentaryAutoIntervalMinutes = 17;

    QVERIFY(repository.save(config));

    QFile savedFile(configPath);
    QVERIFY(savedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject savedRoot = QJsonDocument::fromJson(savedFile.readAll()).object();
    savedFile.close();
    QCOMPARE(savedRoot.value(QStringLiteral("appearance")).toObject().value(QStringLiteral("ascii_width")).toInt(), 72);
    QCOMPARE(savedRoot.value(QStringLiteral("audio")).toObject().value(QStringLiteral("tts_provider")).toString(), QStringLiteral("edge"));
    QCOMPARE(savedRoot.value(QStringLiteral("audio")).toObject().value(QStringLiteral("tts_voice")).toString(), QStringLiteral("zh-CN-YunxiNeural"));
    QCOMPARE(savedRoot.value(QStringLiteral("vision")).toObject().value(QStringLiteral("target_fps")).toInt(), 12);
    QCOMPARE(savedRoot.value(QStringLiteral("wakeup")).toObject().value(QStringLiteral("language")).toString(), QStringLiteral("zh-CN"));

    const AppConfig loaded = repository.load();
    QCOMPARE(loaded.version, config.version);
    QCOMPARE(loaded.debugMode, config.debugMode);
    QCOMPARE(loaded.firstRun, config.firstRun);
    QCOMPARE(loaded.startMinimized, config.startMinimized);
    QCOMPARE(loaded.autoStartOnLogin, config.autoStartOnLogin);
    QCOMPARE(loaded.activeCharacterId, config.activeCharacterId);
    QCOMPARE(loaded.preferredPosition, config.preferredPosition);
    QCOMPARE(loaded.appearanceAsciiWidth, config.appearanceAsciiWidth);
    QCOMPARE(loaded.appearanceFontSizePx, config.appearanceFontSizePx);
    QCOMPARE(loaded.lastVisible, config.lastVisible);
    QCOMPARE(loaded.offlineMode, config.offlineMode);
    QCOMPARE(loaded.fullScreenPause, config.fullScreenPause);
    QCOMPARE(loaded.residentMode, config.residentMode);
    QCOMPARE(loaded.idleThresholdSeconds, config.idleThresholdSeconds);
    QCOMPARE(loaded.idleJitterMinSeconds, config.idleJitterMinSeconds);
    QCOMPARE(loaded.idleJitterMaxSeconds, config.idleJitterMaxSeconds);
    QCOMPARE(loaded.autoDismissSeconds, config.autoDismissSeconds);
    QCOMPARE(loaded.cameraEnabled, config.cameraEnabled);
    QCOMPARE(loaded.cameraConsentGranted, config.cameraConsentGranted);
    QCOMPARE(loaded.cameraIndex, config.cameraIndex);
    QCOMPARE(loaded.cameraTargetFps, config.cameraTargetFps);
    QCOMPARE(loaded.eyeTrackingEnabled, config.eyeTrackingEnabled);
    QCOMPARE(loaded.periodicScanEnabled, config.periodicScanEnabled);
    QCOMPARE(loaded.periodicScanIntervalMinutes, config.periodicScanIntervalMinutes);
    QCOMPARE(loaded.scriptedEntranceEnabled, config.scriptedEntranceEnabled);
    QCOMPARE(loaded.scriptedTrajectoryPath, config.scriptedTrajectoryPath);
    QCOMPARE(loaded.audioOutputReactive, config.audioOutputReactive);
    QCOMPARE(loaded.audioOutputPollIntervalMs, config.audioOutputPollIntervalMs);
    QCOMPARE(loaded.audioMonitorIgnoreCurrentProcessAudio, config.audioMonitorIgnoreCurrentProcessAudio);
    QCOMPARE(loaded.audioMonitorPreferMediaSessions, config.audioMonitorPreferMediaSessions);
    QCOMPARE(loaded.audioMonitorIncludeMasterPeakFallback, config.audioMonitorIncludeMasterPeakFallback);
    QCOMPARE(loaded.voiceScriptsPath, config.voiceScriptsPath);
    QCOMPARE(loaded.ttsProvider, config.ttsProvider);
    QCOMPARE(loaded.ttsVoice, config.ttsVoice);
    QCOMPARE(loaded.ttsRate, config.ttsRate);
    QCOMPARE(loaded.audioVolume, config.audioVolume);
    QCOMPARE(loaded.audioCacheEnabled, config.audioCacheEnabled);
    QCOMPARE(loaded.microphoneEnabled, config.microphoneEnabled);
    QCOMPARE(loaded.voiceInputMode, config.voiceInputMode);
    QCOMPARE(loaded.asrProvider, config.asrProvider);
    QCOMPARE(loaded.asrApiKey, config.asrApiKey);
    QCOMPARE(loaded.asrModel, config.asrModel);
    QCOMPARE(loaded.asrBaseUrl, config.asrBaseUrl);
    QCOMPARE(loaded.asrTemperature, config.asrTemperature);
    QCOMPARE(loaded.asrPrompt, config.asrPrompt);
    QCOMPARE(loaded.wakeupEnabled, config.wakeupEnabled);
    QCOMPARE(loaded.wakeupPhrases, config.wakeupPhrases);
    QCOMPARE(loaded.wakeupLanguage, config.wakeupLanguage);
    QCOMPARE(loaded.idleInvasion.enabled, config.idleInvasion.enabled);
    QCOMPARE(loaded.idleInvasion.startDelayMs, config.idleInvasion.startDelayMs);
    QCOMPARE(loaded.idleInvasion.initialSpawnIntervalMs, config.idleInvasion.initialSpawnIntervalMs);
    QCOMPARE(loaded.idleInvasion.minSpawnIntervalMs, config.idleInvasion.minSpawnIntervalMs);
    QCOMPARE(loaded.idleInvasion.maxInvaders, config.idleInvasion.maxInvaders);
    QCOMPARE(loaded.idleInvasion.scale, config.idleInvasion.scale);
    QCOMPARE(loaded.idleInvasion.cellPadding, config.idleInvasion.cellPadding);
    QCOMPARE(loaded.idleInvasion.participatingGifs, config.idleInvasion.participatingGifs);
    QCOMPARE(loaded.idleInvasion.retreatStyle, config.idleInvasion.retreatStyle);
    QCOMPARE(loaded.windowX, config.windowX);
    QCOMPARE(loaded.windowY, config.windowY);
    QCOMPARE(loaded.llmProvider, config.llmProvider);
    QCOMPARE(loaded.llmModel, config.llmModel);
    QCOMPARE(loaded.llmApiKey, config.llmApiKey);
    QCOMPARE(loaded.llmBaseUrl, config.llmBaseUrl);
    QCOMPARE(loaded.commentarySystemPrompt, config.commentarySystemPrompt);
    QCOMPARE(loaded.commentaryUserPrompt, config.commentaryUserPrompt);
    QCOMPARE(loaded.commentaryNoImagePrompt, config.commentaryNoImagePrompt);
    QCOMPARE(loaded.commentaryMaxTokens, config.commentaryMaxTokens);
    QCOMPARE(loaded.commentaryTemperature, config.commentaryTemperature);
    QCOMPARE(loaded.commentaryStreamingEnabled, config.commentaryStreamingEnabled);
    QCOMPARE(loaded.commentaryOcrFallbackEnabled, config.commentaryOcrFallbackEnabled);
    QCOMPARE(loaded.commentaryStreamChunkChars, config.commentaryStreamChunkChars);
    QCOMPARE(loaded.commentaryMaxResponseChars, config.commentaryMaxResponseChars);
    QCOMPARE(loaded.commentaryPreambleText, config.commentaryPreambleText);
    QCOMPARE(loaded.screenCommentaryAutoEnabled, config.screenCommentaryAutoEnabled);
    QCOMPARE(loaded.screenCommentaryAutoIntervalMinutes, config.screenCommentaryAutoIntervalMinutes);
}

void ConfigRepositoryTest::bootstrapFromLegacyConfig()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString legacyPath = tempDir.filePath(QStringLiteral("legacy.json"));
    const QString targetPath = tempDir.filePath(QStringLiteral("target.json"));

    QFile legacyFile(legacyPath);
    QVERIFY(legacyFile.open(QIODevice::WriteOnly | QIODevice::Text));
    legacyFile.write("{\"version\":\"1.2.3\",\"appearance\":{\"theme\":\"default\",\"position\":\"left\",\"ascii_width\":72,\"font_size_px\":10},\"behavior\":{\"debug_mode\":false,\"first_run\":false,\"full_screen_pause\":false,\"resident_mode\":true,\"auto_start_on_login\":true,\"scripted_entrance_enabled\":true,\"scripted_trajectory_path\":\"recorded_paths/legacy\",\"audio_output_reactive\":false,\"voice_scripts_path\":\"characters/default/scripts.json\",\"audio_output_monitor\":{\"poll_interval_ms\":900,\"ignore_current_process_audio\":false,\"prefer_media_sessions\":false,\"include_master_peak_fallback\":true}},\"trigger\":{\"idle_threshold_seconds\":240,\"jitter_range_seconds\":[-20,40],\"auto_dismiss_seconds\":18},\"audio\":{\"tts_provider\":\"edge\",\"tts_voice\":\"zh-CN-YunxiNeural\",\"tts_rate\":\"+15%\",\"volume\":0.55,\"cache_enabled\":false,\"microphone_enabled\":true,\"voice_input_mode\":\"push_to_talk\",\"asr_provider\":\"zhipu_asr\",\"asr_api_key\":\"legacy-asr-key\",\"asr_model\":\"glm-asr-2512\",\"asr_base_url\":\"https://open.bigmodel.cn/api/paas/v4/audio/transcriptions\",\"asr_temperature\":0.2,\"asr_prompt\":\"legacy voice\"},\"vision\":{\"camera_enabled\":true,\"camera_consent_granted\":true,\"camera_index\":1,\"target_fps\":10,\"eye_tracking_enabled\":false,\"periodic_scan_enabled\":false,\"periodic_scan_interval_minutes\":45},\"wakeup\":{\"enabled\":true,\"phrases\":[\"小爱同学\",\"看看屏幕\"],\"language\":\"zh-CN\"},\"idle_invasion\":{\"enabled\":false,\"start_delay_ms\":120000,\"initial_spawn_interval_ms\":9000,\"min_spawn_interval_ms\":1500,\"max_invaders\":33,\"scale\":0.65,\"cell_padding\":12,\"participating_gifs\":[\"state1.gif\",\"state5.gif\"],\"retreat_style\":\"ripple\"},\"llm\":{\"provider\":\"openai\",\"model\":\"gpt-5-mini\",\"api_key\":\"legacy-key\",\"base_url\":\"https://api.openai.com/v1\",\"commentary_system_prompt\":\"legacy system\",\"commentary_user_prompt\":\"legacy user\",\"commentary_no_image_prompt\":\"legacy no image\",\"commentary_max_tokens\":72,\"commentary_temperature\":0.4},\"screen_commentary\":{\"streaming_enabled\":false,\"ocr_fallback_enabled\":true,\"stream_chunk_chars\":18,\"max_response_chars\":66,\"preamble_text\":\"legacy preamble\",\"auto_enabled\":true,\"auto_interval_minutes\":23}}\n");
    legacyFile.close();

    ConfigRepository repository(targetPath);
    QVERIFY(repository.bootstrapFromLegacy(legacyPath));
    QVERIFY(repository.exists());

    const AppConfig loaded = repository.load();
    QCOMPARE(loaded.version, QStringLiteral("1.2.3"));
    QCOMPARE(loaded.activeCharacterId, QStringLiteral("default"));
    QCOMPARE(loaded.preferredPosition, QStringLiteral("left"));
    QCOMPARE(loaded.debugMode, false);
    QCOMPARE(loaded.firstRun, false);
    QCOMPARE(loaded.fullScreenPause, false);
    QCOMPARE(loaded.residentMode, true);
    QCOMPARE(loaded.autoStartOnLogin, true);
    QCOMPARE(loaded.appearanceAsciiWidth, 72);
    QCOMPARE(loaded.appearanceFontSizePx, 10);
    QCOMPARE(loaded.idleThresholdSeconds, 240);
    QCOMPARE(loaded.idleJitterMinSeconds, -20);
    QCOMPARE(loaded.idleJitterMaxSeconds, 40);
    QCOMPARE(loaded.autoDismissSeconds, 18);
    QCOMPARE(loaded.cameraEnabled, true);
    QCOMPARE(loaded.cameraConsentGranted, true);
    QCOMPARE(loaded.cameraIndex, 1);
    QCOMPARE(loaded.cameraTargetFps, 10);
    QCOMPARE(loaded.eyeTrackingEnabled, false);
    QCOMPARE(loaded.periodicScanEnabled, false);
    QCOMPARE(loaded.periodicScanIntervalMinutes, 45);
    QCOMPARE(loaded.scriptedEntranceEnabled, true);
    QCOMPARE(loaded.scriptedTrajectoryPath, QStringLiteral("recorded_paths/legacy"));
    QCOMPARE(loaded.audioOutputReactive, false);
    QCOMPARE(loaded.audioOutputPollIntervalMs, 900);
    QCOMPARE(loaded.audioMonitorIgnoreCurrentProcessAudio, false);
    QCOMPARE(loaded.audioMonitorPreferMediaSessions, false);
    QCOMPARE(loaded.audioMonitorIncludeMasterPeakFallback, true);
    QCOMPARE(loaded.voiceScriptsPath, QStringLiteral("characters/default/scripts.json"));
    QCOMPARE(loaded.ttsProvider, QStringLiteral("edge"));
    QCOMPARE(loaded.ttsVoice, QStringLiteral("zh-CN-YunxiNeural"));
    QCOMPARE(loaded.ttsRate, QStringLiteral("+15%"));
    QCOMPARE(loaded.audioVolume, 0.55);
    QCOMPARE(loaded.audioCacheEnabled, false);
    QCOMPARE(loaded.microphoneEnabled, true);
    QCOMPARE(loaded.voiceInputMode, QStringLiteral("push_to_talk"));
    QCOMPARE(loaded.asrProvider, QStringLiteral("zhipu_asr"));
    QCOMPARE(loaded.asrApiKey, QStringLiteral("legacy-asr-key"));
    QCOMPARE(loaded.asrModel, QStringLiteral("glm-asr-2512"));
    QCOMPARE(loaded.asrBaseUrl, QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions"));
    QCOMPARE(loaded.asrTemperature, 0.2);
    QCOMPARE(loaded.asrPrompt, QStringLiteral("legacy voice"));
    QCOMPARE(loaded.wakeupEnabled, true);
    QCOMPARE(loaded.wakeupPhrases, QStringList({ QStringLiteral("小爱同学"), QStringLiteral("看看屏幕") }));
    QCOMPARE(loaded.wakeupLanguage, QStringLiteral("zh-CN"));
    QCOMPARE(loaded.idleInvasion.enabled, false);
    QCOMPARE(loaded.idleInvasion.startDelayMs, 120000);
    QCOMPARE(loaded.idleInvasion.initialSpawnIntervalMs, 9000);
    QCOMPARE(loaded.idleInvasion.minSpawnIntervalMs, 1500);
    QCOMPARE(loaded.idleInvasion.maxInvaders, 33);
    QCOMPARE(loaded.idleInvasion.scale, 0.65);
    QCOMPARE(loaded.idleInvasion.cellPadding, 12);
    QCOMPARE(loaded.idleInvasion.participatingGifs, QStringList({ QStringLiteral("state1.gif"), QStringLiteral("state5.gif") }));
    QCOMPARE(loaded.idleInvasion.retreatStyle, QStringLiteral("ripple"));
    QCOMPARE(loaded.llmProvider, QStringLiteral("openai"));
    QCOMPARE(loaded.llmModel, QStringLiteral("gpt-5-mini"));
    QCOMPARE(loaded.llmApiKey, QStringLiteral("legacy-key"));
    QCOMPARE(loaded.commentarySystemPrompt, QStringLiteral("legacy system"));
    QCOMPARE(loaded.commentaryUserPrompt, QStringLiteral("legacy user"));
    QCOMPARE(loaded.commentaryNoImagePrompt, QStringLiteral("legacy no image"));
    QCOMPARE(loaded.commentaryMaxTokens, 72);
    QCOMPARE(loaded.commentaryTemperature, 0.4);
    QCOMPARE(loaded.commentaryStreamingEnabled, false);
    QCOMPARE(loaded.commentaryOcrFallbackEnabled, true);
    QCOMPARE(loaded.commentaryStreamChunkChars, 18);
    QCOMPARE(loaded.commentaryMaxResponseChars, 66);
    QCOMPARE(loaded.commentaryPreambleText, QStringLiteral("legacy preamble"));
    QCOMPARE(loaded.screenCommentaryAutoEnabled, true);
    QCOMPARE(loaded.screenCommentaryAutoIntervalMinutes, 23);
}

void ConfigRepositoryTest::saveFailureReturnsErrorMessage()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("missing/subdir/config.json"));
    ConfigRepository repository(configPath);

    QString errorMessage;
    QVERIFY(!repository.save(AppConfig{}, &errorMessage));
    QVERIFY(!errorMessage.trimmed().isEmpty());
}

void ConfigRepositoryTest::migratesLegacyNonEdgeTtsProvider()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile configFile(configPath);
    QVERIFY(configFile.open(QIODevice::WriteOnly | QIODevice::Text));
    configFile.write("{\"audio\":{\"asr_provider\":\"google\",\"asr_api_key\":\"\",\"asr_model\":\"\",\"asr_base_url\":\"\"}}\n");
    configFile.close();

    ConfigRepository repository(configPath);
    const AppConfig loaded = repository.load();
    QCOMPARE(loaded.asrProvider, QStringLiteral("zhipu_asr"));
    QVERIFY(loaded.asrProviderMigratedFromGoogle);
}

void ConfigRepositoryTest::migratesLegacyGoogleAsrProvider()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath(QStringLiteral("config.json"));
    QFile configFile(configPath);
    QVERIFY(configFile.open(QIODevice::WriteOnly | QIODevice::Text));
    configFile.write("{\"audio\":{\"tts_provider\":\"openai\",\"asr_provider\":\"google\"}}\n");
    configFile.close();

    ConfigRepository repository(configPath);
    const AppConfig loaded = repository.load();
    QCOMPARE(loaded.ttsProvider, QStringLiteral("edge"));
    QVERIFY(loaded.ttsProviderMigratedToEdge);
    QCOMPARE(loaded.asrProvider, QStringLiteral("zhipu_asr"));
    QVERIFY(loaded.asrProviderMigratedFromGoogle);
}

QTEST_MAIN(ConfigRepositoryTest)

#include "test_config_repository.moc"
