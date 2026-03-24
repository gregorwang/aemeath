#pragma once

#include <QString>
#include <QStringList>

struct IdleInvasionConfig
{
    bool enabled = true;
    int startDelayMs = 300000;
    int initialSpawnIntervalMs = 60000;
    int minSpawnIntervalMs = 60000;
    int maxInvaders = 40;
    double scale = 0.7;
    int cellPadding = 20;
    QStringList participatingGifs = {
        QStringLiteral("state1.gif"),
        QStringLiteral("state2.gif"),
        QStringLiteral("state5.gif"),
        QStringLiteral("state6.gif"),
        QStringLiteral("aemeath.gif"),
    };
    QString retreatStyle = QStringLiteral("scatter");
};

struct AppConfig
{
    QString version = QStringLiteral("1.0.0");
    bool debugMode = true;
    bool firstRun = true;
    bool startMinimized = false;
    bool lastVisible = true;
    int windowX = -1;
    int windowY = -1;
    QString activeCharacterId = QStringLiteral("default");
    QString preferredPosition = QStringLiteral("auto");
    int appearanceAsciiWidth = 60;
    int appearanceFontSizePx = 8;
    bool offlineMode = false;
    bool fullScreenPause = true;
    bool residentMode = false;
    bool autoStartOnLogin = false;
    int idleThresholdSeconds = 180;
    int idleJitterMinSeconds = -30;
    int idleJitterMaxSeconds = 60;
    int autoDismissSeconds = 10;
    bool cameraEnabled = false;
    bool cameraConsentGranted = false;
    int cameraIndex = 0;
    int cameraTargetFps = 15;
    bool eyeTrackingEnabled = true;
    bool periodicScanEnabled = true;
    int periodicScanIntervalMinutes = 30;
    bool scriptedEntranceEnabled = false;
    bool audioOutputReactive = true;
    int audioOutputPollIntervalMs = 500;
    bool audioMonitorIgnoreCurrentProcessAudio = true;
    bool audioMonitorPreferMediaSessions = true;
    bool audioMonitorIncludeMasterPeakFallback = false;
    QString voiceScriptsPath;
    QString ttsProvider = QStringLiteral("edge");
    bool ttsProviderMigratedToEdge = false;
    QString ttsVoice = QStringLiteral("zh-CN-XiaoxiaoNeural");
    QString ttsRate = QStringLiteral("+0%");
    double audioVolume = 0.8;
    bool audioCacheEnabled = true;
    bool microphoneEnabled = false;
    QString voiceInputMode = QStringLiteral("push_to_talk");
    QString asrProvider = QStringLiteral("zhipu_asr");
    bool asrProviderMigratedFromGoogle = false;
    QString asrApiKey;
    QString asrModel = QStringLiteral("glm-asr-2512");
    QString asrBaseUrl = QStringLiteral("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions");
    double asrTemperature = 0.0;
    QString asrPrompt;
    bool wakeupEnabled = false;
    QStringList wakeupPhrases = {
        QStringLiteral("小爱同学请你出来"),
        QStringLiteral("小爱同学出来"),
        QStringLiteral("小爱同学"),
    };
    QString wakeupLanguage = QStringLiteral("zh-CN");
    QString llmProvider = QStringLiteral("openai");
    QString llmModel = QStringLiteral("gpt-5-mini");
    QString llmApiKey;
    QString llmBaseUrl = QStringLiteral("https://api.openai.com/v1");
    QString commentarySystemPrompt = QStringLiteral("你是一个中文桌面伴侣，请先说结论，再补一句细节，总长度不超过30字。");
    QString commentaryUserPrompt = QStringLiteral("看看我的当前屏幕，用一句短句告诉我你看到了什么。");
    QString commentaryNoImagePrompt = QStringLiteral("当前抓屏不可用。请给一句不超过30字的状态播报。");
    int commentaryMaxTokens = 60;
    double commentaryTemperature = 0.7;
    bool commentaryStreamingEnabled = true;
    bool commentaryOcrFallbackEnabled = false;
    int commentaryStreamChunkChars = 22;
    int commentaryMaxResponseChars = 90;
    QString commentaryPreambleText = QStringLiteral("正在看你的屏幕内容，让我看看你在做什么。");
    bool screenCommentaryAutoEnabled = false;
    int screenCommentaryAutoIntervalMinutes = 60;
    QString scriptedTrajectoryPath;
    IdleInvasionConfig idleInvasion;
};

