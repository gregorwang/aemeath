#include "runtime/config_repository.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <utility>

namespace {

QString normalizeNativeAsrProvider(const QString &provider, bool *migratedFromGoogle = nullptr)
{
    const QString normalized = provider.trimmed().toLower();
    const bool migrated = normalized == QStringLiteral("google")
        || normalized == QStringLiteral("google_webspeech");
    if (migratedFromGoogle) {
        *migratedFromGoogle = migrated;
    }
    if (migrated) {
        return QStringLiteral("zhipu_asr");
    }
    return normalized;
}

QString normalizeNativeTtsProvider(const QString &provider, bool *migratedToEdge = nullptr)
{
    const QString normalized = provider.trimmed().toLower();
    const bool migrated = !normalized.isEmpty() && normalized != QStringLiteral("edge");
    if (migratedToEdge) {
        *migratedToEdge = migrated;
    }
    return QStringLiteral("edge");
}

}

ConfigRepository::ConfigRepository(QString configFilePath)
    : m_configFilePath(std::move(configFilePath))
{
}

AppConfig ConfigRepository::load() const
{
    QFile file(m_configFilePath);
    if (!file.exists()) {
        return {};
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }

    const QJsonObject root = doc.object();
    AppConfig config;
    config.version = root.value(QStringLiteral("version")).toString(config.version);
    config.startMinimized = root.value(QStringLiteral("start_minimized")).toBool(config.startMinimized);
    const QJsonObject appearance = root.value(QStringLiteral("appearance")).toObject();
    config.activeCharacterId = appearance.value(QStringLiteral("theme"))
        .toString(appearance.value(QStringLiteral("active_character_id")).toString(config.activeCharacterId));
    config.preferredPosition = appearance.value(QStringLiteral("position")).toString(config.preferredPosition);
    config.appearanceAsciiWidth = qMax(20, appearance.value(QStringLiteral("ascii_width")).toInt(config.appearanceAsciiWidth));
    config.appearanceFontSizePx = qMax(6, appearance.value(QStringLiteral("font_size_px")).toInt(config.appearanceFontSizePx));

    const QJsonObject behavior = root.value(QStringLiteral("behavior")).toObject();
    config.debugMode = behavior.value(QStringLiteral("debug_mode")).toBool(config.debugMode);
    config.firstRun = behavior.value(QStringLiteral("first_run")).toBool(config.firstRun);
    config.offlineMode = behavior.value(QStringLiteral("offline_mode")).toBool(config.offlineMode);
    config.fullScreenPause = behavior.value(QStringLiteral("full_screen_pause")).toBool(config.fullScreenPause);
    config.residentMode = behavior.value(QStringLiteral("resident_mode")).toBool(config.residentMode);
    config.autoStartOnLogin = behavior.value(QStringLiteral("auto_start_on_login")).toBool(config.autoStartOnLogin);
    config.scriptedEntranceEnabled = behavior.value(QStringLiteral("scripted_entrance_enabled")).toBool(config.scriptedEntranceEnabled);
    config.scriptedTrajectoryPath = behavior.value(QStringLiteral("scripted_trajectory_path")).toString(config.scriptedTrajectoryPath);
    config.audioOutputReactive = behavior.value(QStringLiteral("audio_output_reactive")).toBool(config.audioOutputReactive);
    config.voiceScriptsPath = behavior.value(QStringLiteral("voice_scripts_path")).toString(config.voiceScriptsPath);

    const QJsonObject vision = root.value(QStringLiteral("vision")).toObject();
    config.cameraEnabled = vision.contains(QStringLiteral("camera_enabled"))
        ? vision.value(QStringLiteral("camera_enabled")).toBool(config.cameraEnabled)
        : behavior.value(QStringLiteral("camera_enabled")).toBool(config.cameraEnabled);
    config.cameraConsentGranted = vision.contains(QStringLiteral("camera_consent_granted"))
        ? vision.value(QStringLiteral("camera_consent_granted")).toBool(config.cameraConsentGranted)
        : behavior.value(QStringLiteral("camera_consent_granted")).toBool(config.cameraConsentGranted);
    config.cameraIndex = qMax(
        0,
        vision.contains(QStringLiteral("camera_index"))
            ? vision.value(QStringLiteral("camera_index")).toInt(config.cameraIndex)
            : behavior.value(QStringLiteral("camera_index")).toInt(config.cameraIndex));
    config.cameraTargetFps = qBound(
        1,
        vision.contains(QStringLiteral("target_fps"))
            ? vision.value(QStringLiteral("target_fps")).toInt(config.cameraTargetFps)
            : behavior.value(QStringLiteral("camera_target_fps")).toInt(config.cameraTargetFps),
        30);
    config.eyeTrackingEnabled = vision.contains(QStringLiteral("eye_tracking_enabled"))
        ? vision.value(QStringLiteral("eye_tracking_enabled")).toBool(config.eyeTrackingEnabled)
        : behavior.value(QStringLiteral("eye_tracking_enabled")).toBool(config.eyeTrackingEnabled);
    config.periodicScanEnabled = vision.contains(QStringLiteral("periodic_scan_enabled"))
        ? vision.value(QStringLiteral("periodic_scan_enabled")).toBool(config.periodicScanEnabled)
        : behavior.value(QStringLiteral("periodic_scan_enabled")).toBool(config.periodicScanEnabled);
    config.periodicScanIntervalMinutes = vision.contains(QStringLiteral("periodic_scan_interval_minutes"))
        ? vision.value(QStringLiteral("periodic_scan_interval_minutes")).toInt(config.periodicScanIntervalMinutes)
        : behavior.value(QStringLiteral("periodic_scan_interval_minutes")).toInt(config.periodicScanIntervalMinutes);

    const QJsonObject audio = root.value(QStringLiteral("audio")).toObject();
    config.ttsProvider = normalizeNativeTtsProvider(
        audio.value(QStringLiteral("tts_provider")).toString(config.ttsProvider),
        &config.ttsProviderMigratedToEdge);
    config.ttsVoice = audio.contains(QStringLiteral("tts_voice"))
        ? audio.value(QStringLiteral("tts_voice")).toString(config.ttsVoice)
        : behavior.value(QStringLiteral("tts_voice")).toString(config.ttsVoice);
    config.ttsRate = audio.contains(QStringLiteral("tts_rate"))
        ? audio.value(QStringLiteral("tts_rate")).toString(config.ttsRate)
        : behavior.value(QStringLiteral("tts_rate")).toString(config.ttsRate);
    config.audioVolume = audio.value(QStringLiteral("volume")).toDouble(config.audioVolume);
    config.audioCacheEnabled = audio.contains(QStringLiteral("cache_enabled"))
        ? audio.value(QStringLiteral("cache_enabled")).toBool(config.audioCacheEnabled)
        : behavior.value(QStringLiteral("audio_cache_enabled")).toBool(config.audioCacheEnabled);
    config.microphoneEnabled = audio.contains(QStringLiteral("microphone_enabled"))
        ? audio.value(QStringLiteral("microphone_enabled")).toBool(config.microphoneEnabled)
        : behavior.value(QStringLiteral("microphone_enabled")).toBool(config.microphoneEnabled);
    config.voiceInputMode = audio.contains(QStringLiteral("voice_input_mode"))
        ? audio.value(QStringLiteral("voice_input_mode")).toString(config.voiceInputMode)
        : behavior.value(QStringLiteral("voice_input_mode")).toString(config.voiceInputMode);
    config.asrProvider = audio.contains(QStringLiteral("asr_provider"))
        ? audio.value(QStringLiteral("asr_provider")).toString(config.asrProvider)
        : behavior.value(QStringLiteral("asr_provider")).toString(config.asrProvider);
    config.asrProvider = normalizeNativeAsrProvider(config.asrProvider, &config.asrProviderMigratedFromGoogle);
    config.asrApiKey = audio.contains(QStringLiteral("asr_api_key"))
        ? audio.value(QStringLiteral("asr_api_key")).toString(config.asrApiKey)
        : behavior.value(QStringLiteral("asr_api_key")).toString(config.asrApiKey);
    config.asrModel = audio.contains(QStringLiteral("asr_model"))
        ? audio.value(QStringLiteral("asr_model")).toString(config.asrModel)
        : behavior.value(QStringLiteral("asr_model")).toString(config.asrModel);
    config.asrBaseUrl = audio.contains(QStringLiteral("asr_base_url"))
        ? audio.value(QStringLiteral("asr_base_url")).toString(config.asrBaseUrl)
        : behavior.value(QStringLiteral("asr_base_url")).toString(config.asrBaseUrl);
    config.asrTemperature = audio.contains(QStringLiteral("asr_temperature"))
        ? audio.value(QStringLiteral("asr_temperature")).toDouble(config.asrTemperature)
        : behavior.value(QStringLiteral("asr_temperature")).toDouble(config.asrTemperature);
    config.asrPrompt = audio.contains(QStringLiteral("asr_prompt"))
        ? audio.value(QStringLiteral("asr_prompt")).toString(config.asrPrompt)
        : behavior.value(QStringLiteral("asr_prompt")).toString(config.asrPrompt);

    const QJsonObject wakeup = root.value(QStringLiteral("wakeup")).toObject();
    config.wakeupEnabled = wakeup.contains(QStringLiteral("enabled"))
        ? wakeup.value(QStringLiteral("enabled")).toBool(config.wakeupEnabled)
        : behavior.value(QStringLiteral("wakeup_enabled")).toBool(config.wakeupEnabled);
    config.wakeupLanguage = wakeup.contains(QStringLiteral("language"))
        ? wakeup.value(QStringLiteral("language")).toString(config.wakeupLanguage)
        : behavior.value(QStringLiteral("wakeup_language")).toString(config.wakeupLanguage);
    const QJsonArray wakeupPhrases = wakeup.contains(QStringLiteral("phrases"))
        ? wakeup.value(QStringLiteral("phrases")).toArray()
        : behavior.value(QStringLiteral("wakeup_phrases")).toArray();
    if (!wakeupPhrases.isEmpty()) {
        QStringList parsedPhrases;
        parsedPhrases.reserve(wakeupPhrases.size());
        for (const QJsonValue &value : wakeupPhrases) {
            const QString phrase = value.toString().trimmed();
            if (!phrase.isEmpty()) {
                parsedPhrases.push_back(phrase);
            }
        }
        if (!parsedPhrases.isEmpty()) {
            config.wakeupPhrases = parsedPhrases;
        }
    }

    const QJsonObject audioMonitor = behavior.value(QStringLiteral("audio_output_monitor")).toObject();
    config.audioOutputPollIntervalMs = audioMonitor.value(QStringLiteral("poll_interval_ms")).toInt(config.audioOutputPollIntervalMs);
    config.audioMonitorIgnoreCurrentProcessAudio = audioMonitor.value(QStringLiteral("ignore_current_process_audio")).toBool(config.audioMonitorIgnoreCurrentProcessAudio);
    config.audioMonitorPreferMediaSessions = audioMonitor.value(QStringLiteral("prefer_media_sessions")).toBool(config.audioMonitorPreferMediaSessions);
    config.audioMonitorIncludeMasterPeakFallback = audioMonitor.value(QStringLiteral("include_master_peak_fallback")).toBool(config.audioMonitorIncludeMasterPeakFallback);

    const QJsonObject trigger = root.value(QStringLiteral("trigger")).toObject();
    config.idleThresholdSeconds = trigger.value(QStringLiteral("idle_threshold_seconds")).toInt(config.idleThresholdSeconds);
    const QJsonArray jitterRange = trigger.value(QStringLiteral("jitter_range_seconds")).toArray();
    if (jitterRange.size() == 2) {
        config.idleJitterMinSeconds = jitterRange.at(0).toInt(config.idleJitterMinSeconds);
        config.idleJitterMaxSeconds = jitterRange.at(1).toInt(config.idleJitterMaxSeconds);
    }
    config.autoDismissSeconds = trigger.value(QStringLiteral("auto_dismiss_seconds")).toInt(config.autoDismissSeconds);

    const QJsonObject idleInvasion = root.value(QStringLiteral("idle_invasion")).toObject();
    config.idleInvasion.enabled = idleInvasion.value(QStringLiteral("enabled")).toBool(config.idleInvasion.enabled);
    config.idleInvasion.startDelayMs = idleInvasion.value(QStringLiteral("start_delay_ms")).toInt(config.idleInvasion.startDelayMs);
    config.idleInvasion.initialSpawnIntervalMs = idleInvasion.value(QStringLiteral("initial_spawn_interval_ms")).toInt(config.idleInvasion.initialSpawnIntervalMs);
    config.idleInvasion.minSpawnIntervalMs = idleInvasion.value(QStringLiteral("min_spawn_interval_ms")).toInt(config.idleInvasion.minSpawnIntervalMs);
    config.idleInvasion.maxInvaders = idleInvasion.value(QStringLiteral("max_invaders")).toInt(config.idleInvasion.maxInvaders);
    config.idleInvasion.scale = idleInvasion.value(QStringLiteral("scale")).toDouble(config.idleInvasion.scale);
    config.idleInvasion.cellPadding = idleInvasion.value(QStringLiteral("cell_padding")).toInt(config.idleInvasion.cellPadding);
    config.idleInvasion.retreatStyle = idleInvasion.value(QStringLiteral("retreat_style")).toString(config.idleInvasion.retreatStyle);
    const QJsonArray invasionGifs = idleInvasion.value(QStringLiteral("participating_gifs")).toArray();
    if (!invasionGifs.isEmpty()) {
        QStringList parsedGifs;
        parsedGifs.reserve(invasionGifs.size());
        for (const QJsonValue &value : invasionGifs) {
            const QString gif = value.toString().trimmed();
            if (!gif.isEmpty()) {
                parsedGifs.push_back(gif);
            }
        }
        if (!parsedGifs.isEmpty()) {
            config.idleInvasion.participatingGifs = parsedGifs;
        }
    }

    const QJsonObject window = root.value(QStringLiteral("window")).toObject();
    config.lastVisible = window.value(QStringLiteral("visible")).toBool(config.lastVisible);
    config.windowX = window.value(QStringLiteral("x")).toInt(config.windowX);
    config.windowY = window.value(QStringLiteral("y")).toInt(config.windowY);

    const QJsonObject llm = root.value(QStringLiteral("llm")).toObject();
    config.llmProvider = llm.value(QStringLiteral("provider")).toString(config.llmProvider);
    config.llmModel = llm.value(QStringLiteral("model")).toString(config.llmModel);
    config.llmApiKey = llm.value(QStringLiteral("api_key")).toString(config.llmApiKey);
    config.llmBaseUrl = llm.value(QStringLiteral("base_url")).toString(config.llmBaseUrl);
    config.commentarySystemPrompt = llm.value(QStringLiteral("commentary_system_prompt")).toString(config.commentarySystemPrompt);
    config.commentaryUserPrompt = llm.value(QStringLiteral("commentary_user_prompt")).toString(config.commentaryUserPrompt);
    config.commentaryNoImagePrompt = llm.value(QStringLiteral("commentary_no_image_prompt")).toString(config.commentaryNoImagePrompt);
    config.commentaryMaxTokens = llm.value(QStringLiteral("commentary_max_tokens")).toInt(config.commentaryMaxTokens);
    config.commentaryTemperature = llm.value(QStringLiteral("commentary_temperature")).toDouble(config.commentaryTemperature);

    const QJsonObject screenCommentary = root.value(QStringLiteral("screen_commentary")).toObject();
    config.commentaryStreamingEnabled = screenCommentary.value(QStringLiteral("streaming_enabled")).toBool(config.commentaryStreamingEnabled);
    config.commentaryOcrFallbackEnabled = screenCommentary.value(QStringLiteral("ocr_fallback_enabled")).toBool(config.commentaryOcrFallbackEnabled);
    config.commentaryStreamChunkChars = screenCommentary.value(QStringLiteral("stream_chunk_chars")).toInt(config.commentaryStreamChunkChars);
    config.commentaryMaxResponseChars = screenCommentary.value(QStringLiteral("max_response_chars")).toInt(config.commentaryMaxResponseChars);
    config.commentaryPreambleText = screenCommentary.value(QStringLiteral("preamble_text")).toString(config.commentaryPreambleText);
    config.screenCommentaryAutoEnabled = screenCommentary.value(QStringLiteral("auto_enabled")).toBool(config.screenCommentaryAutoEnabled);
    config.screenCommentaryAutoIntervalMinutes = screenCommentary.value(QStringLiteral("auto_interval_minutes")).toInt(config.screenCommentaryAutoIntervalMinutes);
    return config;
}

