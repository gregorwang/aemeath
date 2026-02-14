# CyberCompanion 代码业务逻辑教学文档（新手版）

## 1. 这份文档讲什么

这份文档专门讲“**代码业务逻辑**”，也就是程序在运行时怎么组织、怎么流转、谁调用谁、状态怎么变化。

它不讲产品运营层面的“为什么做这个功能”，重点讲“代码怎么跑起来”。

你可以把它理解为一条主线：

1. 程序从 `src/main.py` 启动。
2. `main.py` 负责创建并连接所有模块。
3. `src/core/director.py` 负责统一调度运行时行为。
4. `src/ui/*` 负责界面与动画，`src/ai/*` 负责视觉与大模型能力。
5. 整体通过状态机、信号槽、线程、定时器协同运行。

---

## 2. 总体架构图（概念层）

可以把项目分成 3 层：

1. 组装层：`src/main.py`
- 创建对象
- 读取配置
- 连接信号
- 注册托盘、热键、退出清理

2. 核心调度层：`src/core/*`
- 行为编排（`director.py`）
- 状态机（`state_machine.py`）
- 空闲检测、脚本选择、音频管理、配置管理等

3. 能力实现层：`src/ui/*` + `src/ai/*`
- UI 窗口、动效、托盘、设置面板
- 摄像头视线追踪、屏幕解读、LLM provider

最关键理解：

- `Director` 是“总调度器”
- `main.py` 是“装配工厂”
- 其它模块是“单一职责组件”

---

## 3. 启动时到底发生了什么（`main.py`）

### 3.1 启动顺序（强建议记住）

`main()` 关键顺序如下：

1. 设置高 DPI 环境变量。
2. 创建 `QApplication`。
3. 解析配置路径并加载配置（`ConfigManager` + `resolve_config_path`）。
4. 处理旧版 ASR 配置迁移。
5. 初始化日志系统（文件日志 + Qt 日志桥接）。
6. 如有需要弹出摄像头授权确认。
7. 加载角色包（`CharacterLoader`）。
8. 构造核心对象和 UI/AI 组件。
9. 创建 `Director` 并绑定 `IdleMonitor`，启动 idle 线程。
10. 挂托盘菜单、语音唤醒、全局热键、上下文菜单等。
11. 注册 `aboutToQuit` 清理函数，最后进入 `app.exec()`。

### 3.2 启动时创建的关键对象

在 `main.py` 里会创建并连接这些对象：

- `AssetManager`：加载角色脚本与资源
- `AsciiRenderer`：图片转 ASCII HTML
- `EntityWindow`：主角色窗口
- `AudioManager`：TTS 合成与播放调度
- `GazeTracker`：摄像头线程
- `LLMProvider`：`OpenAIProvider` 或 `DummyProvider`
- `ScreenCommentator`：截图 -> LLM -> TTS
- `GifParticleManager`：粒子动画管理
- `GifStateMapper`：状态到粒子效果映射
- `AudioDetector`：系统音频输出检测（当前主流程用这个）
- `Director`：总编排器
- `IdleMonitor`：用户输入空闲检测线程

然后会做两件重要事：

- `director.bind_idle_monitor(idle_monitor)`
- `idle_monitor.start()`

这意味着从此刻开始，空闲检测事件会持续驱动行为逻辑。

---

## 4. 运行模型：事件驱动 + 轮询混合

你的项目不是“一个 while 循环”这么简单，它是典型的桌面应用混合模型：

1. Qt 主事件循环（UI、信号槽）。
2. 各类轮询（空闲、音频、摄像头帧处理）。
3. 后台线程执行耗时任务（TTS、ASR、屏幕解读 worker）。

### 4.1 主要并发机制

- `QThread`
- `IdleMonitor`
- `GazeTracker`
- `VoiceWakeupListener`
- 音频检测 worker / TTS worker

- `QTimer`
- 自动消失、长时空闲、自动屏幕解读
- 自主 roam/probe 动作
- 音频轮询节拍器

- `threading.Thread`
- push-to-talk 单次转写 worker
- `Director` 内屏幕解读 worker 包装线程

- Qt 原生事件过滤器
- 仅用于全局热键 `WM_HOTKEY` 监听与转发

### 4.2 为什么这样设计

- UI 线程必须保持流畅。
- 音频、网络、摄像头、语音识别都可能阻塞。
- 系统级状态（空闲/音频输出）通常靠轮询更加稳定。

---

## 5. 核心编排器 `Director`（最重要模块）

如果你只看一个核心文件，优先看 `src/core/director.py`。

它的职责是：

- 接收各模块输入信号
- 统一做策略判断
- 驱动实体状态、视觉状态、音频播放、AI触发

