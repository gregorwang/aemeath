from __future__ import annotations

from dataclasses import replace

from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QLabel,
    QLineEdit,
    QSpinBox,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

try:
    from core.config_manager import AppConfig
    from core.paths import get_log_file
except ModuleNotFoundError:
    from ..core.config_manager import AppConfig
    from ..core.paths import get_log_file


class SettingsDialog(QDialog):
    """Runtime settings editor."""
    MODEL_PRESETS = [
        "grok-4-fast-reasoning",
        "grok-4-fast",
        "grok-3-mini-fast",
        "grok-2-mini-transcribe",
        "deepseek-chat",
        "gpt-5.1",
        "gpt-5.1-mini",
        "gpt-5",
        "gpt-5-mini",
        "gpt-4.1",
        "gpt-4o",
    ]
    ENDPOINT_PRESETS = {
        "OpenAI 官方": ("openai", "https://api.openai.com/v1", "gpt-5-mini"),
        "xAI 官方(推荐看图)": ("xai", "https://api.x.ai/v1", "grok-4-fast-reasoning"),
        "DeepSeek 官方(当前文本为主)": ("deepseek", "https://api.deepseek.com/v1", "deepseek-chat"),
    }

    def __init__(self, config: AppConfig, parent=None):
        super().__init__(parent)
        self._source = config
        self.setWindowTitle("Cyber Companion 设置")
        self.setMinimumWidth(560)
        self._build_ui()
        self._load_from_config(config)

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)

        tabs = QTabWidget(self)
        root.addWidget(tabs)

        general_tab = QWidget(self)
        ai_tab = QWidget(self)
        voice_tab = QWidget(self)

        tabs.addTab(general_tab, "基础")
        tabs.addTab(ai_tab, "AI")
        tabs.addTab(voice_tab, "语音与视觉")

        general_form = QFormLayout(general_tab)
        self.position_combo = QComboBox(self)
        self.position_combo.addItems(["auto", "left", "right"])
        self.fullscreen_pause_checkbox = QCheckBox("全屏应用时暂停", self)
        self.audio_output_reactive_checkbox = QCheckBox("系统音频驱动状态/动效 (MEDIA_PLAYING)", self)
        self.debug_mode_checkbox = QCheckBox("调试模式", self)
        self.offline_mode_checkbox = QCheckBox("离线模式（禁用远程 AI）", self)
        self.log_path_edit = QLineEdit(self)
        self.log_path_edit.setReadOnly(True)
        self.log_path_edit.setText(str(get_log_file()))
        self.idle_threshold_spin = QSpinBox(self)
        self.idle_threshold_spin.setRange(30, 1800)
        self.idle_threshold_spin.setSuffix(" 秒")
        self.auto_dismiss_spin = QSpinBox(self)
        self.auto_dismiss_spin.setRange(5, 600)
        self.auto_dismiss_spin.setSuffix(" 秒")
        general_form.addRow("出场位置", self.position_combo)
        general_form.addRow("空闲触发阈值", self.idle_threshold_spin)
        general_form.addRow("自动消失时间", self.auto_dismiss_spin)
        general_form.addRow("", self.fullscreen_pause_checkbox)
        general_form.addRow("", self.audio_output_reactive_checkbox)
        general_form.addRow("", self.debug_mode_checkbox)
        general_form.addRow("", self.offline_mode_checkbox)
        general_form.addRow("日志文件", self.log_path_edit)

        ai_form = QFormLayout(ai_tab)
        self.provider_combo = QComboBox(self)
        self.provider_combo.addItems(["none", "openai", "xai", "deepseek"])
        self.endpoint_preset_combo = QComboBox(self)
        self.endpoint_preset_combo.addItems(["自定义(不改)", *self.ENDPOINT_PRESETS.keys()])
        self.endpoint_preset_combo.currentTextChanged.connect(self._on_endpoint_preset_changed)
        self.model_combo = QComboBox(self)
        self.model_combo.setEditable(True)
        self.model_combo.addItems(self.MODEL_PRESETS)
        self.model_combo.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
        self.base_url_edit = QLineEdit(self)
        self.base_url_edit.setPlaceholderText("例如 https://api.x.ai/v1 或 https://api.openai.com/v1")
        self.api_key_edit = QLineEdit(self)
        self.api_key_edit.setEchoMode(QLineEdit.EchoMode.Password)
        self.screen_streaming_checkbox = QCheckBox("屏幕解读使用流式输出", self)
        self.screen_chunk_chars_spin = QSpinBox(self)
        self.screen_chunk_chars_spin.setRange(8, 80)
        self.screen_max_chars_spin = QSpinBox(self)
        self.screen_max_chars_spin.setRange(20, 300)
        self.screen_preamble_edit = QLineEdit(self)
        self.screen_preamble_edit.setPlaceholderText("例如：正在看你的屏幕内容，让我看看你在做什么。")
        ai_form.addRow("LLM 提供商", self.provider_combo)
        ai_form.addRow("官方端点预设", self.endpoint_preset_combo)
        ai_form.addRow("模型", self.model_combo)
        ai_form.addRow("API Base URL", self.base_url_edit)
        ai_form.addRow("API Key", self.api_key_edit)
        ai_form.addRow("", self.screen_streaming_checkbox)
        ai_form.addRow("流式分段字数", self.screen_chunk_chars_spin)
        ai_form.addRow("单次最大回复字数", self.screen_max_chars_spin)
        ai_form.addRow("屏幕解读过渡语", self.screen_preamble_edit)

        voice_layout = QVBoxLayout(voice_tab)
        vision_box = QGroupBox("视觉 (CV)", voice_tab)
        vision_form = QFormLayout(vision_box)
        self.camera_enabled_checkbox = QCheckBox("启用摄像头", self)
        self.eye_tracking_checkbox = QCheckBox("启用视线跟踪", self)
        self.camera_index_spin = QSpinBox(self)
        self.camera_index_spin.setRange(0, 8)
        self.camera_index_spin.setToolTip("默认填 0（系统默认摄像头），有多个摄像头时可试 1、2。")
        self.target_fps_spin = QSpinBox(self)
        self.target_fps_spin.setRange(1, 30)
        self.target_fps_spin.setToolTip("每秒处理帧数。越高越流畅，但占用更高。")
        vision_form.addRow("", self.camera_enabled_checkbox)
        vision_form.addRow("", self.eye_tracking_checkbox)
        vision_form.addRow("摄像头设备编号(0=默认)", self.camera_index_spin)
        vision_form.addRow("视觉采样帧率(FPS)", self.target_fps_spin)

        audio_box = QGroupBox("语音 (TTS + 唤醒)", voice_tab)
        audio_form = QFormLayout(audio_box)
        self.tts_voice_edit = QLineEdit(self)
        self.tts_rate_edit = QLineEdit(self)
        self.volume_spin = QDoubleSpinBox(self)
        self.volume_spin.setRange(0.0, 1.0)
        self.volume_spin.setSingleStep(0.05)
        self.volume_spin.setDecimals(2)
        self.cache_enabled_checkbox = QCheckBox("TTS 音频缓存", self)
        self.mic_enabled_checkbox = QCheckBox("启用麦克风监听", self)
        self.wakeup_enabled_checkbox = QCheckBox("启用语音唤醒词", self)
        self.voice_input_mode_combo = QComboBox(self)
        self.voice_input_mode_combo.addItem("continuous（后台连续唤醒）", "continuous")
        self.voice_input_mode_combo.addItem("push_to_talk（按全局 B 键单次转写）", "push_to_talk")
        self.wakeup_phrases_edit = QLineEdit(self)
        self.wakeup_phrases_edit.setPlaceholderText("多个唤醒词请用英文逗号分隔")
        self.wakeup_language_edit = QLineEdit(self)
        self.wakeup_language_edit.setPlaceholderText("例如 zh-CN")
        self.asr_provider_combo = QComboBox(self)
        self.asr_provider_combo.addItems(["zhipu_asr", "xai_realtime", "google", "openai_whisper"])
        self.asr_provider_combo.currentTextChanged.connect(self._on_asr_provider_changed)
        self.asr_api_key_edit = QLineEdit(self)
        self.asr_api_key_edit.setEchoMode(QLineEdit.EchoMode.Password)
        self.asr_model_edit = QLineEdit(self)
        self.asr_model_edit.setPlaceholderText("zhipu_asr: glm-asr-2512")
        self.asr_base_url_edit = QLineEdit(self)
        self.asr_base_url_edit.setPlaceholderText("zhipu_asr 建议 https://open.bigmodel.cn/api/paas/v4/audio/transcriptions")
        self.asr_temperature_spin = QDoubleSpinBox(self)
        self.asr_temperature_spin.setRange(0.0, 1.0)
        self.asr_temperature_spin.setSingleStep(0.1)
        self.asr_temperature_spin.setDecimals(2)
        self.asr_prompt_edit = QLineEdit(self)
        self.asr_prompt_edit.setPlaceholderText("可选：用于提示词上下文")
        audio_form.addRow("TTS 提供商", QLabel("edge (固定)"))
        audio_form.addRow("TTS 语音", self.tts_voice_edit)
        audio_form.addRow("TTS 语速", self.tts_rate_edit)
        audio_form.addRow("音量", self.volume_spin)
        audio_form.addRow("", self.cache_enabled_checkbox)
        audio_form.addRow("", self.mic_enabled_checkbox)
        audio_form.addRow("语音输入模式", self.voice_input_mode_combo)
        audio_form.addRow("", self.wakeup_enabled_checkbox)
        audio_form.addRow("唤醒词", self.wakeup_phrases_edit)
        audio_form.addRow("识别语言", self.wakeup_language_edit)
        audio_form.addRow("语音识别提供商", self.asr_provider_combo)
        audio_form.addRow("ASR API Key", self.asr_api_key_edit)
        audio_form.addRow("ASR 模型", self.asr_model_edit)
        audio_form.addRow("ASR Base URL", self.asr_base_url_edit)
        audio_form.addRow("ASR 温度", self.asr_temperature_spin)
        audio_form.addRow("ASR Prompt", self.asr_prompt_edit)

        voice_layout.addWidget(vision_box)
        voice_layout.addWidget(audio_box)
        voice_layout.addWidget(
            QLabel(
                "提示: 麦克风无权限或依赖缺失会自动降级。"
                "\n提示: 摄像头不是常亮，只有角色出现/互动时才会短时启用。"
                "\n提示: 屏幕识别失败可在托盘菜单点“打开日志目录”，日志文件名为 app.log。"
                "\n提示: push_to_talk 模式下可按全局 B 键进行单次语音转写。"
                "\n提示: 推荐 ASR=zhipu_asr（云端）或 xai_realtime。",
                voice_tab,
            )
        )
        voice_layout.addStretch(1)

        button_box = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel,
            parent=self,
        )
        button_box.accepted.connect(self.accept)
        button_box.rejected.connect(self.reject)
        root.addWidget(button_box)

    def _load_from_config(self, config: AppConfig) -> None:
        self._set_combo_text(self.position_combo, config.appearance.position, "auto")
        self.idle_threshold_spin.setValue(max(30, int(config.trigger.idle_threshold_seconds)))
        self.auto_dismiss_spin.setValue(max(5, int(config.trigger.auto_dismiss_seconds)))
        self.fullscreen_pause_checkbox.setChecked(bool(config.behavior.full_screen_pause))
        self.audio_output_reactive_checkbox.setChecked(bool(config.behavior.audio_output_reactive))
        self.debug_mode_checkbox.setChecked(bool(config.behavior.debug_mode))
        self.offline_mode_checkbox.setChecked(bool(config.behavior.offline_mode))

        self._set_combo_text(self.provider_combo, config.llm.provider, "xai")
        self._set_combo_text(self.model_combo, config.llm.model, "grok-4-fast-reasoning")
        self.endpoint_preset_combo.setCurrentIndex(0)
        self.base_url_edit.setText(config.llm.base_url)
        self.api_key_edit.setText(config.llm.api_key)
        self.screen_streaming_checkbox.setChecked(bool(config.screen_commentary.streaming_enabled))
        self.screen_chunk_chars_spin.setValue(max(8, min(80, int(config.screen_commentary.stream_chunk_chars))))
        self.screen_max_chars_spin.setValue(max(20, min(300, int(config.screen_commentary.max_response_chars))))
        self.screen_preamble_edit.setText(config.screen_commentary.preamble_text)

        self.camera_enabled_checkbox.setChecked(bool(config.vision.camera_enabled))
        self.eye_tracking_checkbox.setChecked(bool(config.vision.eye_tracking_enabled))
        self.camera_index_spin.setValue(max(0, int(config.vision.camera_index)))
        self.target_fps_spin.setValue(max(1, min(30, int(config.vision.target_fps))))

        self.tts_voice_edit.setText(config.audio.tts_voice)
        self.tts_rate_edit.setText(config.audio.tts_rate)
        self.volume_spin.setValue(float(config.audio.volume))
        self.cache_enabled_checkbox.setChecked(bool(config.audio.cache_enabled))
        self.mic_enabled_checkbox.setChecked(bool(config.audio.microphone_enabled))
        self._set_combo_data(self.voice_input_mode_combo, config.audio.voice_input_mode, "push_to_talk")
        self.wakeup_enabled_checkbox.setChecked(bool(config.wakeup.enabled))
        self.wakeup_phrases_edit.setText(", ".join(config.wakeup.phrases))
        self.wakeup_language_edit.setText(config.wakeup.language)
        self._set_combo_text(self.asr_provider_combo, config.audio.asr_provider, "zhipu_asr")
        self._on_asr_provider_changed(self.asr_provider_combo.currentText())
        self.asr_api_key_edit.setText(config.audio.asr_api_key)
        self.asr_model_edit.setText(config.audio.asr_model)
        self.asr_base_url_edit.setText(config.audio.asr_base_url)
        self.asr_temperature_spin.setValue(float(config.audio.asr_temperature))
        self.asr_prompt_edit.setText(config.audio.asr_prompt)

    def to_config(self) -> AppConfig:
        appearance = replace(
            self._source.appearance,
            position=self.position_combo.currentText().strip() or "auto",
        )
        trigger = replace(
            self._source.trigger,
            idle_threshold_seconds=max(30, int(self.idle_threshold_spin.value())),
            auto_dismiss_seconds=max(5, int(self.auto_dismiss_spin.value())),
        )
        behavior = replace(
            self._source.behavior,
            full_screen_pause=self.fullscreen_pause_checkbox.isChecked(),
            audio_output_reactive=self.audio_output_reactive_checkbox.isChecked(),
            debug_mode=self.debug_mode_checkbox.isChecked(),
            offline_mode=self.offline_mode_checkbox.isChecked(),
        )
        llm = replace(
            self._source.llm,
            provider=self.provider_combo.currentText().strip().lower() or "none",
            model=self.model_combo.currentText().strip() or self._source.llm.model,
            base_url=self.base_url_edit.text().strip() or self._source.llm.base_url,
            api_key=self.api_key_edit.text().strip(),
        )
        vision = replace(
            self._source.vision,
            camera_enabled=self.camera_enabled_checkbox.isChecked(),
            eye_tracking_enabled=self.eye_tracking_checkbox.isChecked(),
            camera_index=int(self.camera_index_spin.value()),
            target_fps=int(self.target_fps_spin.value()),
        )
        audio = replace(
            self._source.audio,
            tts_provider="edge",
            tts_voice=self.tts_voice_edit.text().strip() or self._source.audio.tts_voice,
            tts_rate=self.tts_rate_edit.text().strip() or self._source.audio.tts_rate,
            volume=float(self.volume_spin.value()),
            cache_enabled=self.cache_enabled_checkbox.isChecked(),
            microphone_enabled=self.mic_enabled_checkbox.isChecked(),
            voice_input_mode=str(self.voice_input_mode_combo.currentData() or "push_to_talk"),
            asr_provider=self.asr_provider_combo.currentText().strip().lower() or "zhipu_asr",
            asr_api_key=self.asr_api_key_edit.text().strip(),
            asr_model=self.asr_model_edit.text().strip() or self._source.audio.asr_model,
            asr_base_url=self.asr_base_url_edit.text().strip() or self._source.audio.asr_base_url,
            asr_temperature=float(self.asr_temperature_spin.value()),
            asr_prompt=self.asr_prompt_edit.text().strip(),
        )

        raw_phrases = self.wakeup_phrases_edit.text().strip()
        phrases = tuple(part.strip() for part in raw_phrases.split(",") if part.strip()) if raw_phrases else self._source.wakeup.phrases
        wakeup = replace(
            self._source.wakeup,
            enabled=self.wakeup_enabled_checkbox.isChecked(),
            phrases=phrases,
            language=self.wakeup_language_edit.text().strip() or "zh-CN",
        )

        return AppConfig(
            version=self._source.version,
            trigger=trigger,
            appearance=appearance,
            audio=audio,
            behavior=behavior,
            vision=vision,
            wakeup=wakeup,
            llm=llm,
            screen_commentary=replace(
                self._source.screen_commentary,
                streaming_enabled=self.screen_streaming_checkbox.isChecked(),
                ocr_fallback_enabled=False,
                stream_chunk_chars=int(self.screen_chunk_chars_spin.value()),
                max_response_chars=int(self.screen_max_chars_spin.value()),
                preamble_text=self.screen_preamble_edit.text().strip()
                or self._source.screen_commentary.preamble_text,
            ),
        )

    @staticmethod
    def _set_combo_text(combo: QComboBox, value: str, default: str) -> None:
        preferred = (value or "").strip()
        idx = combo.findText(preferred)
        if idx >= 0:
            combo.setCurrentIndex(idx)
            return
        if combo.isEditable() and preferred:
            combo.setEditText(preferred)
            return
        fallback = combo.findText(default)
        combo.setCurrentIndex(fallback if fallback >= 0 else 0)

    @staticmethod
    def _set_combo_data(combo: QComboBox, value: str, default: str) -> None:
        preferred = (value or "").strip().lower()
        for idx in range(combo.count()):
            if str(combo.itemData(idx) or "").strip().lower() == preferred:
                combo.setCurrentIndex(idx)
                return
        for idx in range(combo.count()):
            if str(combo.itemData(idx) or "").strip().lower() == default:
                combo.setCurrentIndex(idx)
                return
        combo.setCurrentIndex(0)

    def _on_endpoint_preset_changed(self, label: str) -> None:
        preset = self.ENDPOINT_PRESETS.get((label or "").strip())
        if preset is None:
            return
        provider, base_url, model = preset
        self._set_combo_text(self.provider_combo, provider, provider)
        self.base_url_edit.setText(base_url)
        if model:
            self._set_combo_text(self.model_combo, model, model)

    def _on_asr_provider_changed(self, provider: str) -> None:
        normalized = (provider or "").strip().lower()
        if normalized == "zhipu_asr":
            self.asr_model_edit.setPlaceholderText("zhipu_asr: glm-asr-2512")
            self.asr_base_url_edit.setPlaceholderText("zhipu_asr 建议 https://open.bigmodel.cn/api/paas/v4/audio/transcriptions")
            current_model = self.asr_model_edit.text().strip().lower()
            current_base = self.asr_base_url_edit.text().strip().lower()
            if current_model in {"", "grok-2-mini-transcribe", "whisper-1"}:
                self.asr_model_edit.setText("glm-asr-2512")
            if current_base in {"", "https://api.x.ai/v1"} or "x.ai" in current_base:
                self.asr_base_url_edit.setText("https://open.bigmodel.cn/api/paas/v4/audio/transcriptions")
            return
        if normalized == "openai_whisper":
            self.asr_model_edit.setPlaceholderText("openai_whisper: whisper-1 或 gpt-4o-mini-transcribe")
            self.asr_base_url_edit.setPlaceholderText("openai_whisper 建议 https://api.openai.com/v1")
            return
        if normalized == "google":
            self.asr_model_edit.setPlaceholderText("google: 无需模型名（可留空）")
            self.asr_base_url_edit.setPlaceholderText("google: 使用 SpeechRecognition 内置 Web Speech")
            return
        self.asr_model_edit.setPlaceholderText("xai_realtime: grok-2-mini-transcribe")
        self.asr_base_url_edit.setPlaceholderText("xai_realtime 建议 https://api.x.ai/v1")
