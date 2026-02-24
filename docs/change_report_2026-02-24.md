# Aemeath 改动报告（2026-02-24）

## 1. 本轮目标与范围

本轮改动以“稳定性修复 + 主入口解耦 + 并发安全 + 运行时行为一致性”为目标，覆盖如下范围：

- 运行时行为修复（热键、空闲检测、屏幕评论并发、Presence 判定）。
- 架构拆分（`main.py` 的 hotkey/voice 逻辑下沉到 `core`）。
- 日志与依赖治理（`print` -> logger；测试依赖分离）。
- 测试补强（新增并发/语音/热键/Presence 等回归测试）。

不在本轮范围内：

- UI 大规模重构（`settings_dialog.py`、`entity_window.py` 的深度拆分）。
- 全仓导入模式彻底统一（双重导入仅做局部收敛，未全量清除）。


## 2. 代码改动总览（按模块）

### 2.1 输入与热键链路

涉及文件：
- `src/main.py`
- `src/core/hotkey_manager.py`（新增）
- `src/ui/settings_dialog.py`
- `src/core/voice_wakeup.py`
- `tests/test_hotkey_manager.py`（新增）

改动内容：
- 全局 PTT 热键从裸 `B` 改为 `Ctrl+B`。
- 热键注册/注销与 Windows native event filter 从 `main.py` 抽离到 `core/hotkey_manager.py`。
- 设置页、启动提示、错误文案同步改为 `Ctrl+B`。

业务影响：
- 明显行为变更：不会再拦截用户在其他应用中输入普通 `B`，避免打字冲突。
- 使用门槛变化：用户需要按 `Ctrl+B` 才能触发单次转写。


### 2.2 语音运行时链路（PTT + Wakeup）

涉及文件：
- `src/main.py`
- `src/core/voice_runtime.py`（新增）
- `tests/test_voice_runtime.py`（新增）

改动内容：
- 将 `main.py` 中 PTT 与唤醒监听生命周期管理下沉到 `VoiceRuntimeController`：
  - `start_push_to_talk_once()`
  - `start_voice_listener()`
  - `stop_voice_listener()`
- 保留原业务语义（唤醒词命中、屏幕意图触发、降级通知、debug 转写提示），但主入口只负责编排。

业务影响：
- 预期无功能倒退：用户侧语音行为应与之前一致（除热键从 `B` 到 `Ctrl+B`）。
- 工程收益：后续语音链路改造不再需要在 `main.py` 大块改动，降低回归风险。


### 2.3 屏幕评论并发与线程安全

涉及文件：
- `src/core/director.py`
- `src/ai/screen_commentator.py`
- `tests/test_director_screen_commentary_threading.py`（新增）
- `tests/test_director_screen_commentary_request.py`（新增）
- `tests/test_screen_commentator.py`（扩展）

改动内容：
- `Director.request_screen_commentary()`：
  - 从 `threading.Thread` 改为 `QThread + QObject worker`。
  - GUI 状态切换统一通过主线程信号派发，避免后台线程直接触碰 UI。
  - 生命周期回收（done/failed/skipped）统一收口，防止状态计数漂移。
- `ScreenCommentator.comment_on_screen_sync()`：
  - 从“每次 `asyncio.run()` 新建事件循环”改为“复用后台事件循环线程”。
  - 增加 `shutdown()`，在 `Director.shutdown()` 时显式收尾。

业务影响：
- 并发策略变更：当已有屏幕评论任务进行中时，新请求会被跳过（不再允许叠加重入）。
- 用户体感：屏幕评论触发更稳，减少偶发 UI 状态错乱/线程相关问题。


### 2.4 Presence 判定模型（帧数 -> 时间归一化）

涉及文件：
- `src/core/presence_detector.py`
- `src/main.py`
- `src/core/director.py`
- `tests/test_presence_detector.py`（扩展）

改动内容：
- `PresenceDetector` 增加基于时间累计的 absent/still/dark 窗口判断。
- 引入 `target_fps` 与样本时间归一化（含 runtime FPS 更新接口 `set_target_fps`）。
- `main` 初始化时传入 `vision.target_fps`，`Director.apply_runtime_config()` 更新时同步。