### 5.1 双状态体系（必须分清）

你的代码里有两套“状态”：

1. 实体生命周期状态（`StateMachine`）
- `HIDDEN`
- `PEEKING`
- `ENGAGED`
- `FLEEING`

2. 行为模式状态（`BehaviorMode`）
- `IDLE`
- `BUSY`
- `MEDIA_PLAYING`
- `SUMMONING`

两者不是同一个维度。

例子：

- 实体可能是 `ENGAGED`，但行为模式可在 `IDLE` 和 `MEDIA_PLAYING` 之间切换。

### 5.2 `Director` 的关键入口事件

1. `on_user_idle()`
- 来源：`IdleMonitor.user_idle_confirmed`
- 守卫条件：轨迹播放中直接忽略；非 `HIDDEN` 状态忽略；全屏暂停规则可拦截
- 用 `PresenceDetector` 决定是 `PRESENT_ACTIVE` / `PRESENT_PASSIVE` / `ABSENT`
- 合法时转入 `ENGAGED`

2. `on_user_active()`
- 来源：`IdleMonitor.user_active_detected`
- 若可见状态则转入 `FLEEING`
- 重置 idle 监控为待命

3. `_enter_engaged()`
- 启动摄像头（若需要且资源计划允许）
- 选脚本（`ScriptEngine` 优先，`AssetManager` 兜底）
- 触发窗口展示（从隐藏进场或已可见则 enter）
- 播放脚本音频
- 启动自动消失计时

4. `_enter_fleeing()`
- 停摄像头
- 切换 flee 视觉态
- 选 panic 脚本并高优先级播报
- 执行窗口逃离动画

5. `request_screen_commentary()`
- 手动或定时触发
- 用锁和计数防止并发重入
- 线程里做资源计划判断 + 屏幕解读调用

6. `_on_audio_output_started()` / `_on_audio_output_stopped()`
- 系统音频播放时切 `MEDIA_PLAYING`
- 必要时强制召唤
- 避免把自己 TTS 当成外部媒体（通过 playback_started/finished 协调）

7. `shutdown()`
- 统一释放计时器、监控器、粒子系统、摄像头、轨迹播放器

### 5.3 `Director` 内部定时器

- `_auto_dismiss_timer`：出现后超时自动隐藏
- `_prolonged_idle_timer`：长时隐藏触发额外特效
- `_auto_screen_commentary_timer`：定时自动屏幕解读
- `_mood_decay_timer`：心情自然回归
- `_voice_trajectory_timeout`：轨迹播放超时兜底

---

## 6. 空闲检测链路（`IdleMonitor`）

`src/core/idle_monitor.py` 是后台线程：

1. 调用 Windows `GetLastInputInfo`
2. 计算 idle 毫秒
3. 发 `idle_time_updated`
4. 驱动内部状态机，发 `user_idle_confirmed` / `user_active_detected`

内部状态机：

- `STANDBY -> PRE_IDLE -> IDLE_TRIGGERED -> ACTIVE`

你最容易混淆的两个时间：

1. 轮询间隔：`POLL_INTERVAL_MS`（当前 100ms）
2. 空闲阈值：`idle_threshold_seconds`（默认 180 秒）

它们是不同概念，代码里是分开控制的。

---

## 7. 脚本与资源业务逻辑（`AssetManager` + `ScriptEngine`）

### 7.1 `AssetManager` 做了什么

- 从角色目录加载 idle/panic 脚本
- 支持 `scripts/dialogue.yaml` + `scripts.json`
- 支持旧格式兼容
- 若加载失败则使用内置默认脚本

脚本数据模型 `Script` 包含：

- `id`
- `text`
- `audio_path`
- `sprite_path`
- `anim_speed`
- `priority`
- `time_range`
- `probability`
- `cooldown_minutes`
- `event_type`

### 7.2 `ScriptEngine` 做了什么

它负责“运行时选择哪条脚本”：

- 按时间段过滤
- 按冷却过滤
- 尽量避免连续重复
- 按概率加权随机

它维护：

- `_last_played`（每条脚本上次触发时间）
- `_last_script_id`（防连续重复）

两个入口：

- `select_idle_script()`：更严格（冷却 + 防重复）
- `select_panic_script()`：更激进（panic 场景不强调防重复）

---

## 8. 配置系统（`ConfigManager`）

### 8.1 配置模型结构

`AppConfig` 是总配置，包含：

- `trigger`
- `appearance`
- `audio`
- `behavior`
- `vision`
- `wakeup`
- `llm`
- `screen_commentary`

每块都是 dataclass，运行时字段访问稳定、可维护性高。

### 8.2 加载/保存特点

