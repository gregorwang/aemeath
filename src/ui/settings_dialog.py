from __future__ import annotations

from dataclasses import replace
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit, urlunsplit
from urllib.request import Request, urlopen

from PySide6.QtCore import QSize, Qt
from PySide6.QtGui import QMovie
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
    QScrollArea,
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
        "kimi-k2.5",
        "kimi-k2-thinking",
        "moonshot-v1-auto",
        "moonshot-v1-32k",
        "glm-5",
        "glm-4.7",
        "glm-4.6",
        "glm-4.5",
        "glm-4.5-air",
        "glm-4.5-flash",
        "ep-XXXXXXX",
        "doubao-1-5-pro-256k-250115",
        "doubao-1-5-pro-32k-250115",
        "doubao-seed-1-6-thinking-250715",
        "doubao-1-5-ui-tars-250428",
        "grok-4.1",
        "grok-4.1-fast-reasoning",
        "grok-4.1-fast-non-reasoning",
        "grok-4",
        "grok-4-fast-reasoning",
        "grok-3-mini",
        "grok-code-fast-1",
        "grok-2-image-1212",
        "deepseek-chat",
        "deepseek-reasoner",
        "gpt-5.1",
        "gpt-5.1-mini",
        "gpt-5",
        "gpt-5-mini",
        "gpt-4.1",
        "gpt-4o",
    ]
    MODEL_PRESETS_BY_PROVIDER = {
        "openai": ["gpt-5-mini", "gpt-5", "gpt-4.1", "gpt-4o", "gpt-5.1", "gpt-5.1-mini"],
        "xai": ["grok-4.1", "grok-4.1-fast-reasoning", "grok-4.1-fast-non-reasoning", "grok-4", "grok-4-fast-reasoning", "grok-3-mini", "grok-code-fast-1", "grok-2-image-1212"],
        "deepseek": ["deepseek-chat", "deepseek-reasoner"],
        "kimi": ["kimi-k2.5", "kimi-k2-thinking", "moonshot-v1-auto", "moonshot-v1-32k"],
        "zhipu": ["glm-5", "glm-4.7", "glm-4.6", "glm-4.5", "glm-4.5-air", "glm-4.5-flash"],
        "doubao": [
            "ep-XXXXXXX",
            "doubao-1-5-pro-256k-250115",
            "doubao-1-5-pro-32k-250115",
            "doubao-seed-1-6-thinking-250715",
            "doubao-1-5-ui-tars-250428",
        ],
    }
    PROVIDER_DEFAULTS = {
        "none": ("", ""),
        "openai": ("https://api.openai.com/v1", "gpt-5-mini"),
        "xai": ("https://api.x.ai", "grok-4.1"),
        "deepseek": ("https://api.deepseek.com/v1", "deepseek-chat"),
        "kimi": ("https://api.moonshot.cn/v1", "kimi-k2.5"),
        "zhipu": ("https://open.bigmodel.cn/api/paas/v4", "glm-5"),
        "doubao": ("https://ark.cn-beijing.volces.com/api/v3", "ep-XXXXXXX"),
    }
    ENDPOINT_PRESETS = {
        "OpenAI 官方": ("openai", "https://api.openai.com/v1", "gpt-5-mini"),
        "xAI 官方(推荐看图)": ("xai", "https://api.x.ai", "grok-4.1"),
        "DeepSeek 官方(当前文本为主)": ("deepseek", "https://api.deepseek.com/v1", "deepseek-chat"),
        "Kimi 官方": ("kimi", "https://api.moonshot.cn/v1", "kimi-k2.5"),
        "智谱官方": ("zhipu", "https://open.bigmodel.cn/api/paas/v4", "glm-5"),
        "豆包方舟": ("doubao", "https://ark.cn-beijing.volces.com/api/v3", "ep-XXXXXXX"),
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
        tabs.addTab(self._create_animation_preview_tab(), "动画预览")
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
        general_layout = QVBoxLayout(general_tab)
        general_form = QFormLayout()
        self.position_combo = QComboBox(self)
        self.position_combo.addItems(["auto", "left", "right"])
        self.fullscreen_pause_checkbox = QCheckBox("全屏应用时暂停", self)
        self.resident_mode_checkbox = QCheckBox("角色常驻模式（全屏时自动隐藏）", self)
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
        general_form.addRow("", self.resident_mode_checkbox)
        general_form.addRow("", self.audio_output_reactive_checkbox)
        general_form.addRow("", self.debug_mode_checkbox)
        general_form.addRow("", self.offline_mode_checkbox)
        general_form.addRow("日志文件", self.log_path_edit)
        general_layout.addLayout(general_form)

        invasion_box = QGroupBox("空闲入侵 (Idle Invasion)", general_tab)
        invasion_form = QFormLayout(invasion_box)
        self.invasion_enabled_checkbox = QCheckBox("启用空闲入侵", self)
        self.invasion_start_delay_spin = QSpinBox(self)
        self.invasion_start_delay_spin.setRange(5, 3600)
        self.invasion_start_delay_spin.setSuffix(" 秒")
        self.invasion_initial_interval_spin = QSpinBox(self)
        self.invasion_initial_interval_spin.setRange(1, 120)
        self.invasion_initial_interval_spin.setSuffix(" 秒")
        self.invasion_min_interval_spin = QSpinBox(self)
        self.invasion_min_interval_spin.setRange(1, 60)
        self.invasion_min_interval_spin.setSuffix(" 秒")
        self.invasion_max_invaders_spin = QSpinBox(self)
        self.invasion_max_invaders_spin.setRange(1, 100)
        self.invasion_scale_spin = QDoubleSpinBox(self)
        self.invasion_scale_spin.setRange(0.2, 2.0)
        self.invasion_scale_spin.setSingleStep(0.05)
        self.invasion_scale_spin.setDecimals(2)
        self.invasion_cell_padding_spin = QSpinBox(self)
        self.invasion_cell_padding_spin.setRange(0, 100)
        self.invasion_cell_padding_spin.setSuffix(" px")
        self.invasion_retreat_style_combo = QComboBox(self)
        self.invasion_retreat_style_combo.addItem("scatter（四散奔逃）", "scatter")
        self.invasion_retreat_style_combo.addItem("ripple（波纹退场）", "ripple")
        self.invasion_retreat_style_combo.addItem("instant（瞬间消失）", "instant")
        self.invasion_gifs_edit = QLineEdit(self)
        self.invasion_gifs_edit.setPlaceholderText("例如 state1.gif, state2.gif, aemeath.gif")
        invasion_form.addRow("", self.invasion_enabled_checkbox)
        invasion_form.addRow("开始入侵延迟", self.invasion_start_delay_spin)
        invasion_form.addRow("初始生成间隔", self.invasion_initial_interval_spin)
        invasion_form.addRow("最小生成间隔", self.invasion_min_interval_spin)
        invasion_form.addRow("最大入侵数量", self.invasion_max_invaders_spin)
        invasion_form.addRow("GIF 缩放比例", self.invasion_scale_spin)
        invasion_form.addRow("网格额外间距", self.invasion_cell_padding_spin)
        invasion_form.addRow("退场风格", self.invasion_retreat_style_combo)
        invasion_form.addRow("参与入侵 GIF", self.invasion_gifs_edit)
        general_layout.addWidget(invasion_box)
        general_layout.addStretch(1)
        self._idle_invasion_controls = [
            self.invasion_start_delay_spin,
            self.invasion_initial_interval_spin,
            self.invasion_min_interval_spin,
            self.invasion_max_invaders_spin,
            self.invasion_scale_spin,
            self.invasion_cell_padding_spin,
            self.invasion_retreat_style_combo,
            self.invasion_gifs_edit,
        ]
        return general_tab

    def _create_ai_tab(self) -> QWidget:
        """Create and return the AI settings tab."""
        ai_tab = QWidget(self)
        ai_form = QFormLayout(ai_tab)
        self.provider_combo = QComboBox(self)
        self.provider_combo.addItems(["none", "openai", "xai", "deepseek", "kimi", "zhipu", "doubao"])
        self.provider_combo.currentTextChanged.connect(self._on_provider_changed)
        self.endpoint_preset_combo = QComboBox(self)
        self.endpoint_preset_combo.addItems(["自定义(不改)", *self.ENDPOINT_PRESETS.keys()])
        self.endpoint_preset_combo.currentTextChanged.connect(self._on_endpoint_preset_changed)
        self.model_combo = QComboBox(self)
        self.model_combo.setEditable(True)
        self.model_combo.addItems(self.MODEL_PRESETS)
        self.model_combo.setInsertPolicy(QComboBox.InsertPolicy.NoInsert)
        self.base_url_edit = QLineEdit(self)
        self.base_url_edit.setPlaceholderText("例如 https://api.x.ai 或 https://api.openai.com/v1")
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
        self.periodic_scan_checkbox = QCheckBox("启用周期性摄像头巡检", self)
        self.periodic_scan_interval_spin = QSpinBox(self)
        self.periodic_scan_interval_spin.setRange(5, 240)
        self.periodic_scan_interval_spin.setSuffix(" 分钟")
        self.camera_index_spin = QSpinBox(self)
        self.camera_index_spin.setRange(0, 8)
        self.target_fps_spin = QSpinBox(self)
        self.target_fps_spin.setRange(1, 30)
        vision_form.addRow("", self.camera_enabled_checkbox)
        vision_form.addRow("", self.eye_tracking_checkbox)
        vision_form.addRow("", self.periodic_scan_checkbox)
        vision_form.addRow("巡检间隔", self.periodic_scan_interval_spin)
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
        self.voice_input_mode_combo.addItem("push_to_talk（按全局 Ctrl+B 单次转写）", "push_to_talk")
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
                "\n提示: push_to_talk 模式下可按全局 Ctrl+B 进行单次语音转写。"
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

    def _create_animation_preview_tab(self) -> QWidget:
        """Create and return the Animation Preview tab."""
        tab = QWidget(self)
        outer_layout = QVBoxLayout(tab)

        scroll = QScrollArea(tab)
        scroll.setWidgetResizable(True)
        scroll_content = QWidget(scroll)
        layout = QVBoxLayout(scroll_content)

        preview_box = QGroupBox("GIF 状态预览", scroll_content)
        preview_layout = QVBoxLayout(preview_box)

        gif_info: list[tuple[str, str, str, list[str]]] = [
            (
                "state1.gif",
                "默认/好奇",
                "STATE_IDLE",
                [
                    "角色出场后的默认状态",
                    "BehaviorMode.IDLE 时显示",
                    "表情追踪识别为 neutral 时显示",
                    "台词播放时的默认视觉",
                ],
            ),
            (
                "state2.gif",
                "兴奋/活跃",
                "STATE_EXCITED",
                [
                    "左键单击角色时随机切换到此状态（3秒后恢复）",
                    "用户从深度睡眠唤醒后显示",
                    "粒子：角色进入互动状态时生成 1 个此 GIF 粒子",
                ],
            ),
            (
                "state3.gif",
                "律动/音乐",
                "STATE_ROAMING",
                [
                    "BehaviorMode.MEDIA_PLAYING 时显示（检测到系统音频）",
                    "角色自主漫步移动时显示",
                    "粒子：检测到系统音频开始时生成律动粒子",
                    "粒子：音频播放中每 4 秒可能追加 1 个粒子",
                ],
            ),
            (
                "state4.gif",
                "害羞/逃跑",
                "STATE_FLEE",
                [
                    "角色逃跑（FLEEING）时显示",
                    "BehaviorMode.BUSY 时显示",
                    "表情追踪识别为 angry 时显示",
                    "粒子：角色逃跑时生成 1 个此 GIF 粒子",
                ],
            ),
            (
                "state5.gif",
                "思考/观察",
                "STATE_HOVER",
                [
                    "鼠标悬停在角色上时显示",
                    "屏幕评论（'你在看什么？'）进行时显示",
                    "进入被动陪伴（用户在但不活跃）时显示",
                    "深度睡眠（用户不在）时显示",
                    "表情追踪识别为 sad 时显示",
                    "悲伤安慰触发时显示",
                    "粒子：悲伤安慰时生成 1 个、长时间空闲时生成 1 个",
                ],
            ),
            (
                "state6.gif",
                "开心/问候",
                "STATE_GREETING",
                [
                    "左键单击角色时随机切换到此状态（3秒后恢复）",
                    "BehaviorMode.SUMMONING 时显示（轨迹登场中）",
                    "用户离开后回来时显示",
                    "表情追踪识别为 happy 时显示",
                    "粒子：语音唤醒/召唤完成时 1 个、用户回来时 1 个",
                ],
            ),
            (
                "state7.gif",
                "环境/装饰",
                "STATE_AMBIENT",
                [
                    "角色自主探头侦查时随机使用",
                    "粒子：音频脉动中随机替代律动粒子",
                    "粒子：长时间空闲时随机生成",
                    "粒子：spawn_random_ambient() 调用时生成",
                ],
            ),
        ]

        characters_dir = self._find_characters_dir()
        self._preview_movies: list[QMovie] = []

        for gif_file, label_text, const_name, triggers in gif_info:
            row_widget = QWidget(preview_box)
            row_layout = QHBoxLayout(row_widget)
            row_layout.setContentsMargins(4, 4, 4, 4)

            preview_label = QLabel(row_widget)
            preview_label.setFixedSize(64, 64)
            preview_label.setStyleSheet(
                "QLabel { background: #2a2a2a; border: 1px solid #555; border-radius: 4px; }"
            )
            preview_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
            if characters_dir is not None:
                gif_path = characters_dir / gif_file
                if gif_path.exists():
                    movie = QMovie(str(gif_path))
                    if movie.isValid():
                        movie.setParent(preview_label)
                        movie.setCacheMode(QMovie.CacheMode.CacheAll)
                        movie.setScaledSize(QSize(56, 56))
                        preview_label.setMovie(movie)
                        movie.start()
                        self._preview_movies.append(movie)
            row_layout.addWidget(preview_label)

            info_layout = QVBoxLayout()
            title_label = QLabel(f"<b>{gif_file}</b> — {label_text} ({const_name})", row_widget)
            info_layout.addWidget(title_label)
            for trigger in triggers:
                trigger_label = QLabel(f"  • {trigger}", row_widget)
                trigger_label.setStyleSheet("QLabel { color: #888; font-size: 11px; }")
                trigger_label.setWordWrap(True)
                info_layout.addWidget(trigger_label)
            row_layout.addLayout(info_layout, stretch=1)
            preview_layout.addWidget(row_widget)

        layout.addWidget(preview_box)

        mode_box = QGroupBox("行为模式 → GIF 映射", scroll_content)
        mode_layout = QVBoxLayout(mode_box)
        mode_info = QLabel(
            "<pre>"
            "BehaviorMode.IDLE          → state1.gif (默认/好奇)\n"
            "BehaviorMode.BUSY          → state4.gif (害羞/逃跑)\n"
            "BehaviorMode.MEDIA_PLAYING → state3.gif (律动/音乐)\n"
            "BehaviorMode.SUMMONING     → state6.gif (开心/问候)"
            "</pre>",
            mode_box,
        )
        mode_info.setTextFormat(Qt.TextFormat.RichText)
        mode_layout.addWidget(mode_info)
        layout.addWidget(mode_box)

        expr_box = QGroupBox("摄像头表情识别 → GIF 映射", scroll_content)
        expr_layout = QVBoxLayout(expr_box)
        expr_info = QLabel(
            "<pre>"
            "neutral (中性) → state1.gif\n"
            "angry   (生气) → state4.gif\n"
            "sad     (悲伤) → state5.gif\n"
            "happy   (开心) → state6.gif"
            "</pre>",
            expr_box,
        )
        expr_info.setTextFormat(Qt.TextFormat.RichText)
        expr_layout.addWidget(expr_info)
        expr_note = QLabel(
            "提示: 表情识别需要启用摄像头功能，且角色处于可见状态时才会生效。",
            expr_box,
        )
        expr_note.setStyleSheet("QLabel { color: #888; }")
        expr_note.setWordWrap(True)
        expr_layout.addWidget(expr_note)
        layout.addWidget(expr_box)

        mouse_box = QGroupBox("鼠标交互 → GIF 映射", scroll_content)
        mouse_layout = QVBoxLayout(mouse_box)
        mouse_info = QLabel(
            "• <b>鼠标悬停</b>: 切换到 state5.gif (思考/观察)\n"
            "• <b>左键单击</b>: 随机切换到 state2.gif 或 state6.gif, 3 秒后恢复\n"
            "• <b>自主漫步</b>: 移动时切换到 state3.gif, 到达后恢复\n"
            "• <b>自主探头</b>: 随机使用 state1.gif 或 state7.gif",
            mouse_box,
        )
        mouse_info.setWordWrap(True)
        mouse_layout.addWidget(mouse_info)
        layout.addWidget(mouse_box)

        layout.addStretch(1)
        scroll.setWidget(scroll_content)
        outer_layout.addWidget(scroll)
        return tab

    @staticmethod
    def _find_characters_dir() -> Path | None:
        """Locate the characters directory for GIF preview."""
        candidates = [
            Path.cwd() / "characters",
            Path(__file__).resolve().parent.parent.parent / "characters",
        ]
        for candidate in candidates:
            if candidate.is_dir() and (candidate / "state1.gif").exists():
                return candidate
        return None

    def _create_button_box(self) -> QDialogButtonBox:
        """Create the OK/Cancel button box."""
        button_box = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel,
            parent=self,
        )
        ok_button = button_box.button(QDialogButtonBox.StandardButton.Ok)
        if ok_button is not None:
            ok_button.setText("保存并应用")
        cancel_button = button_box.button(QDialogButtonBox.StandardButton.Cancel)
        if cancel_button is not None:
            cancel_button.setText("取消")
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
        self.resident_mode_checkbox.setToolTip("开启后角色保持常驻显示；若检测到全屏应用会自动隐藏。")
        self.invasion_enabled_checkbox.setToolTip("开启后，空闲达到阈值会触发小人入侵。")
        self.invasion_start_delay_spin.setToolTip("空闲多久后开始入侵。")
        self.invasion_initial_interval_spin.setToolTip("刚开始入侵时的生成节奏（秒）。")
        self.invasion_min_interval_spin.setToolTip("加速后最低生成间隔（秒）。")
        self.invasion_max_invaders_spin.setToolTip("屏幕上同时存在的小人上限。")
        self.invasion_scale_spin.setToolTip("入侵小人的 GIF 缩放比例。")
        self.invasion_cell_padding_spin.setToolTip("网格格子额外间距（像素），用于防重叠。")
        self.invasion_retreat_style_combo.setToolTip("用户恢复操作时的退场动画风格。")
        self.invasion_gifs_edit.setToolTip("参与入侵的 GIF 文件名，英文逗号分隔。")
        self.provider_combo.setToolTip(
            "LLM 提供商选择:\n"
            "- none: 不调用远程模型\n"
            "- openai: OpenAI 官方接口\n"
            "- xai: xAI 官方接口，视觉场景常用\n"
            "- deepseek: DeepSeek 官方接口，文本场景常用\n"
            "- kimi: Moonshot Kimi 官方接口\n"
            "- zhipu: 智谱 GLM 官方接口\n"
            "- doubao: 火山方舟（豆包）官方接口"
        )
        self.endpoint_preset_combo.setToolTip("快速填充 provider/base_url/model 组合；“自定义(不改)”不覆盖现有输入。")
        self.model_combo.setToolTip(
            "模型 ID。可从预设中选，也可手动输入服务端支持的模型名。"
            "豆包方舟常见做法是填写推理接入点 ID（ep-...）。"
        )
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
        self.periodic_scan_checkbox.setToolTip("每隔固定时间短暂启动摄像头，判断是否在屏幕前并输出状态动作。")
        self.periodic_scan_interval_spin.setToolTip("周期性摄像头巡检间隔（分钟）。")
        self.camera_index_spin.setToolTip("默认填 0（系统默认摄像头），有多个摄像头时可试 1、2。")
        self.target_fps_spin.setToolTip("每秒处理帧数。越高越流畅，但占用更高。")
        self.mic_enabled_checkbox.setToolTip("语音输入总开关；关闭后将禁用 ASR 相关配置。")
        self.voice_input_mode_combo.setToolTip(
            "语音输入模式:\n"
            "- continuous: 后台持续监听，支持唤醒词\n"
            "- push_to_talk: 按全局 Ctrl+B 触发一次转写，资源占用更低"
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
        self.periodic_scan_checkbox.toggled.connect(self._apply_control_dependencies)
        self.wakeup_enabled_checkbox.toggled.connect(self._apply_control_dependencies)
        self.mic_enabled_checkbox.toggled.connect(self._apply_control_dependencies)
        self.invasion_enabled_checkbox.toggled.connect(self._apply_control_dependencies)
        self._apply_control_dependencies()

    def _apply_control_dependencies(self, *_: object) -> None:
        """Refresh enabled states for dependent controls."""
        offline_mode = self.offline_mode_checkbox.isChecked()
        for control in self._ai_controls:
            control.setEnabled(not offline_mode)
        if not offline_mode:
            self._on_screen_auto_toggled(self.screen_auto_commentary_checkbox.isChecked())

        camera_enabled = self.camera_enabled_checkbox.isChecked()
        self.periodic_scan_checkbox.setEnabled(camera_enabled)
        self.periodic_scan_interval_spin.setEnabled(camera_enabled and self.periodic_scan_checkbox.isChecked())
        self.camera_index_spin.setEnabled(camera_enabled)
        self.target_fps_spin.setEnabled(camera_enabled)

        mic_enabled = self.mic_enabled_checkbox.isChecked()
        for control in self._asr_controls:
            control.setEnabled(mic_enabled)
        wakeup_enabled = mic_enabled and self.wakeup_enabled_checkbox.isChecked()
        self.wakeup_phrases_edit.setEnabled(wakeup_enabled)
        self.wakeup_language_edit.setEnabled(wakeup_enabled)

        invasion_enabled = self.invasion_enabled_checkbox.isChecked()
        for control in self._idle_invasion_controls:
            control.setEnabled(invasion_enabled)

    def _validate_url(self, line_edit: QLineEdit, text: str) -> None:
        """Validate URL text and show a red border when invalid."""
        raw = (text or "").strip()
        if not raw:
            line_edit.setStyleSheet("")
            return
        parsed = urlsplit(raw)
        valid = parsed.scheme in {"http", "https"} and bool(parsed.netloc)
        line_edit.setStyleSheet("" if valid else "QLineEdit { border: 1px solid #d9534f; }")

    @staticmethod
    def _normalize_base_url_for_probe(base_url: str) -> str:
        """
        Normalize base URL before probing /models.
        Accepts inputs such as:
        - https://api.x.ai
        - https://api.x.ai/v1
        - https://api.x.ai/v1/chat/completions
        - https://open.bigmodel.cn/api/paas/v4/chat/completions
        - https://ark.cn-beijing.volces.com/api/v3/responses
        """
        raw = (base_url or "").strip().rstrip("/")
        if not raw:
            return ""
        parsed = urlsplit(raw)
        if parsed.scheme not in {"http", "https"} or not parsed.netloc:
            return raw

        path = (parsed.path or "").rstrip("/")
        for suffix in ("/chat/completions", "/responses", "/audio/transcriptions", "/audio/translations"):
            if path.endswith(suffix):
                path = path[: -len(suffix)]
                break
        if not path:
            path = "/v1"
        return urlunsplit((parsed.scheme, parsed.netloc, path, "", ""))

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

        probe_base = self._normalize_base_url_for_probe(base_url)
        probe_url = f"{probe_base.rstrip('/')}/models"
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
        self.resident_mode_checkbox.setChecked(bool(config.behavior.resident_mode))
        self.audio_output_reactive_checkbox.setChecked(bool(config.behavior.audio_output_reactive))
        self.debug_mode_checkbox.setChecked(bool(config.behavior.debug_mode))
        self.offline_mode_checkbox.setChecked(bool(config.behavior.offline_mode))
        self.invasion_enabled_checkbox.setChecked(bool(config.idle_invasion.enabled))
        self.invasion_start_delay_spin.setValue(max(5, int(config.idle_invasion.start_delay_ms / 1000)))
        self.invasion_initial_interval_spin.setValue(max(1, int(config.idle_invasion.initial_spawn_interval_ms / 1000)))
        self.invasion_min_interval_spin.setValue(max(1, int(config.idle_invasion.min_spawn_interval_ms / 1000)))
        self.invasion_max_invaders_spin.setValue(max(1, min(100, int(config.idle_invasion.max_invaders))))
        self.invasion_scale_spin.setValue(float(config.idle_invasion.scale))
        self.invasion_cell_padding_spin.setValue(max(0, min(100, int(config.idle_invasion.cell_padding))))
        self._set_combo_data(self.invasion_retreat_style_combo, config.idle_invasion.retreat_style, "scatter")
        self.invasion_gifs_edit.setText(", ".join(config.idle_invasion.participating_gifs))

        self._set_combo_text(self.provider_combo, config.llm.provider, "xai")
        self._on_provider_changed(self.provider_combo.currentText())
        self._set_combo_text(self.model_combo, config.llm.model, self._default_model_for_provider(self.provider_combo.currentText()))
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
        self.periodic_scan_checkbox.setChecked(bool(config.vision.periodic_scan_enabled))
        self.periodic_scan_interval_spin.setValue(max(5, min(240, int(config.vision.periodic_scan_interval_minutes))))
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
            resident_mode=self.resident_mode_checkbox.isChecked(),
            audio_output_reactive=self.audio_output_reactive_checkbox.isChecked(),
            debug_mode=self.debug_mode_checkbox.isChecked(),
            offline_mode=self.offline_mode_checkbox.isChecked(),
        )
        llm = replace(
            self._source.llm,
            provider=self.provider_combo.currentText().strip().lower() or "none",
            model=(
                self.model_combo.currentText().strip()
                or self._source.llm.model
                or self._default_model_for_provider(self.provider_combo.currentText())
            ),
            base_url=(
                self.base_url_edit.text().strip()
                or self._source.llm.base_url
                or self._default_base_url_for_provider(self.provider_combo.currentText())
            ),
            api_key=self.api_key_edit.text().strip(),
        )
        vision = replace(
            self._source.vision,
            camera_enabled=self.camera_enabled_checkbox.isChecked(),
            eye_tracking_enabled=self.eye_tracking_checkbox.isChecked(),
            periodic_scan_enabled=self.periodic_scan_checkbox.isChecked(),
            periodic_scan_interval_minutes=int(self.periodic_scan_interval_spin.value()),
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
            asr_model=self.asr_model_edit.text().strip(),
            asr_base_url=self.asr_base_url_edit.text().strip(),
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
        raw_invasion_gifs = self.invasion_gifs_edit.text().strip()
        invasion_gifs = (
            tuple(part.strip() for part in raw_invasion_gifs.split(",") if part.strip())
            if raw_invasion_gifs
            else self._source.idle_invasion.participating_gifs
        )
        idle_invasion = replace(
            self._source.idle_invasion,
            enabled=self.invasion_enabled_checkbox.isChecked(),
            start_delay_ms=max(5000, int(self.invasion_start_delay_spin.value()) * 1000),
            initial_spawn_interval_ms=max(1000, int(self.invasion_initial_interval_spin.value()) * 1000),
            min_spawn_interval_ms=max(500, int(self.invasion_min_interval_spin.value()) * 1000),
            max_invaders=max(1, int(self.invasion_max_invaders_spin.value())),
            scale=float(self.invasion_scale_spin.value()),
            cell_padding=max(0, int(self.invasion_cell_padding_spin.value())),
            participating_gifs=invasion_gifs,
            retreat_style=str(self.invasion_retreat_style_combo.currentData() or "scatter"),
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
                preamble_text=self.screen_preamble_edit.text().strip(),
                auto_enabled=self.screen_auto_commentary_checkbox.isChecked(),
                auto_interval_minutes=int(self.screen_auto_interval_spin.value()),
            ),
            idle_invasion=idle_invasion,
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

    def _default_base_url_for_provider(self, provider: str) -> str:
        normalized = (provider or "").strip().lower()
        return str(self.PROVIDER_DEFAULTS.get(normalized, ("", ""))[0])

    def _default_model_for_provider(self, provider: str) -> str:
        normalized = (provider or "").strip().lower()
        return str(self.PROVIDER_DEFAULTS.get(normalized, ("", ""))[1])

    def _model_presets_for_provider(self, provider: str) -> list[str]:
        normalized = (provider or "").strip().lower()
        presets = self.MODEL_PRESETS_BY_PROVIDER.get(normalized)
        return list(presets) if presets else list(self.MODEL_PRESETS)

    def _on_provider_changed(self, provider: str) -> None:
        normalized = (provider or "").strip().lower()
        current_model = self.model_combo.currentText().strip()
        model_presets = self._model_presets_for_provider(normalized)
        preferred_default = self._default_model_for_provider(normalized) or (model_presets[0] if model_presets else "")

        self.model_combo.blockSignals(True)
        self.model_combo.clear()
        if model_presets:
            self.model_combo.addItems(model_presets)
        self.model_combo.blockSignals(False)
        self._set_combo_text(
            self.model_combo,
            current_model or preferred_default,
            preferred_default,
        )
        model_edit = self.model_combo.lineEdit()
        if model_edit is not None:
            if normalized == "doubao":
                model_edit.setPlaceholderText("例如 doubao-seed-1-6-250615 或 ep-xxxxxxxxxxxxxxxxx")
            elif normalized == "kimi":
                model_edit.setPlaceholderText("例如 kimi-latest")
            elif normalized == "zhipu":
                model_edit.setPlaceholderText("例如 glm-5")
            else:
                model_edit.setPlaceholderText("")

        if normalized == "openai":
            self.base_url_edit.setPlaceholderText("例如 https://api.openai.com/v1")
        elif normalized == "xai":
            self.base_url_edit.setPlaceholderText("例如 https://api.x.ai")
        elif normalized == "deepseek":
            self.base_url_edit.setPlaceholderText("例如 https://api.deepseek.com/v1")
        elif normalized == "kimi":
            self.base_url_edit.setPlaceholderText("例如 https://api.moonshot.cn/v1")
        elif normalized == "zhipu":
            self.base_url_edit.setPlaceholderText("例如 https://open.bigmodel.cn/api/paas/v4")
        elif normalized == "doubao":
            self.base_url_edit.setPlaceholderText("例如 https://ark.cn-beijing.volces.com/api/v3")
        else:
            self.base_url_edit.setPlaceholderText("例如 https://api.openai.com/v1")

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
