#pragma once

#include <QDialog>

#include "runtime/app_config.h"
#include "runtime/character_manifest_catalog.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(
        const AppConfig &config,
        const QVector<CharacterManifest> &availableCharacters = {},
        QWidget *parent = nullptr);

    AppConfig editedConfig() const;

public Q_SLOTS:
    void accept() override;

private:
    void buildUi();
    void loadFromConfig(const AppConfig &config);
    void syncControlState();
    void testApiConnection();

    AppConfig m_resultConfig;
    QCheckBox *m_debugModeCheckBox = nullptr;
    QCheckBox *m_offlineModeCheckBox = nullptr;
    QCheckBox *m_startMinimizedCheckBox = nullptr;
    QCheckBox *m_autoStartOnLoginCheckBox = nullptr;
    QComboBox *m_activeCharacterComboBox = nullptr;
    QComboBox *m_preferredPositionComboBox = nullptr;
    QSpinBox *m_asciiWidthSpinBox = nullptr;
    QSpinBox *m_fontSizeSpinBox = nullptr;
    QCheckBox *m_fullScreenPauseCheckBox = nullptr;
    QCheckBox *m_residentModeCheckBox = nullptr;
    QSpinBox *m_idleThresholdSpinBox = nullptr;
    QSpinBox *m_idleJitterMinSpinBox = nullptr;
    QSpinBox *m_idleJitterMaxSpinBox = nullptr;
    QSpinBox *m_autoDismissSpinBox = nullptr;
    QCheckBox *m_cameraEnabledCheckBox = nullptr;
    QCheckBox *m_cameraConsentGrantedCheckBox = nullptr;
    QSpinBox *m_cameraIndexSpinBox = nullptr;
    QSpinBox *m_cameraTargetFpsSpinBox = nullptr;
    QCheckBox *m_eyeTrackingEnabledCheckBox = nullptr;
    QCheckBox *m_periodicScanEnabledCheckBox = nullptr;
    QSpinBox *m_periodicScanIntervalSpinBox = nullptr;
    QCheckBox *m_scriptedEntranceEnabledCheckBox = nullptr;
    QLineEdit *m_scriptedTrajectoryPathEdit = nullptr;
    QLineEdit *m_voiceScriptsPathEdit = nullptr;
    QComboBox *m_ttsProviderComboBox = nullptr;
    QLineEdit *m_ttsVoiceEdit = nullptr;
    QLineEdit *m_ttsRateEdit = nullptr;
    QDoubleSpinBox *m_audioVolumeSpinBox = nullptr;
    QCheckBox *m_audioCacheEnabledCheckBox = nullptr;
    QCheckBox *m_microphoneEnabledCheckBox = nullptr;
    QComboBox *m_voiceInputModeComboBox = nullptr;
    QCheckBox *m_wakeupEnabledCheckBox = nullptr;
    QLineEdit *m_wakeupPhrasesEdit = nullptr;
    QLineEdit *m_wakeupLanguageEdit = nullptr;
    QComboBox *m_asrProviderComboBox = nullptr;
    QLineEdit *m_asrApiKeyEdit = nullptr;
    QLineEdit *m_asrModelEdit = nullptr;
    QLineEdit *m_asrBaseUrlEdit = nullptr;
    QDoubleSpinBox *m_asrTemperatureSpinBox = nullptr;
    QLineEdit *m_asrPromptEdit = nullptr;
    QCheckBox *m_idleInvasionEnabledCheckBox = nullptr;
    QSpinBox *m_idleInvasionStartDelaySpinBox = nullptr;
    QSpinBox *m_idleInvasionInitialIntervalSpinBox = nullptr;
    QSpinBox *m_idleInvasionMinIntervalSpinBox = nullptr;
    QSpinBox *m_idleInvasionMaxInvadersSpinBox = nullptr;
    QDoubleSpinBox *m_idleInvasionScaleSpinBox = nullptr;
    QSpinBox *m_idleInvasionCellPaddingSpinBox = nullptr;
    QLineEdit *m_idleInvasionGifsEdit = nullptr;
    QComboBox *m_idleInvasionRetreatStyleComboBox = nullptr;
    QCheckBox *m_audioOutputReactiveCheckBox = nullptr;
    QSpinBox *m_audioOutputPollIntervalSpinBox = nullptr;
    QCheckBox *m_ignoreCurrentProcessCheckBox = nullptr;
    QCheckBox *m_preferMediaSessionsCheckBox = nullptr;
    QCheckBox *m_includeMasterPeakFallbackCheckBox = nullptr;
    QComboBox *m_endpointPresetComboBox = nullptr;
    QComboBox *m_providerComboBox = nullptr;
    QLineEdit *m_modelEdit = nullptr;
    QLineEdit *m_baseUrlEdit = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
    QPushButton *m_testApiButton = nullptr;
    QLineEdit *m_commentarySystemPromptEdit = nullptr;
    QLineEdit *m_commentaryUserPromptEdit = nullptr;
    QLineEdit *m_commentaryNoImagePromptEdit = nullptr;
    QSpinBox *m_commentaryMaxTokensSpinBox = nullptr;
    QDoubleSpinBox *m_commentaryTemperatureSpinBox = nullptr;
    QCheckBox *m_commentaryStreamingEnabledCheckBox = nullptr;
    QCheckBox *m_commentaryOcrFallbackEnabledCheckBox = nullptr;
    QSpinBox *m_commentaryStreamChunkCharsSpinBox = nullptr;
    QSpinBox *m_commentaryMaxResponseCharsSpinBox = nullptr;
    QLineEdit *m_commentaryPreambleTextEdit = nullptr;
    QCheckBox *m_screenCommentaryAutoEnabledCheckBox = nullptr;
    QSpinBox *m_screenCommentaryAutoIntervalSpinBox = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};