1. `load()`
- 输入不合法时回退默认值
- 每个子模块有自己的 build/归一化逻辑

2. `save()`
- 用 Qt `QSaveFile`，减少写配置损坏风险

3. 路径策略（`core/paths.py`）
- 写配置：`%LOCALAPPDATA%/CyberCompanion/config.json`
- 首次可从打包内 `config.json` 引导复制

---

## 9. UI 业务逻辑

## 9.1 主窗口 `EntityWindow`

`src/ui/entity_window.py` 是角色本体窗口，核心能力：

- 透明顶层窗口
- sprite/ascii 双显示栈
- peek/enter/summon/flee 动画
- 自主 roam/probe 动作
- 鼠标交互（拖拽、单双击、右键菜单）

内部还有一个“动画阶段状态”：

- `hidden`
- `peeking`
- `engaged`
- `fleeing`
- `roaming`
- `probing`

状态视觉合成优先级（代码层面非常关键）：

1. 正在移动态
2. 点击覆盖态
3. 悬停态
4. 探头态
5. 基础态

这套优先级决定了“同一时刻多个事件同时发生，显示哪个 GIF”。

## 9.2 托盘 `SystemTrayManager`

托盘模块只发信号，不做业务决策：

- summon
- settings
- status
- commentary
- open logs
- quit
- character switch
- toggle

具体响应逻辑都在 `main.py` 连接处。

## 9.3 设置面板 `SettingsDialog`

三大页签：

1. 基础
2. AI
3. 语音与视觉

核心职责：

- `AppConfig -> UI`（`_load_from_config`）
- `UI -> AppConfig`（`to_config`）
- URL 合法性校验
- 控件依赖联动（比如离线模式禁用 AI 组）
- API 连通性测试

## 9.4 粒子与轨迹动画 `gif_particle.py`

三层对象：

- `GifParticle`：单个粒子生命周期
- `TrajectoryPlayer`：按录制轨迹播放并切换状态 GIF
- `GifParticleManager`：统一管理并发与回收

---

## 10. 音频业务逻辑

## 10.1 `AudioManager`（TTS 与播放总线）

它不是“直接播一下”这么简单，而是完整调度系统：

- 优先级队列（critical/high/normal/low）
- 可中断语义（interrupt）
- 缓存优先（cache first）
- 合成在后台 worker 线程
- 播放队列和合成队列解耦

关键机制：

- token 失效机制，防止过时任务继续播放
- `playback_started` / `playback_finished` 给 `Director` 用来避免“自己播音频触发媒体检测回环”

## 10.2 系统音频检测

当前 `main.py` 走的是 `AudioDetector`：

- 通过 pycaw 读峰值
- 轮询 + 启停去抖
- 发兼容信号 `audio_playing_started/stopped`

仓库还有 `AudioOutputMonitor`（`core/audio_output_monitor.py`）：

- 更偏会话级 WASAPI 检测
- 可按进程名筛媒体会话

`Director` 的接口设计兼容两者信号风格。

---

## 11. 语音业务逻辑（`VoiceWakeupListener`）

`src/core/voice_wakeup.py` 是语音识别主线程：

流程如下：

1. 启动前检查依赖/麦克风/API key
2. 进入持续监听循环（continuous 模式）
3. 每段音频按 provider 识别
- `xai_realtime`
- `zhipu_asr`
- `openai_whisper`
- `google`
4. 发实时转写信号
5. 匹配唤醒词后发 `wake_phrase_detected`

设计亮点：

- xAI 瞬时错误自动重试与退避
- provider/base_url/model 归一化
- `transcribe_once()` 支持 push-to-talk 单次转写
- 错误尽量降级而不是崩溃

---

## 12. 视觉与屏幕解读业务逻辑

## 12.1 `GazeTracker`

`src/ai/gaze_tracker.py` 后台线程做：

1. 打开摄像头抓帧
2. MediaPipe FaceMesh 推理
3. 计算脸部归一化位置 `face_x/face_y`
4. 用启发式规则估计表情（happy/neutral/angry/sad）
5. 发 `gaze_updated(GazeData)`

这个输出会被 `Director` 用于：

- 视线驱动 ASCII 眼睛占位符偏移
- 表情稳定投票后切换角色状态图
- sad/no-face 特殊触发逻辑

## 12.2 `ScreenCommentator`

`src/ai/screen_commentator.py` 的完整链路：

1. 创建会话 id（防并发串音）
2. 打断旧音频并播开场白
3. 截图（优先前台窗口 bbox）
4. 转 base64 JPEG
5. 调 LLM 视觉接口
- 优先流式
- 失败回退单次
6. 文本按 `TextChunker` 切片并送 TTS
7. 全失败则播固定兜底句

