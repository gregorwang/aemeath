from __future__ import annotations

from dataclasses import replace
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import Request, urlopen

from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPushButton,
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

    def __init__(self, config: AppConfig, parent: QWidget | None = None):
        super().__init__(parent)
        self._source = config
        self._defaults = AppConfig(version=config.version)
        self.setWindowTitle("Cyber Companion 设置")
        self.setMinimumWidth(560)
        self._build_ui()
        self._load_from_config(config)

    def _build_ui(self) -> None:
        """Build the dialog widgets and connect non-data initialization hooks."""
        root = QVBoxLayout(self)

        tabs = QTabWidget(self)
        tabs.addTab(self._create_general_tab(), "基础")
        tabs.addTab(self._create_ai_tab(), "AI")
        tabs.addTab(self._create_voice_tab(), "语音与视觉")
        root.addWidget(tabs)

        footer = QHBoxLayout()
        self.restore_defaults_button = QPushButton("恢复默认值", self)
        self.restore_defaults_button.clicked.connect(self._restore_defaults)
        footer.addWidget(self.restore_defaults_button)
        footer.addStretch(1)
        footer.addWidget(self._create_button_box())
        root.addLayout(footer)

        self._setup_tooltips()
        self._setup_control_dependencies()

    def _create_general_tab(self) -> QWidget:
        """Create and return the General settings tab."""
        general_tab = QWidget(self)
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
        return general_tab

    def _create_ai_tab(self) -> QWidget:
        """Create and return the AI settings tab."""
        ai_tab = QWidget(self)
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
        self._connect_url_validation(self.base_url_edit)
        self.api_key_edit = self._create_password_line_edit()
        self.screen_streaming_checkbox = QCheckBox("屏幕解读使用流式输出", self)
        self.screen_chunk_chars_spin = QSpinBox(self)
        self.screen_chunk_chars_spin.setRange(8, 80)
        self.screen_max_chars_spin = QSpinBox(self)
        self.screen_max_chars_spin.setRange(20, 300)
        self.screen_preamble_edit = QLineEdit(self)
        self.screen_preamble_edit.setPlaceholderText("例如：正在看你的屏幕内容，让我看看你在做什么。")
        self.screen_auto_commentary_checkbox = QCheckBox("启用定时自动屏幕解读", self)
        self.screen_auto_commentary_checkbox.toggled.connect(self._on_screen_auto_toggled)
        self.screen_auto_interval_spin = QSpinBox(self)
        self.screen_auto_interval_spin.setRange(1, 240)
        self.screen_auto_interval_spin.setSuffix(" 分钟")
        self.test_api_button = QPushButton("测试API连接", self)
        self.test_api_button.clicked.connect(self._test_api_connection)
        ai_form.addRow("LLM 提供商", self.provider_combo)
        ai_form.addRow("官方端点预设", self.endpoint_preset_combo)
        ai_form.addRow("模型", self.model_combo)
        ai_form.addRow("API Base URL", self.base_url_edit)
        ai_form.addRow("API Key", self.api_key_edit)
        ai_form.addRow("", self.screen_streaming_checkbox)
        ai_form.addRow("流式分段字数", self.screen_chunk_chars_spin)
        ai_form.addRow("单次最大回复字数", self.screen_max_chars_spin)
        ai_form.addRow("屏幕解读过渡语", self.screen_preamble_edit)
        ai_form.addRow("", self.screen_auto_commentary_checkbox)
        ai_form.addRow("自动解读间隔", self.screen_auto_interval_spin)
        ai_form.addRow("", self.test_api_button)

        self._ai_controls = [
            self.provider_combo,
            self.endpoint_preset_combo,
            self.model_combo,
            self.base_url_edit,
            self.api_key_edit,
            self.screen_streaming_checkbox,
            self.screen_chunk_chars_spin,
            self.screen_max_chars_spin,
            self.screen_preamble_edit,
            self.screen_auto_commentary_checkbox,
            self.screen_auto_interval_spin,
            self.test_api_button,
        ]
        return ai_tab

    def _create_voice_tab(self) -> QWidget:
        """Create and return the Voice/Vision settings tab."""
        voice_tab = QWidget(self)
        voice_layout = QVBoxLayout(voice_tab)
        vision_box = QGroupBox("视觉 (CV)", voice_tab)
        vision_form = QFormLayout(vision_box)
        self.camera_enabled_checkbox = QCheckBox("启用摄像头", self)
        self.eye_tracking_checkbox = QCheckBox("启用视线跟踪", self)
        self.camera_index_spin = QSpinBox(self)
        self.camera_index_spin.setRange(0, 8)
        self.target_fps_spin = QSpinBox(self)
        self.target_fps_spin.setRange(1, 30)
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
        self.asr_api_key_edit = self._create_password_line_edit()
        self.asr_model_edit = QLineEdit(self)
        self.asr_model_edit.setPlaceholderText("zhipu_asr: glm-asr-2512")
        self.asr_base_url_edit = QLineEdit(self)
        self.asr_base_url_edit.setPlaceholderText("zhipu_asr 建议 https://open.bigmodel.cn/api/paas/v4/audio/transcriptions")
        self._connect_url_validation(self.asr_base_url_edit)
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
        self._asr_controls = [
            self.voice_input_mode_combo,
            self.wakeup_enabled_checkbox,
            self.wakeup_phrases_edit,
            self.wakeup_language_edit,
            self.asr_provider_combo,
            self.asr_api_key_edit,
            self.asr_model_edit,
            self.asr_base_url_edit,
            self.asr_temperature_spin,
            self.asr_prompt_edit,
        ]
        return voice_tab

    def _create_button_box(self) -> QDialogButtonBox:
        """Create the OK/Cancel button box."""
        button_box = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel,
            parent=self,
        )
        button_box.accepted.connect(self.accept)
        button_box.rejected.connect(self.reject)
        return button_box

    def _create_password_line_edit(self) -> QLineEdit:
        """Create a password-style line edit."""
        line_edit = QLineEdit(self)
        line_edit.setEchoMode(QLineEdit.EchoMode.Password)
        return line_edit

    def _connect_url_validation(self, line_edit: QLineEdit) -> None:
        """Bind URL validation callback to a line edit."""
        line_edit.textChanged.connect(lambda text, widget=line_edit: self._validate_url(widget, text))
        self._validate_url(line_edit, line_edit.text())

    def _setup_tooltips(self) -> None:
        """Attach practical tooltips to major controls."""
        self.position_combo.setToolTip("角色窗口停靠位置。auto 为自动避让，left/right 固定在屏幕边缘。")
        self.idle_threshold_spin.setToolTip("用户无输入达到该秒数后，触发空闲互动。")
        self.auto_dismiss_spin.setToolTip("角色出现后若无进一步互动，多少秒后自动隐藏。")
        self.offline_mode_checkbox.setToolTip("开启后禁用 AI 页全部远程能力，适合纯本地运行或排障。")
        self.provider_combo.setToolTip(
            "LLM 提供商选择:\n"
            "- none: 不调用远程模型\n"
            "- openai: OpenAI 官方接口\n"
            "- xai: xAI 官方接口，视觉场景常用\n"
            "- deepseek: DeepSeek 官方接口，文本场景常用"
        )
        self.endpoint_preset_combo.setToolTip("快速填充 provider/base_url/model 组合；“自定义(不改)”不覆盖现有输入。")
        self.model_combo.setToolTip("模型 ID。可从预设中选，也可手动输入服务端支持的模型名。")
        self.base_url_edit.setToolTip("LLM API 根地址，仅接受 http/https。格式错误会显示红框。")
        self.api_key_edit.setToolTip("LLM API 密钥，不保存到日志。留空将导致远程调用失败。")
        self.screen_streaming_checkbox.setToolTip("屏幕解读时分段输出，响应更快。")
        self.screen_chunk_chars_spin.setToolTip("流式模式每段字符数，越小更新越频繁。")
        self.screen_max_chars_spin.setToolTip("单次屏幕解读文本上限，控制输出长度与延迟。")
        self.screen_preamble_edit.setToolTip("屏幕解读前的固定过渡语。")
        self.screen_auto_commentary_checkbox.setToolTip("按固定周期自动触发一次屏幕解读。")
        self.screen_auto_interval_spin.setToolTip("自动屏幕解读触发间隔（分钟）。")
        self.test_api_button.setToolTip("基于当前 provider/base_url/api_key 发起一次轻量连通性测试。")
        self.camera_enabled_checkbox.setToolTip("开启后允许程序按需访问摄像头。")
        self.camera_index_spin.setToolTip("默认填 0（系统默认摄像头），有多个摄像头时可试 1、2。")
        self.target_fps_spin.setToolTip("每秒处理帧数。越高越流畅，但占用更高。")
        self.mic_enabled_checkbox.setToolTip("语音输入总开关；关闭后将禁用 ASR 相关配置。")
        self.voice_input_mode_combo.setToolTip(
            "语音输入模式:\n"
            "- continuous: 后台持续监听，支持唤醒词\n"
            "- push_to_talk: 按全局 B 键触发一次转写，资源占用更低"
        )
        self.wakeup_enabled_checkbox.setToolTip("仅在 continuous 模式下有意义，用于语音唤醒角色。")
        self.wakeup_phrases_edit.setToolTip("多个唤醒词用英文逗号分隔，例如: 小爱同学, 你好助手")
        self.wakeup_language_edit.setToolTip("语音识别语言代码，例如 zh-CN、en-US。")
        self.asr_provider_combo.setToolTip(
            "ASR 提供商选择:\n"
            "- zhipu_asr: 云端中文识别，默认推荐\n"
            "- xai_realtime: xAI 实时语音接口\n"
            "- google: Web Speech 路径，配置最少\n"
            "- openai_whisper: OpenAI Whisper/转写模型"
        )
        self.asr_api_key_edit.setToolTip("ASR 服务密钥；google 路径通常可留空。")
        self.asr_model_edit.setToolTip("ASR 模型名，按所选 provider 填写，未知时可先使用占位提示值。")
        self.asr_base_url_edit.setToolTip("ASR 接口地址，仅接受 http/https。格式错误会显示红框。")
        self.asr_temperature_spin.setToolTip("ASR 采样温度。越低越稳定，越高越发散。")
        self.asr_prompt_edit.setToolTip("可选提示词，用于补充领域上下文。")
        self.restore_defaults_button.setToolTip("将当前对话框中的所有设置恢复为程序默认值（未保存前可继续修改）。")

    def _setup_control_dependencies(self) -> None:
        """Wire control dependencies and apply initial enabled states."""
        self.offline_mode_checkbox.toggled.connect(self._apply_control_dependencies)
        self.camera_enabled_checkbox.toggled.connect(self._apply_control_dependencies)
        self.wakeup_enabled_checkbox.toggled.connect(self._apply_control_dependencies)
        self.mic_enabled_checkbox.toggled.connect(self._apply_control_dependencies)
        self._apply_control_dependencies()

    def _apply_control_dependencies(self, *_: object) -> None:
        """Refresh enabled states for dependent controls."""
        offline_mode = self.offline_mode_checkbox.isChecked()
        for control in self._ai_controls:
            control.setEnabled(not offline_mode)
        if not offline_mode:
            self._on_screen_auto_toggled(self.screen_auto_commentary_checkbox.isChecked())

        camera_enabled = self.camera_enabled_checkbox.isChecked()
        self.camera_index_spin.setEnabled(camera_enabled)
        self.target_fps_spin.setEnabled(camera_enabled)

        mic_enabled = self.mic_enabled_checkbox.isChecked()
        for control in self._asr_controls:
            control.setEnabled(mic_enabled)
        wakeup_enabled = mic_enabled and self.wakeup_enabled_checkbox.isChecked()
        self.wakeup_phrases_edit.setEnabled(wakeup_enabled)
        self.wakeup_language_edit.setEnabled(wakeup_enabled)

    def _validate_url(self, line_edit: QLineEdit, text: str) -> None:
        """Validate URL text and show a red border when invalid."""
        raw = (text or "").strip()
        if not raw:
            line_edit.setStyleSheet("")
            return
        parsed = urlsplit(raw)
        valid = parsed.scheme in {"http", "https"} and bool(parsed.netloc)
        line_edit.setStyleSheet("" if valid else "QLineEdit { border: 1px solid #d9534f; }")

    def _test_api_connection(self) -> None:
        """Test the current AI provider endpoint and show a result dialog."""
        provider = self.provider_combo.currentText().strip().lower()
        if provider == "none":
            QMessageBox.information(self, "API 连接测试", "当前 LLM 提供商为 none，无需测试远程连接。")
            return

        base_url = self.base_url_edit.text().strip()
        self._validate_url(self.base_url_edit, base_url)
        if not base_url:
            QMessageBox.warning(self, "API 连接测试", "请先填写 API Base URL。")
            return
        parsed = urlsplit(base_url)
        if parsed.scheme not in {"http", "https"} or not parsed.netloc:
            QMessageBox.warning(self, "API 连接测试", "API Base URL 格式错误，请修正后再测试。")
            return

        probe_url = f"{base_url.rstrip('/')}/models"
        headers = {"Accept": "application/json"}
        api_key = self.api_key_edit.text().strip()
        if api_key:
            headers["Authorization"] = f"Bearer {api_key}"

        request = Request(probe_url, headers=headers, method="GET")
        try:
            with urlopen(request, timeout=8) as response:
                status_code = int(getattr(response, "status", 200))
            QMessageBox.information(
                self,
                "API 连接测试",
                f"连接成功。\nProvider: {provider}\nURL: {probe_url}\nHTTP 状态码: {status_code}",
            )
        except HTTPError as exc:
            body_preview = exc.read(180).decode("utf-8", errors="ignore").strip()
            extra = f"\n响应片段: {body_preview}" if body_preview else ""
            QMessageBox.warning(
                self,
                "API 连接测试",
                f"连接失败。\nProvider: {provider}\nURL: {probe_url}\nHTTP 状态码: {exc.code}{extra}",
            )
        except URLError as exc:
            QMessageBox.warning(
                self,
                "API 连接测试",
                f"连接失败。\nProvider: {provider}\nURL: {probe_url}\n错误: {exc.reason}",
            )
        except Exception as exc:
            QMessageBox.warning(
                self,
                "API 连接测试",
                f"连接测试异常。\nProvider: {provider}\nURL: {probe_url}\n错误: {exc}",
            )

    def _restore_defaults(self) -> None:
        """Reset current dialog values to application defaults."""
        answer = QMessageBox.question(
            self,
            "恢复默认值",
            "确定将当前设置恢复为默认值吗？此操作只影响当前对话框，点击“确定”保存后才会生效。",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No,
        )
        if answer != QMessageBox.StandardButton.Yes:
            return
        self._load_from_config(self._defaults)

    def _load_from_config(self, config: AppConfig) -> None:
        """Load UI values from an AppConfig instance."""
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
        self.screen_auto_commentary_checkbox.setChecked(bool(config.screen_commentary.auto_enabled))
        self.screen_auto_interval_spin.setValue(max(1, min(240, int(config.screen_commentary.auto_interval_minutes))))
        self._on_screen_auto_toggled(self.screen_auto_commentary_checkbox.isChecked())

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
        self._apply_control_dependencies()

    def to_config(self) -> AppConfig:
        """Build and return a new AppConfig from current UI values."""
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
                auto_enabled=self.screen_auto_commentary_checkbox.isChecked(),
                auto_interval_minutes=int(self.screen_auto_interval_spin.value()),
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
            self._validate_url(self.asr_base_url_edit, self.asr_base_url_edit.text())
            return
        if normalized == "openai_whisper":
            self.asr_model_edit.setPlaceholderText("openai_whisper: whisper-1 或 gpt-4o-mini-transcribe")
            self.asr_base_url_edit.setPlaceholderText("openai_whisper 建议 https://api.openai.com/v1")
            self._validate_url(self.asr_base_url_edit, self.asr_base_url_edit.text())
            return
        if normalized == "google":
            self.asr_model_edit.setPlaceholderText("google: 无需模型名（可留空）")
            self.asr_base_url_edit.setPlaceholderText("google: 使用 SpeechRecognition 内置 Web Speech")
            self._validate_url(self.asr_base_url_edit, self.asr_base_url_edit.text())
            return
        self.asr_model_edit.setPlaceholderText("xai_realtime: grok-2-mini-transcribe")
        self.asr_base_url_edit.setPlaceholderText("xai_realtime 建议 https://api.x.ai/v1")
        self._validate_url(self.asr_base_url_edit, self.asr_base_url_edit.text())

    def _on_screen_auto_toggled(self, enabled: bool) -> None:
        self.screen_auto_interval_spin.setEnabled(bool(enabled))
