# Qt 原生能力改造评审（CyberCompanion）

## 1. 评审目标

本次评审目标是：在不改变产品功能的前提下，系统性识别项目中可由 Qt 原生能力接管的实现，提升动画流畅度、调度稳定性、跨平台一致性和可维护性。

评审时间：2026-02-13  
评审方式：并行子代理专项审查（UI/动画、调度/定时、音频/线程、路径/配置/资源）

## 2. 范围

- `src/ui/entity_window.py`
- `src/ui/gif_particle.py`
- `src/core/director.py`
- `src/core/audio_manager.py`
- `src/core/audio_detector.py`
- `src/core/audio_output_monitor.py`
- `src/core/voice_wakeup.py`
- `src/core/paths.py`
- `src/core/config_manager.py`
- `src/core/asset_manager.py`
- `src/main.py`
- 轨迹文件：`recorded_paths/trajectory_1770800738_optimized.json`、`recorded_paths/trajectory_1770800738_qt_animation.json`

## 3. 总体结论

项目已大量使用 Qt（`QWidget/QMovie/QTimer/QPropertyAnimation/Signal-Slot`），方向是对的；但仍有几类“非 Qt 风格”实现造成复杂度和卡顿风险：

1. 部分动画仍是手写循环/手写状态切换，Qt 动画框架没有完全接管。
2. 存在 `threading + queue + polling QTimer` 的混合模型，事件链路偏绕。
3. 若干 IO/路径逻辑以 `Path/os.environ` 为主，未充分利用 `QStandardPaths/QSaveFile/QResource`。
4. 音频检测存在 COM 轮询在 GUI 线程运行的风险点。

结论：
- **可以进一步“原生化”**，且收益明确。
- 不需要新增第三方库，核心改造可完全依赖 PySide6 已有模块。

## 4. 优先级总览（建议）

### P0（先做，直接影响体验）

1. 轨迹播放完全切到 Qt 动画时间线驱动（减少手写逐帧逻辑）。
2. 热键从“线程 + 队列 + 90ms 轮询”改为 Qt 原生事件过滤。
3. 音频检测/音频输出轮询移出 GUI 线程。

### P1（随后做，提升稳定性/可维护性）

1. `AudioManager` 手写工作线程迁移到 Qt 线程模型（`QThread` 或 `QThreadPool+QRunnable`）。
2. 配置写入改为 `QSaveFile` 原子落盘。
3. 路径体系统一到 `QStandardPaths`。

### P2（中长期，结构优化）

1. `EntityWindow` 动画状态化（`QStateMachine` / 统一 animation group）。
2. 资源分发引入 `QResource`（`:/assets/...`）。
3. 日志体系逐步引入 `QLoggingCategory` 与 Qt 消息链路对齐。

## 5. 详细评审结果

## 5.1 UI 与动画（`src/ui/entity_window.py`, `src/ui/gif_particle.py`, `src/core/director.py`）

### 现状

- `EntityWindow` 中多处动态创建 `QPropertyAnimation/QSequentialAnimationGroup`（peek/enter/summon/roam/probe/flee）。
- `TrajectoryPlayer` 目前通过 `QTimer(16ms)` + 手写插值更新位置 + 手写状态切换阈值。
- `Director` 负责轨迹加载与播放触发。

### 可替换为 Qt 原生能力的点

1. `TrajectoryPlayer` 改为 `QVariantAnimation`（value 取 0~1 进度）或 `QTimeLine` 驱动。
2. GIF 状态切换与轨迹进度绑定到 `QStateMachine` 或“定时事件表 + signal”。
3. `EntityWindow` 多入口动画统一建模（状态机 + 复用 animation group），减少重复构造。

### 预期收益

- 帧节奏交由 Qt 管理，减少手写循环造成的抖动。
- 动画可暂停/恢复/中断控制更清晰。
- 代码结构更易维护和测试。

### 风险

- 迁移阶段要严格比对轨迹复现精度（位置误差、状态切换时机）。

## 5.2 调度与定时（`src/main.py`, `src/ai/gaze_tracker.py`, `src/core/voice_wakeup.py`）