业务影响：
- 行为改进：在摄像头实际 FPS 偏低/波动时，Presence 判定不再因“帧计数拉长”而明显漂移。
- 预期效果：减少“同样时长、不同机器/负载下判定不一致”的问题。


### 2.5 空闲时间计算修复（Windows Tick64）

涉及文件：
- `src/core/idle_monitor.py`
- `tests/test_idle_monitor.py`（重写为单元测试）

改动内容：
- 修复 `GetTickCount64()` 后错误截断为 32 位的问题。
- 引入 64/32 对齐与回绕处理逻辑，避免长 uptime 下的 idle 计算异常。

业务影响：
- 长时间运行机器上 idle 判定稳定性提升，减少异常跳变。


### 2.6 配置迁移逻辑下沉

涉及文件：
- `src/core/config_manager.py`
- `src/main.py`
- `tests/test_config_manager_phase4.py`（扩展）

改动内容：
- 将 legacy ASR/LLM 迁移逻辑从 `main.py` 下沉到 `ConfigManager`：
  - `migrate_legacy_asr_defaults`
  - `migrate_legacy_llm_defaults`
  - `migrate_legacy_defaults`
- `main.py` 保留兼容包装函数，调用迁移入口统一化。

业务影响：
- 对用户无直接功能变化。
- 对工程有正向影响：迁移规则聚合在配置层，避免入口层堆积业务细节。


### 2.7 日志与依赖治理

涉及文件：
- `src/core/state_machine.py`
- `src/core/gif_state_mapper.py`
- `src/core/director.py`
- `src/ui/gif_particle.py`
- `requirements.txt`
- `requirements-dev.txt`（新增）
- `README.md`
- `AGENTS.md`

改动内容：
- 多处 `print` 统一为项目 logger。
- `pytest` 从运行时依赖移至开发依赖文件 `requirements-dev.txt`。
- 文档更新测试安装命令。

业务影响：
- 日志可观测性提升（统一进 app log）。
- 发行包更干净（测试依赖不再混入运行时依赖）。


## 3. 用户可感知的产品行为变化（重点）

1) **PTT 热键变更：`B` -> `Ctrl+B`**  
- 这是明确的用户行为变更。  
- 目的：避免破坏用户正常打字输入。

2) **屏幕评论并发门控收紧**  
- 同时只允许一个屏幕评论任务。  
- 新请求在已有任务进行中会被跳过。  
- 目的：避免重入造成状态错乱和线程竞争。

3) **Presence 判定更“按时间”一致**  
- 低 FPS/波动 FPS 下，不再因为帧数采样稀疏导致判定延迟过大。  
- 目的：跨机器、跨负载场景行为更一致。

其余改动主要是稳定性与工程性，不改变用户功能入口。


## 4. 风险与兼容性说明

- `Ctrl+B` 与少数应用快捷键冲突时，仍可能注册失败（已保留失败告警与降级提示）。
- 屏幕评论并发改为严格单任务，极端情况下用户连续触发会看到“跳过”而非排队。
- Presence 时间归一化引入了 sample 时间估算，已通过单测覆盖低 FPS 场景，但建议继续收集线上日志观察阈值灵敏度。


## 5. 测试与验证证据

本地执行：

```powershell
python -m pytest tests/ -v --tb=short
```

结果：
- `151 passed, 7 skipped`

新增/强化测试包括：
- `tests/test_director_screen_commentary_threading.py`
- `tests/test_director_screen_commentary_request.py`
- `tests/test_presence_detector.py`（低 FPS 时间归一化）
- `tests/test_hotkey_manager.py`
- `tests/test_voice_runtime.py`
- `tests/test_screen_commentator.py`（sync loop 复用）
- `tests/test_idle_monitor.py`（Tick64 逻辑）


## 6. 交付结论

本轮以“稳定性与可维护性优先”为策略，已完成关键风险点修复并通过全量测试。  
建议后续继续推进：
- `main.py` 其余菜单/设置编排继续模块化；
- 全仓双重导入模式逐步收敛；
- 针对屏幕评论/Presence 线上日志做一轮阈值复盘。