会话安全关键点：

- `cancel_current_session()` 递增 session id
- 每个阶段都检查 `session_id` 是否仍有效

## 12.3 `LLMProvider` 抽象

`src/ai/llm_provider.py` 统一接口：

- `generate()`
- `generate_with_image()`
- `generate_with_image_stream()`

实现：

- `OpenAIProvider`
- 聊天 + 视觉 + SSE 流式
- 鉴权方式回退
- HTTP 瞬时错误重试
- base URL 回退策略

- `DummyProvider`
- 离线固定回复，保证主流程可运行

---

## 13. 命令路由与交互入口

`core/command_matcher.py` 用模糊匹配把语音文本映射到动作：

- summon
- screen_commentary
- hide
- toggle_visibility
- status

这些动作会从多个入口进入：

1. 连续语音唤醒（wake phrase）
2. 全局 B 键 push-to-talk 单次转写
3. 托盘菜单
4. 角色窗口右键菜单
5. 窗口双击

最终都路由到 `Director` 的动作接口。

---

## 14. 原生系统能力接入点

1. 空闲检测：Windows `GetLastInputInfo`
2. 全局热键：`RegisterHotKey` + Qt `nativeEventFilter` 监听 `WM_HOTKEY`
3. 前台窗口信息：`GetForegroundWindow` + `GetWindowRect`
4. 用户数据目录：`%LOCALAPPDATA%/CyberCompanion`

---

## 15. 三条典型事件时序（代码视角）

### 15.1 空闲触发出场

1. `IdleMonitor` 轮询达到阈值，发 `user_idle_confirmed`
2. `Director.on_user_idle()` 做守卫判断和 presence 判断
3. 状态机 `HIDDEN -> ENGAGED`
4. `_enter_engaged()` 选脚本 + 召唤窗口
5. `AudioManager.play_script()` 播报
6. 自动消失计时到期后可能回 `HIDDEN`

### 15.2 用户回来触发逃离

1. `IdleMonitor` 检测活跃，发 `user_active_detected`
2. `Director.on_user_active()` 触发 `FLEEING`
3. panic 脚本高优先级播放
4. `EntityWindow.flee()` 动画结束
5. FSM 回 `HIDDEN`

### 15.3 手动触发屏幕解读

1. 托盘/菜单/语音命令触发 `request_screen_commentary`
2. worker 线程做资源计划判断
3. `ScreenCommentator` 截图并请求视觉模型
4. 分块 TTS 播报结果

---

## 16. 降级与容错模式（代码层设计思想）

这个项目很多地方都采用“可降级，不崩溃”策略：

- LLM 网络错误：可切离线模式，走 `DummyProvider`
- 摄像头失败：仅本次会话降级摄像头能力
- 麦克风/ASR 失败：语音模块停用但主程序继续
- 脚本文件缺失：回退内置脚本
- 音频检测依赖缺失：检测不可用但 UI 主流程可继续

这是代码业务逻辑中的“稳定性优先”设计，不是产品文案层面的逻辑。

---

## 17. 文件到职责的速查表

- `src/main.py`：程序装配与总入口
- `src/core/director.py`：运行时总调度
- `src/core/state_machine.py`：实体状态机
- `src/core/idle_monitor.py`：用户输入空闲检测
- `src/core/asset_manager.py`：脚本与资源加载
- `src/core/script_engine.py`：脚本选择策略
- `src/core/config_manager.py`：配置模型与读写
- `src/core/audio_manager.py`：TTS 合成与播放调度
- `src/core/audio_detector.py`：系统音频峰值检测
- `src/core/voice_wakeup.py`：连续唤醒与单次转写
- `src/ai/gaze_tracker.py`：摄像头视线/表情
- `src/ai/screen_commentator.py`：截图 + 视觉模型 + 播报
- `src/ai/llm_provider.py`：LLM 抽象和 OpenAI 实现
- `src/ui/entity_window.py`：主角色窗口与交互动画
- `src/ui/gif_particle.py`：粒子系统和轨迹播放器
- `src/ui/tray_icon.py`：托盘信号面板
- `src/ui/settings_dialog.py`：设置编辑器

---

## 18. 新手推荐阅读顺序（按执行链路）

建议按这个顺序读源码：

1. `src/main.py`
2. `src/core/director.py`
3. `src/core/state_machine.py`
4. `src/core/idle_monitor.py`
5. `src/ui/entity_window.py`
6. `src/core/audio_manager.py`
7. `src/core/voice_wakeup.py`
8. `src/ai/screen_commentator.py`
9. `src/core/config_manager.py`

这个顺序基本等于“真实运行时主链路”。