### 现状

- 存在 `time.sleep` 与 Qt 线程并用。
- 全局热键由独立线程采集，再用 `QTimer` 周期拉取队列。

### 可替换为 Qt 原生能力的点

1. `time.sleep` 优先改 `QThread.msleep`（在线程上下文）。
2. 热键改 `QAbstractNativeEventFilter`（或等价 Qt 方案），直接进入 Qt 事件循环。

### 预期收益

- 降低跨模型协作复杂度。
- 减少轮询延迟与事件丢失窗口。

## 5.3 音频与并发（`src/core/audio_manager.py`, `audio_detector.py`, `audio_output_monitor.py`, `voice_wakeup.py`）

### 现状

- `AudioManager` 仍是手写 `threading.Thread + queue + asyncio.run`。
- `audio_detector/audio_output_monitor` 的 COM 检测逻辑可能在 GUI 线程定时执行。

### 可替换为 Qt 原生能力的点

1. `AudioManager` 迁移到 Qt worker 模式：
   - 方案 A：`QThread + QObject worker`
   - 方案 B：`QThreadPool + QRunnable`
2. 音频检测轮询改后台线程执行，结果以 signal 回主线程。
3. `voice_wakeup.py` 中休眠与互斥逐步 Qt 化（可后置）。

### 预期收益

- UI 线程更稳定，减少“偶发卡一下”。
- 并发生命周期管理更统一（start/stop/cleanup）。

## 5.4 路径、配置、资源、日志（`paths.py`, `config_manager.py`, `asset_manager.py`, `main.py`）

### 现状

- 数据目录解析主要依赖 `Path + 环境变量` 手工拼接。
- 配置写入为普通写文件，原子性弱。
- 资源加载以磁盘路径为主。

### 可替换为 Qt 原生能力的点

1. 路径统一到 `QStandardPaths`：
   - AppData: `QStandardPaths.AppDataLocation`
   - Cache: `QStandardPaths.CacheLocation`
   - Config: `QStandardPaths.GenericConfigLocation`（按项目策略）
2. 配置写入改 `QSaveFile` 原子提交（`commit()`）。
3. 可打包资源改 `QResource`（`:/assets/...`）统一访问。
4. 可选引入 `QLoggingCategory` 做分域日志。

### 预期收益

- 打包/部署一致性更好。
- 配置损坏风险降低。
- 资源路径在 exe 与源码模式下行为更统一。

## 6. 可落地改造清单（按文件）

1. `src/ui/gif_particle.py`
- 将 `TrajectoryPlayer` 主循环抽象为 Qt 时间线对象（`QVariantAnimation`）。
- 保留现有 GIF cache，但把状态切换事件化（state event table）。

2. `src/ui/entity_window.py`
- 抽取统一动画工厂与状态机（hidden/peeking/full/roam/probe/flee）。
- 减少临时动画对象的重复创建。

3. `src/core/director.py`
- 轨迹查找路径逐步切到 `QStandardPaths`。
- 轨迹解析支持 `qt.animation.timeline.v1`（已生成样例文件）。

4. `src/main.py`
- 热键链路改为 Qt 原生 native event filter。
- 去掉队列轮询定时器。

5. `src/core/audio_manager.py`
- 任务合成与播放调度迁移 Qt worker。

6. `src/core/audio_detector.py` 与 `src/core/audio_output_monitor.py`
- COM 轮询移后台线程，UI 线程只接收信号。

7. `src/core/paths.py`
- 路径 API 内部统一使用 `QStandardPaths` 返回值。

8. `src/core/config_manager.py`
- 写入改 `QSaveFile`；读取失败分层错误日志。

9. `src/core/asset_manager.py`
- 资源解析优先支持 `QResource` 前缀路径。

## 7. 迁移路线图（建议 4 阶段）

### 阶段 A：体验优先（1~2 周）

- 轨迹播放器 Qt 时间线化。
- 热键 Qt 事件化。
- 音频检测线程隔离。

验收：
- 拖动/召唤/轨迹期间 UI 主线程无明显阻塞。
- 热键响应延迟可观测下降（无 90ms 轮询抖动）。