bool ConfigRepository::save(const AppConfig &config, QString *errorMessage) const
{
    if (errorMessage) {
        errorMessage->clear();
    }

    QJsonObject behavior;
    behavior.insert(QStringLiteral("debug_mode"), config.debugMode);
    behavior.insert(QStringLiteral("first_run"), config.firstRun);
    behavior.insert(QStringLiteral("offline_mode"), config.offlineMode);
    behavior.insert(QStringLiteral("full_screen_pause"), config.fullScreenPause);
    behavior.insert(QStringLiteral("resident_mode"), config.residentMode);
    behavior.insert(QStringLiteral("auto_start_on_login"), config.autoStartOnLogin);
    behavior.insert(QStringLiteral("scripted_entrance_enabled"), config.scriptedEntranceEnabled);
    behavior.insert(QStringLiteral("scripted_trajectory_path"), config.scriptedTrajectoryPath);
    behavior.insert(QStringLiteral("audio_output_reactive"), config.audioOutputReactive);
    behavior.insert(QStringLiteral("voice_scripts_path"), config.voiceScriptsPath);

    QJsonObject audioMonitor;
    audioMonitor.insert(QStringLiteral("poll_interval_ms"), config.audioOutputPollIntervalMs);
    audioMonitor.insert(QStringLiteral("ignore_current_process_audio"), config.audioMonitorIgnoreCurrentProcessAudio);
    audioMonitor.insert(QStringLiteral("prefer_media_sessions"), config.audioMonitorPreferMediaSessions);
    audioMonitor.insert(QStringLiteral("include_master_peak_fallback"), config.audioMonitorIncludeMasterPeakFallback);
    behavior.insert(QStringLiteral("audio_output_monitor"), audioMonitor);

    QJsonObject idleInvasion;
    idleInvasion.insert(QStringLiteral("enabled"), config.idleInvasion.enabled);
    idleInvasion.insert(QStringLiteral("start_delay_ms"), config.idleInvasion.startDelayMs);
    idleInvasion.insert(QStringLiteral("initial_spawn_interval_ms"), config.idleInvasion.initialSpawnIntervalMs);
    idleInvasion.insert(QStringLiteral("min_spawn_interval_ms"), config.idleInvasion.minSpawnIntervalMs);
    idleInvasion.insert(QStringLiteral("max_invaders"), config.idleInvasion.maxInvaders);
    idleInvasion.insert(QStringLiteral("scale"), config.idleInvasion.scale);
    idleInvasion.insert(QStringLiteral("cell_padding"), config.idleInvasion.cellPadding);
    idleInvasion.insert(QStringLiteral("retreat_style"), config.idleInvasion.retreatStyle);
    QJsonArray invasionGifs;
    for (const QString &gif : config.idleInvasion.participatingGifs) {
        invasionGifs.append(gif);
    }
    idleInvasion.insert(QStringLiteral("participating_gifs"), invasionGifs);

    QJsonObject root;
    root.insert(QStringLiteral("version"), config.version);
    root.insert(QStringLiteral("start_minimized"), config.startMinimized);
    QJsonObject appearance;
    appearance.insert(QStringLiteral("theme"), config.activeCharacterId);
    appearance.insert(QStringLiteral("active_character_id"), config.activeCharacterId);
    appearance.insert(QStringLiteral("position"), config.preferredPosition);
    appearance.insert(QStringLiteral("ascii_width"), qMax(20, config.appearanceAsciiWidth));
    appearance.insert(QStringLiteral("font_size_px"), qMax(6, config.appearanceFontSizePx));
    root.insert(QStringLiteral("appearance"), appearance);
    root.insert(QStringLiteral("behavior"), behavior);
    root.insert(QStringLiteral("idle_invasion"), idleInvasion);

    QJsonObject vision;
    vision.insert(QStringLiteral("camera_enabled"), config.cameraEnabled);
    vision.insert(QStringLiteral("camera_consent_granted"), config.cameraConsentGranted);
    vision.insert(QStringLiteral("camera_index"), qMax(0, config.cameraIndex));
    vision.insert(QStringLiteral("target_fps"), qBound(1, config.cameraTargetFps, 30));
    vision.insert(QStringLiteral("eye_tracking_enabled"), config.eyeTrackingEnabled);
    vision.insert(QStringLiteral("periodic_scan_enabled"), config.periodicScanEnabled);
    vision.insert(QStringLiteral("periodic_scan_interval_minutes"), config.periodicScanIntervalMinutes);
    root.insert(QStringLiteral("vision"), vision);

    QJsonObject trigger;
    trigger.insert(QStringLiteral("idle_threshold_seconds"), qMax(1, config.idleThresholdSeconds));
    QJsonArray jitterRange;
    jitterRange.append(config.idleJitterMinSeconds);
    jitterRange.append(qMax(config.idleJitterMinSeconds, config.idleJitterMaxSeconds));
    trigger.insert(QStringLiteral("jitter_range_seconds"), jitterRange);
    trigger.insert(QStringLiteral("auto_dismiss_seconds"), qMax(1, config.autoDismissSeconds));
    root.insert(QStringLiteral("trigger"), trigger);

    QJsonObject window;
    window.insert(QStringLiteral("visible"), config.lastVisible);
    window.insert(QStringLiteral("x"), config.windowX);
    window.insert(QStringLiteral("y"), config.windowY);
    root.insert(QStringLiteral("window"), window);

    QJsonObject audio;
    audio.insert(QStringLiteral("tts_provider"), config.ttsProvider);
    audio.insert(QStringLiteral("tts_voice"), config.ttsVoice);
    audio.insert(QStringLiteral("tts_rate"), config.ttsRate);
    audio.insert(QStringLiteral("volume"), qBound(0.0, config.audioVolume, 1.0));
    audio.insert(QStringLiteral("cache_enabled"), config.audioCacheEnabled);
    audio.insert(QStringLiteral("microphone_enabled"), config.microphoneEnabled);
    audio.insert(QStringLiteral("voice_input_mode"), config.voiceInputMode);
    audio.insert(QStringLiteral("asr_provider"), config.asrProvider);
    audio.insert(QStringLiteral("asr_api_key"), config.asrApiKey);
    audio.insert(QStringLiteral("asr_model"), config.asrModel);
    audio.insert(QStringLiteral("asr_base_url"), config.asrBaseUrl);
    audio.insert(QStringLiteral("asr_temperature"), config.asrTemperature);
    audio.insert(QStringLiteral("asr_prompt"), config.asrPrompt);
    root.insert(QStringLiteral("audio"), audio);

    QJsonObject wakeup;
    wakeup.insert(QStringLiteral("enabled"), config.wakeupEnabled);
    QJsonArray wakeupPhrases;
    for (const QString &phrase : config.wakeupPhrases) {
        const QString trimmed = phrase.trimmed();
        if (!trimmed.isEmpty()) {
            wakeupPhrases.append(trimmed);
        }
    }
    wakeup.insert(QStringLiteral("phrases"), wakeupPhrases);
    wakeup.insert(QStringLiteral("language"), config.wakeupLanguage);
    root.insert(QStringLiteral("wakeup"), wakeup);

    QJsonObject llm;
    llm.insert(QStringLiteral("provider"), config.llmProvider);
    llm.insert(QStringLiteral("model"), config.llmModel);
    llm.insert(QStringLiteral("api_key"), config.llmApiKey);
    llm.insert(QStringLiteral("base_url"), config.llmBaseUrl);
    llm.insert(QStringLiteral("commentary_system_prompt"), config.commentarySystemPrompt);
    llm.insert(QStringLiteral("commentary_user_prompt"), config.commentaryUserPrompt);
    llm.insert(QStringLiteral("commentary_no_image_prompt"), config.commentaryNoImagePrompt);
    llm.insert(QStringLiteral("commentary_max_tokens"), config.commentaryMaxTokens);
    llm.insert(QStringLiteral("commentary_temperature"), config.commentaryTemperature);
    root.insert(QStringLiteral("llm"), llm);

    QJsonObject screenCommentary;
    screenCommentary.insert(QStringLiteral("streaming_enabled"), config.commentaryStreamingEnabled);
    screenCommentary.insert(QStringLiteral("ocr_fallback_enabled"), config.commentaryOcrFallbackEnabled);
    screenCommentary.insert(QStringLiteral("stream_chunk_chars"), config.commentaryStreamChunkChars);
    screenCommentary.insert(QStringLiteral("max_response_chars"), config.commentaryMaxResponseChars);
    screenCommentary.insert(QStringLiteral("preamble_text"), config.commentaryPreambleText);
    screenCommentary.insert(QStringLiteral("auto_enabled"), config.screenCommentaryAutoEnabled);
    screenCommentary.insert(QStringLiteral("auto_interval_minutes"), config.screenCommentaryAutoIntervalMinutes);
    root.insert(QStringLiteral("screen_commentary"), screenCommentary);

    QSaveFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    if (file.write("\n") != 1) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}

bool ConfigRepository::exists() const
{
    return QFileInfo::exists(m_configFilePath);
}

bool ConfigRepository::bootstrapFromLegacy(const QString &legacyConfigFilePath) const
{
    if (exists() || !QFileInfo::exists(legacyConfigFilePath)) {
        return false;
    }

    QFile legacyFile(legacyConfigFilePath);
    if (!legacyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray payload = legacyFile.readAll();
    if (payload.isEmpty()) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    AppConfig imported;
    imported.version = root.value(QStringLiteral("version")).toString(imported.version);
    const QJsonObject appearance = root.value(QStringLiteral("appearance")).toObject();
    imported.activeCharacterId = appearance.value(QStringLiteral("theme"))
        .toString(appearance.value(QStringLiteral("active_character_id")).toString(imported.activeCharacterId));
    imported.preferredPosition = appearance.value(QStringLiteral("position")).toString(imported.preferredPosition);
    imported.appearanceAsciiWidth = qMax(20, appearance.value(QStringLiteral("ascii_width")).toInt(imported.appearanceAsciiWidth));
    imported.appearanceFontSizePx = qMax(6, appearance.value(QStringLiteral("font_size_px")).toInt(imported.appearanceFontSizePx));

    const QJsonObject behavior = root.value(QStringLiteral("behavior")).toObject();
    imported.debugMode = behavior.value(QStringLiteral("debug_mode")).toBool(imported.debugMode);
    imported.firstRun = behavior.value(QStringLiteral("first_run")).toBool(imported.firstRun);
    imported.offlineMode = behavior.value(QStringLiteral("offline_mode")).toBool(imported.offlineMode);
    imported.fullScreenPause = behavior.value(QStringLiteral("full_screen_pause")).toBool(imported.fullScreenPause);
    imported.residentMode = behavior.value(QStringLiteral("resident_mode")).toBool(imported.residentMode);
    imported.autoStartOnLogin = behavior.value(QStringLiteral("auto_start_on_login")).toBool(imported.autoStartOnLogin);
    imported.scriptedEntranceEnabled = behavior.value(QStringLiteral("scripted_entrance_enabled")).toBool(imported.scriptedEntranceEnabled);
    imported.scriptedTrajectoryPath = behavior.value(QStringLiteral("scripted_trajectory_path")).toString(imported.scriptedTrajectoryPath);
    imported.audioOutputReactive = behavior.value(QStringLiteral("audio_output_reactive")).toBool(imported.audioOutputReactive);
    imported.voiceScriptsPath = behavior.value(QStringLiteral("voice_scripts_path")).toString(imported.voiceScriptsPath);

    const QJsonObject vision = root.value(QStringLiteral("vision")).toObject();
    imported.cameraEnabled = vision.contains(QStringLiteral("camera_enabled"))
        ? vision.value(QStringLiteral("camera_enabled")).toBool(imported.cameraEnabled)
        : behavior.value(QStringLiteral("camera_enabled")).toBool(imported.cameraEnabled);
    imported.cameraConsentGranted = vision.contains(QStringLiteral("camera_consent_granted"))
        ? vision.value(QStringLiteral("camera_consent_granted")).toBool(imported.cameraConsentGranted)
        : behavior.value(QStringLiteral("camera_consent_granted")).toBool(imported.cameraConsentGranted);
    imported.cameraIndex = qMax(
        0,
        vision.contains(QStringLiteral("camera_index"))
            ? vision.value(QStringLiteral("camera_index")).toInt(imported.cameraIndex)
            : behavior.value(QStringLiteral("camera_index")).toInt(imported.cameraIndex));
    imported.cameraTargetFps = qBound(
        1,
        vision.contains(QStringLiteral("target_fps"))
            ? vision.value(QStringLiteral("target_fps")).toInt(imported.cameraTargetFps)
            : behavior.value(QStringLiteral("camera_target_fps")).toInt(imported.cameraTargetFps),
        30);
    imported.eyeTrackingEnabled = vision.contains(QStringLiteral("eye_tracking_enabled"))
        ? vision.value(QStringLiteral("eye_tracking_enabled")).toBool(imported.eyeTrackingEnabled)
        : behavior.value(QStringLiteral("eye_tracking_enabled")).toBool(imported.eyeTrackingEnabled);
    imported.periodicScanEnabled = vision.contains(QStringLiteral("periodic_scan_enabled"))
        ? vision.value(QStringLiteral("periodic_scan_enabled")).toBool(imported.periodicScanEnabled)
        : behavior.value(QStringLiteral("periodic_scan_enabled")).toBool(imported.periodicScanEnabled);
    imported.periodicScanIntervalMinutes = vision.contains(QStringLiteral("periodic_scan_interval_minutes"))
        ? vision.value(QStringLiteral("periodic_scan_interval_minutes")).toInt(imported.periodicScanIntervalMinutes)
        : behavior.value(QStringLiteral("periodic_scan_interval_minutes")).toInt(imported.periodicScanIntervalMinutes);

    const QJsonObject audio = root.value(QStringLiteral("audio")).toObject();
    imported.ttsProvider = normalizeNativeTtsProvider(
        audio.value(QStringLiteral("tts_provider")).toString(imported.ttsProvider),
        &imported.ttsProviderMigratedToEdge);
    imported.ttsVoice = audio.contains(QStringLiteral("tts_voice"))
        ? audio.value(QStringLiteral("tts_voice")).toString(imported.ttsVoice)
        : behavior.value(QStringLiteral("tts_voice")).toString(imported.ttsVoice);
    imported.ttsRate = audio.contains(QStringLiteral("tts_rate"))
        ? audio.value(QStringLiteral("tts_rate")).toString(imported.ttsRate)
        : behavior.value(QStringLiteral("tts_rate")).toString(imported.ttsRate);
    imported.microphoneEnabled = audio.contains(QStringLiteral("microphone_enabled"))
        ? audio.value(QStringLiteral("microphone_enabled")).toBool(imported.microphoneEnabled)
        : behavior.value(QStringLiteral("microphone_enabled")).toBool(imported.microphoneEnabled);
    imported.voiceInputMode = audio.contains(QStringLiteral("voice_input_mode"))
        ? audio.value(QStringLiteral("voice_input_mode")).toString(imported.voiceInputMode)
        : behavior.value(QStringLiteral("voice_input_mode")).toString(imported.voiceInputMode);
    imported.asrProvider = audio.contains(QStringLiteral("asr_provider"))
        ? audio.value(QStringLiteral("asr_provider")).toString(imported.asrProvider)
        : behavior.value(QStringLiteral("asr_provider")).toString(imported.asrProvider);
    imported.asrProvider = normalizeNativeAsrProvider(imported.asrProvider, &imported.asrProviderMigratedFromGoogle);
    imported.asrApiKey = audio.contains(QStringLiteral("asr_api_key"))
        ? audio.value(QStringLiteral("asr_api_key")).toString(imported.asrApiKey)
        : behavior.value(QStringLiteral("asr_api_key")).toString(imported.asrApiKey);
    imported.asrModel = audio.contains(QStringLiteral("asr_model"))
        ? audio.value(QStringLiteral("asr_model")).toString(imported.asrModel)
        : behavior.value(QStringLiteral("asr_model")).toString(imported.asrModel);
    imported.asrBaseUrl = audio.contains(QStringLiteral("asr_base_url"))
        ? audio.value(QStringLiteral("asr_base_url")).toString(imported.asrBaseUrl)
        : behavior.value(QStringLiteral("asr_base_url")).toString(imported.asrBaseUrl);
    imported.asrTemperature = audio.contains(QStringLiteral("asr_temperature"))
        ? audio.value(QStringLiteral("asr_temperature")).toDouble(imported.asrTemperature)
        : behavior.value(QStringLiteral("asr_temperature")).toDouble(imported.asrTemperature);
    imported.asrPrompt = audio.contains(QStringLiteral("asr_prompt"))
        ? audio.value(QStringLiteral("asr_prompt")).toString(imported.asrPrompt)
        : behavior.value(QStringLiteral("asr_prompt")).toString(imported.asrPrompt);

    const QJsonObject wakeup = root.value(QStringLiteral("wakeup")).toObject();
    imported.wakeupEnabled = wakeup.contains(QStringLiteral("enabled"))
        ? wakeup.value(QStringLiteral("enabled")).toBool(imported.wakeupEnabled)
        : behavior.value(QStringLiteral("wakeup_enabled")).toBool(imported.wakeupEnabled);
    imported.wakeupLanguage = wakeup.contains(QStringLiteral("language"))
        ? wakeup.value(QStringLiteral("language")).toString(imported.wakeupLanguage)
        : behavior.value(QStringLiteral("wakeup_language")).toString(imported.wakeupLanguage);
    const QJsonArray wakeupPhrases = wakeup.contains(QStringLiteral("phrases"))
        ? wakeup.value(QStringLiteral("phrases")).toArray()
        : behavior.value(QStringLiteral("wakeup_phrases")).toArray();
    if (!wakeupPhrases.isEmpty()) {
        QStringList parsedPhrases;
        parsedPhrases.reserve(wakeupPhrases.size());
        for (const QJsonValue &value : wakeupPhrases) {
            const QString phrase = value.toString().trimmed();
            if (!phrase.isEmpty()) {
                parsedPhrases.push_back(phrase);
            }
        }
        if (!parsedPhrases.isEmpty()) {
            imported.wakeupPhrases = parsedPhrases;
        }
    }

    const QJsonObject audioMonitor = behavior.value(QStringLiteral("audio_output_monitor")).toObject();
    imported.audioOutputPollIntervalMs = audioMonitor.value(QStringLiteral("poll_interval_ms")).toInt(imported.audioOutputPollIntervalMs);
    imported.audioMonitorIgnoreCurrentProcessAudio = audioMonitor.value(QStringLiteral("ignore_current_process_audio")).toBool(imported.audioMonitorIgnoreCurrentProcessAudio);
    imported.audioMonitorPreferMediaSessions = audioMonitor.value(QStringLiteral("prefer_media_sessions")).toBool(imported.audioMonitorPreferMediaSessions);
    imported.audioMonitorIncludeMasterPeakFallback = audioMonitor.value(QStringLiteral("include_master_peak_fallback")).toBool(imported.audioMonitorIncludeMasterPeakFallback);

    const QJsonObject trigger = root.value(QStringLiteral("trigger")).toObject();
    imported.idleThresholdSeconds = trigger.value(QStringLiteral("idle_threshold_seconds")).toInt(imported.idleThresholdSeconds);
    const QJsonArray jitterRange = trigger.value(QStringLiteral("jitter_range_seconds")).toArray();
    if (jitterRange.size() == 2) {
        imported.idleJitterMinSeconds = jitterRange.at(0).toInt(imported.idleJitterMinSeconds);
        imported.idleJitterMaxSeconds = jitterRange.at(1).toInt(imported.idleJitterMaxSeconds);
    }
    imported.autoDismissSeconds = trigger.value(QStringLiteral("auto_dismiss_seconds")).toInt(imported.autoDismissSeconds);

    const QJsonObject idleInvasion = root.value(QStringLiteral("idle_invasion")).toObject();
    imported.idleInvasion.enabled = idleInvasion.value(QStringLiteral("enabled")).toBool(imported.idleInvasion.enabled);
    imported.idleInvasion.startDelayMs = idleInvasion.value(QStringLiteral("start_delay_ms")).toInt(imported.idleInvasion.startDelayMs);
    imported.idleInvasion.initialSpawnIntervalMs = idleInvasion.value(QStringLiteral("initial_spawn_interval_ms")).toInt(imported.idleInvasion.initialSpawnIntervalMs);
    imported.idleInvasion.minSpawnIntervalMs = idleInvasion.value(QStringLiteral("min_spawn_interval_ms")).toInt(imported.idleInvasion.minSpawnIntervalMs);
    imported.idleInvasion.maxInvaders = idleInvasion.value(QStringLiteral("max_invaders")).toInt(imported.idleInvasion.maxInvaders);
    imported.idleInvasion.scale = idleInvasion.value(QStringLiteral("scale")).toDouble(imported.idleInvasion.scale);
    imported.idleInvasion.cellPadding = idleInvasion.value(QStringLiteral("cell_padding")).toInt(imported.idleInvasion.cellPadding);
    imported.idleInvasion.retreatStyle = idleInvasion.value(QStringLiteral("retreat_style")).toString(imported.idleInvasion.retreatStyle);
    const QJsonArray invasionGifs = idleInvasion.value(QStringLiteral("participating_gifs")).toArray();
    if (!invasionGifs.isEmpty()) {
        QStringList parsedGifs;
        parsedGifs.reserve(invasionGifs.size());
        for (const QJsonValue &value : invasionGifs) {
            const QString gif = value.toString().trimmed();
            if (!gif.isEmpty()) {
                parsedGifs.push_back(gif);
            }
        }
        if (!parsedGifs.isEmpty()) {
            imported.idleInvasion.participatingGifs = parsedGifs;
        }
    }

    const QJsonObject window = root.value(QStringLiteral("window")).toObject();
    imported.lastVisible = window.value(QStringLiteral("visible")).toBool(imported.lastVisible);
    imported.windowX = window.value(QStringLiteral("x")).toInt(imported.windowX);
    imported.windowY = window.value(QStringLiteral("y")).toInt(imported.windowY);

    imported.ttsProvider = audio.value(QStringLiteral("tts_provider")).toString(imported.ttsProvider);
    imported.audioVolume = audio.value(QStringLiteral("volume")).toDouble(imported.audioVolume);
    imported.audioCacheEnabled = audio.value(QStringLiteral("cache_enabled")).toBool(imported.audioCacheEnabled);

    const QJsonObject llm = root.value(QStringLiteral("llm")).toObject();
    imported.llmProvider = llm.value(QStringLiteral("provider")).toString(imported.llmProvider);
    imported.llmModel = llm.value(QStringLiteral("model")).toString(imported.llmModel);
    imported.llmApiKey = llm.value(QStringLiteral("api_key")).toString(imported.llmApiKey);
    imported.llmBaseUrl = llm.value(QStringLiteral("base_url")).toString(imported.llmBaseUrl);
    imported.commentarySystemPrompt = llm.value(QStringLiteral("commentary_system_prompt")).toString(imported.commentarySystemPrompt);
    imported.commentaryUserPrompt = llm.value(QStringLiteral("commentary_user_prompt")).toString(imported.commentaryUserPrompt);
    imported.commentaryNoImagePrompt = llm.value(QStringLiteral("commentary_no_image_prompt")).toString(imported.commentaryNoImagePrompt);
    imported.commentaryMaxTokens = llm.value(QStringLiteral("commentary_max_tokens")).toInt(imported.commentaryMaxTokens);
    imported.commentaryTemperature = llm.value(QStringLiteral("commentary_temperature")).toDouble(imported.commentaryTemperature);

    const QJsonObject screenCommentary = root.value(QStringLiteral("screen_commentary")).toObject();
    imported.commentaryStreamingEnabled = screenCommentary.value(QStringLiteral("streaming_enabled")).toBool(imported.commentaryStreamingEnabled);
    imported.commentaryOcrFallbackEnabled = screenCommentary.value(QStringLiteral("ocr_fallback_enabled")).toBool(imported.commentaryOcrFallbackEnabled);
    imported.commentaryStreamChunkChars = screenCommentary.value(QStringLiteral("stream_chunk_chars")).toInt(imported.commentaryStreamChunkChars);
    imported.commentaryMaxResponseChars = screenCommentary.value(QStringLiteral("max_response_chars")).toInt(imported.commentaryMaxResponseChars);
    imported.commentaryPreambleText = screenCommentary.value(QStringLiteral("preamble_text")).toString(imported.commentaryPreambleText);
    imported.screenCommentaryAutoEnabled = screenCommentary.value(QStringLiteral("auto_enabled")).toBool(imported.screenCommentaryAutoEnabled);
    imported.screenCommentaryAutoIntervalMinutes = screenCommentary.value(QStringLiteral("auto_interval_minutes")).toInt(imported.screenCommentaryAutoIntervalMinutes);

    return save(imported);
}