### 阶段 B：并发重构（1 周）

- `AudioManager` 迁移 Qt worker。

验收：
- 播放链路与合成链路在高频触发下无死锁/卡死。

### 阶段 C：IO/路径/资源（1 周）

- `QStandardPaths + QSaveFile + QResource` 接入。

验收：
- 源码运行与打包运行路径一致。
- 配置写入中断不损坏。

### 阶段 D：状态化收敛（持续）

- `EntityWindow` 动画状态机化、日志分类化。

## 8. 回归测试与观测指标

## 8.1 回归清单

1. 召唤流程：普通召唤、语音召唤、轨迹召唤。
2. 状态切换：state1~state7 的视觉一致性。
3. 音频场景：系统播放/停止、应用自播、并发触发。
4. 热键：后台前台切换、连续按键。
5. 打包后路径：日志、配置、资源读取。

## 8.2 建议指标

- UI tick 抖动：主线程长帧数（>32ms）
- 轨迹误差：相对基准轨迹的位置误差 p95
- 热键延迟：按下到动作触发耗时
- 音频检测耗时：单次轮询耗时分布
- 配置写入失败率/回滚率

## 9. 关键 API 映射（便于实施）

- 动画：`QVariantAnimation`, `QPropertyAnimation`, `QSequentialAnimationGroup`, `QEasingCurve`, `QStateMachine`
- 线程：`QThread`, `QObject.moveToThread`, `QThreadPool`, `QRunnable`, `QMetaObject.invokeMethod`
- 定时：`QTimer`, `QThread.msleep`
- IO：`QFile`, `QSaveFile`, `QTextStream`, `QFileInfo`, `QDir`
- 路径：`QStandardPaths`
- 资源：`QResource` / `:/...`
- 系统事件：`QAbstractNativeEventFilter`
- 日志：`QLoggingCategory`

## 10. 结论

这个项目已经在 Qt 之上运行，但仍有明显空间将“可工作的实现”升级为“Qt 原生范式实现”。建议先做 P0（轨迹动画、热键事件化、音频轮询线程隔离），可以最快降低你现在最关心的卡顿与不丝滑问题；随后再做并发与 IO 的 Qt 化收口，最终把工程稳定性和可维护性整体抬高一个层级。

## 11. 当前落地进度（2026-02-13）

已完成：

1. `P0` 全部完成
- 轨迹播放切为 Qt 时间线驱动（`QVariantAnimation`）。
- 全局热键改为 `QAbstractNativeEventFilter`（移除线程+轮询队列）。
- 音频检测与音频输出轮询改为 Qt worker 线程执行，UI 线程只收信号。

2. `P1` 部分完成
- 路径体系：`paths.py` 已优先使用 `QStandardPaths`（含回退）。
- 配置持久化：`ConfigManager.save` 已使用 `QSaveFile` 原子写入。
- 音频调度：`AudioManager` 已迁移为 Qt worker 线程模型（`QThread + QObject worker`），移除主路径上的手写 `threading + PriorityQueue` 轮转。

3. `P2` 主要项完成
- 资源路径兼容：`AssetManager`、`EntityWindow`、`TrajectoryPlayer` 已支持 `:/...` Qt 资源路径读取。
- 动画状态机化：`EntityWindow` 已引入 `QStateMachine` 管理 `hidden / peeking / engaged / fleeing / roaming / probing` 阶段，并在主要动画入口与回调中统一状态迁移。
- 动画对象复用：`EntityWindow` 的 `summon` 动画序列已改为可复用 `QSequentialAnimationGroup`，减少重复构建。

4. 可观测性增强
- 日志桥接：已在 `setup_logger` 中安装 Qt 消息处理桥接（`qInstallMessageHandler`），Qt 侧日志（含多媒体/插件层）会进入 `app.log`。

待完成（建议下一阶段）：

1. `P2` 可选增强
- 继续把 `EntityWindow` 的临时动画对象构建收敛为可复用 animation group（非必须，属于维护性优化）。
- 日志分类化（`QLoggingCategory`）与可观测性增强。
