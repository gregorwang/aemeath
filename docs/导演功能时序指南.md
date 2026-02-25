# Director 函数级时序导图（超详细版）

## 0. 这份文档解决什么问题

你已经知道项目用了信号槽和模块通信。  
这份文档继续往下一层，专门回答：

1. `Director` 每个关键函数到底在什么时候被调用。
2. 每条事件链路会改哪些状态。
3. 哪些是同步步骤，哪些是异步回调。
4. 你以后改需求时，第一刀应该落在哪个函数。

这是一份“函数级执行地图”，可以当作读代码和改代码的导航图。

---

## 1. 先看 `Director` 在系统里的位置

文件：`src/core/director.py`（约 1100 行）  
核心角色：行为编排中枢（Orchestrator）

可以把它理解成：

1. 上游：接收事件（idle、音频、托盘、语音、摄像头、定时器）。
2. 中游：状态决策（状态机 + 行为模式 + 配置约束）。
3. 下游：执行动作（显示窗口、播放脚本、触发 GIF 粒子、切换可见性）。

一句话定义：

`Director = 事件汇聚 + 状态约束 + 副作用调度`

---

## 2. 外部接线图（从 `main.py` 到 `Director`）

关键接线点在 `src/main.py`：

1. `Director` 实例创建：`src/main.py:233`
2. 空闲监控绑定：`src/main.py:247-249`
3. 托盘事件连接到 `director`：`src/main.py:529-536`
4. 角色窗口交互连接：`src/main.py:588-589`
5. 退出清理入口：`src/main.py:737`

注意一个重要细节：

1. `Director` 构造参数名是 `audio_output_monitor`（类型标注为 `AudioOutputMonitor`）。
2. 当前 `main.py` 实际注入的是 `AudioDetector`（`src/main.py:226-231` 创建，`src/main.py:244` 注入）。
3. 之所以能工作，是因为 `AudioDetector` 提供了兼容信号接口：
   - `audio_playing_started`
   - `audio_playing_stopped`
   - `audio_state_changed`
   - `start()/stop()`

这属于“鸭子类型接口兼容”。

---

## 3. `Director.__init__` 深拆（启动期一次性时序）

位置：`src/core/director.py:88`

你可以按 9 个阶段看初始化过程：

## 3.1 依赖注入与核心字段

主要保存：

1. UI对象：`_entity_window`
2. 音频对象：`_audio_manager`
3. 资源对象：`_asset_manager`
4. 渲染器：`_ascii_renderer`
5. 可选模块：`_gaze_tracker`、`_screen_commentator`、`_gif_state_mapper`、`_audio_output_monitor`

## 3.2 配置快照与运行参数

主要从 `app_config` 转为内部字段：

1. `_base_idle_threshold_ms`
2. `_jitter_range_seconds`
3. `_auto_dismiss_ms`
4. `_full_screen_pause`
5. `_audio_output_reactive`
6. `_preferred_position`

## 3.3 视觉/情绪相关运行态

例如：

1. `_latest_gaze_data`
2. `_stable_expression`
3. `_expression_votes`
4. `_last_sad_comfort_at`
5. `_no_face_absent_since`

这些字段决定了摄像头驱动的行为触发。

## 3.4 子系统实例准备

内部创建或接入：

1. `_entropy`
2. `_script_engine`
3. `_presence_detector`
4. `_mood_system`
5. `_resource_scheduler`

## 3.5 摄像头信号接线

如果有 `gaze_tracker`：

1. `gaze_updated -> _on_gaze_updated`
2. `camera_error -> _on_camera_error`

## 3.6 状态机构建

`_state_machine = StateMachine(self)`，并注册状态进入/退出回调：

1. `HIDDEN -> _enter_hidden`
2. `PEEKING -> _enter_peeking`
3. `ENGAGED -> _enter_engaged`（退出时 `_stop_auto_dismiss_timer`）
4. `FLEEING -> _enter_fleeing`（退出时 `_stop_auto_dismiss_timer`）

## 3.7 定时器注册

在 `__init__` 中创建并连接了多个定时器：

1. `_auto_dismiss_timer` -> `_on_auto_dismiss_timeout`
2. `_voice_trajectory_timeout` -> `_on_voice_trajectory_timeout`
3. `_mood_decay_timer` -> `mood_system.natural_decay`
4. `_prolonged_idle_timer` -> `_on_prolonged_idle`
5. `_auto_screen_commentary_timer` -> `_on_auto_screen_commentary_timeout`

## 3.8 音频监控接线

如果传入监控器：

1. `audio_playing_started -> _on_audio_output_started`
2. `audio_playing_stopped -> _on_audio_output_stopped`
3. 立即 `start()` 监控

## 3.9 播放回流保护与粒子映射接线

1. `audio_manager.playback_started -> _on_self_playback_started`
2. `audio_manager.playback_finished -> _on_self_playback_finished`
3. `state_machine.state_changed -> _on_state_changed_for_particles`
4. `_sync_auto_screen_commentary_timer()` 启动自动解读节拍

---

## 4. `Director` 的“输入事件矩阵”

你可以把入口按来源分为 8 组。

## 4.1 Idle 事件入口

绑定函数：`bind_idle_monitor`（`src/core/director.py:245`）

接收信号：

1. `user_idle_confirmed` -> `on_user_idle`
2. `user_active_detected` -> `on_user_active`
3. `idle_time_updated` -> `_on_idle_time_updated`

## 4.2 音频输出事件入口

处理函数：

1. `_on_audio_output_started`
2. `_on_audio_output_stopped`

## 4.3 音频播放回流入口（防误判）

处理函数：

1. `_on_self_playback_started`
2. `_on_self_playback_finished`

## 4.4 状态机变更入口

处理函数：`_on_state_changed_for_particles`

## 4.5 定时器入口

处理函数：

1. `_on_auto_dismiss_timeout`
2. `_on_auto_screen_commentary_timeout`
3. `_on_voice_trajectory_timeout`
4. `_on_prolonged_idle`

## 4.6 摄像头/表情入口

处理函数：

1. `_on_gaze_updated`
2. `_on_camera_error`

## 4.7 UI 显式动作入口

公开方法：

1. `summon_now`
2. `toggle_visibility`
3. `request_screen_commentary`
4. `switch_character`
5. `apply_runtime_config`
6. `shutdown`

## 4.8 轨迹播放入口

处理函数：

1. `_try_start_voice_scripted_entrance`
2. `_on_voice_trajectory_finished`
3. `_complete_voice_scripted_entrance`
4. `_cleanup_voice_trajectory_player`

---

## 5. 核心状态模型：双层状态

`Director` 不是单状态，而是“双层状态叠加”。

## 5.1 第一层：实体状态（StateMachine）

定义在 `src/core/state_machine.py`：

1. `HIDDEN`
2. `PEEKING`
3. `ENGAGED`
4. `FLEEING`

合法迁移规则由 `VALID_TRANSITIONS` 控制。

## 5.2 第二层：行为模式（BehaviorMode）

定义在 `src/core/director.py`：

1. `IDLE`
2. `BUSY`
3. `MEDIA_PLAYING`
4. `SUMMONING`

视觉表现由 `_apply_behavior_mode_visual` 映射到 `state1/3/4/6`。

## 5.3 双层模型为什么有用

1. 实体状态控制“位置和生命周期”（是否隐藏、是否逃跑）。
2. 行为模式控制“语义表现”（忙碌、听音乐、召唤中）。

这样能避免把所有语义都挤进单一状态机。

---

## 6. 时序链路 A：`IdleMonitor` 触发用户空闲

入口：`on_user_idle`（`src/core/director.py:254`）

完整链路：

```text
IdleMonitor.user_idle_confirmed.emit()
 -> Director.on_user_idle()
 -> PresenceDetector.determine_presence(...)
 -> _set_behavior_mode(IDLE, apply_visual=False)
 -> StateMachine.transition_to(ENGAGED)
 -> _enter_engaged()
 -> EntityWindow.summon(...) or EntityWindow.enter(...)
 -> AudioManager.play_script(...) (条件满足时)
 -> _auto_dismiss_timer.start(...)
```

关键分支逻辑：

1. 如果正在轨迹播放：直接 return。
2. 如果当前不在 `HIDDEN`：不处理。
3. 如果全屏暂停生效且检测到全屏应用：重置 idle 并返回。
4. `presence_state` 为 `PRESENT_ACTIVE/ABSENT`：回到 BUSY 并重置阈值。
5. `PRESENT_PASSIVE`：进入 `ENGAGED` 并可触发静默互动。

---

## 7. 时序链路 B：用户重新活跃

入口：`on_user_active`（`src/core/director.py:287`）

链路：

```text
IdleMonitor.user_active_detected.emit()
 -> Director.on_user_active()
 -> _set_behavior_mode(BUSY)
 -> if state in (PEEKING, ENGAGED): transition_to(FLEEING)
 -> _enter_fleeing()
 -> EntityWindow.flee()
 -> EntityWindow.flee_completed.emit()
 -> Director._on_flee_finished()
 -> transition_to(HIDDEN)
```

关键点：

1. 这是“打断互动”的收敛链路。
2. 逃跑后会最终归位到 `HIDDEN`。
3. 若状态本来不在互动区，通常只做 monitor 重置。

---

## 8. 时序链路 C：托盘/右键/双击触发显示切换

入口来自 `main.py`：

1. 托盘“立即召唤”：`summon_requested -> _summon_now_or_notify -> director.summon_now()`
2. 托盘“双击切换”：`toggle_requested -> director.toggle_visibility()`
3. 角色窗口双击：`double_clicked -> director.toggle_visibility()`

## 8.1 `summon_now`（`src/core/director.py:345`）

状态分支：

1. `FLEEING`：返回 `False`（拒绝抢占）。
2. `HIDDEN`：尝试 `_try_start_voice_scripted_entrance()`。
3. `PEEKING`：迁移到 `ENGAGED`。
4. `ENGAGED`：刷新自动消失定时器。

## 8.2 `toggle_visibility`（`src/core/director.py:371`）

1. 若 `HIDDEN`：调用 `summon_now()`。
2. 若 `PEEKING/ENGAGED`：调用 `on_user_active()` 触发收起。

---

## 9. 时序链路 D：音频输出开始

入口：`_on_audio_output_started`（`src/core/director.py:567`）

```text
AudioDetector/AudioOutputMonitor.audio_playing_started.emit()
 -> Director._on_audio_output_started()
 -> 若 HIDDEN: summon_now() 让角色显现
 -> _audio_output_active = True
 -> GifStateMapper.on_audio_started()
 -> _set_behavior_mode(MEDIA_PLAYING)
 -> _apply_behavior_mode_visual() 映射到 state3
```

防护逻辑：

1. 若 `audio_output_reactive` 关闭：不处理。
2. 若 `_self_playback_active`：忽略（避免把自己 TTS 当外部媒体）。
3. 若召唤失败（轨迹错误）：记录并返回。

---

## 10. 时序链路 E：音频输出停止

入口：`_on_audio_output_stopped`（`src/core/director.py:586`）

```text
audio_playing_stopped.emit()
 -> Director._on_audio_output_stopped()
 -> _audio_output_active = False
 -> GifStateMapper.on_audio_stopped()
 -> 如果是“音频强制显示”且当前可见: transition_to(HIDDEN)
 -> 否则回到 IDLE/BUSY
```

为什么有 `_audio_forced_visible`：

1. 某些情况下角色是因“检测到音频”才被强制召唤出来。
2. 音频停了后需要决定是否自动撤回。
3. 这个标志专门解决“触发来源是音频”这一语义差异。

---

## 11. 时序链路 F：播放回流抑制（self playback guard）

入口：

1. `_on_self_playback_started`（`src/core/director.py:607`）
2. `_on_self_playback_finished`（`src/core/director.py:618`）

作用：

1. 防止自身 TTS/脚本播放误触发“媒体播放模式”。
2. 播放开始时：标记 `_self_playback_active = True`，必要时强制退出媒体态。
3. 播放结束时：恢复 `_self_playback_active = False`，若外部媒体真实在播可重新进入媒体态。

---

## 12. 时序链路 G：进入 `ENGAGED` 的完整执行细节

入口函数：`_enter_engaged`（`src/core/director.py:479`）

内部步骤顺序：

1. `_start_camera_tracking_if_needed()`
2. `_set_entity_state("state1")`
3. 选择 idle script（`ScriptEngine` + `AssetManager` 回退）
4. 若窗口不可见：
   - 计算屏幕位置
   - 选择边缘
   - `EntityWindow.summon(...)`
5. 若窗口可见：
   - `EntityWindow.enter(...)`
6. 条件满足时播放脚本音频 `AudioManager.play_script(...)`
7. 若不在 `SUMMONING`，行为模式设为 `IDLE`
8. `_apply_behavior_mode_visual()`
9. `_set_entity_autonomous(True)`
10. 启动 `_auto_dismiss_timer`

这个函数是“进入互动态”的主落点。

---

## 13. 时序链路 H：进入 `FLEEING`

入口函数：`_enter_fleeing`（`src/core/director.py:516`）

步骤：

1. 停止自动消失计时器。
2. 停止摄像头追踪。
3. 关闭实体自治动画。
4. 行为模式设为 `BUSY`。
5. 视觉设 `state4`（害羞/逃离语义）。
6. 播 panic 脚本（若有）。
7. 调 `EntityWindow.flee()`。

收尾在 `_on_flee_finished`：

1. 检查若当前状态仍是 `FLEEING`，则 `transition_to(HIDDEN)`。

---

## 14. 时序链路 I：隐藏态进入逻辑

入口函数：`_enter_hidden`（`src/core/director.py:457`）

关键动作：

1. 停止自动消失定时器。
2. 停止摄像头与 no-face 状态。
3. 禁用实体自治。
4. 调 `EntityWindow.hide_now()`（或 `hide()` 回退）。
5. 重置 idle monitor 并重新抖动阈值。
6. 清理 silent mode / audio forced visible。
7. 启动 `_prolonged_idle_timer`（10 分钟后触发粒子）。
8. 清空 ASCII 模板缓存。
9. 行为模式回 `BUSY`（不立即做视觉，因为已隐藏）。

---

## 15. 时序链路 J：自动屏幕解读

入口 1：手动调用 `request_screen_commentary(source="manual")`  
入口 2：定时器 `_on_auto_screen_commentary_timeout`（`source="timer"`）

`request_screen_commentary` 流程：

1. 组件可用性检查（`screen_commentator` 是否存在）。
2. 若 `source=timer`：
   - 全屏暂停检查
   - 并发计数检查（防止重叠请求）
3. 设实体临时状态为 `state5`
4. 取消已有会话 `cancel_current_session()`
5. `active_count += 1`
6. 启动后台线程 `_worker`
7. `_worker` 内：
   - `resource_scheduler.resolve_plan(...)`
   - 若不允许运行 LLM，走语音提示降级
   - 否则执行 `comment_on_screen_sync(...)`
8. `finally` 中：
   - 恢复实体状态 `state1`
   - `active_count -= 1`

为什么这里用 `threading.Thread` 而不是主线程直接调用：  
屏幕分析 + LLM 请求可能耗时，不可阻塞 UI 事件循环。

---

## 16. 时序链路 K：摄像头数据更新与表情/安慰分支

入口：`_on_gaze_updated`（`src/core/director.py:541`）

处理顺序：

1. 类型校验（必须是 `GazeData`）。
2. 更新 `_latest_gaze_data`。
3. `_maybe_trigger_no_face_test(gaze_data)`
4. `_track_expression_state(gaze_data)`
5. `_maybe_trigger_sad_comfort(gaze_data)`
6. 若启用了眼动渲染且当前可见，则更新 ASCII 眼睛方向。

## 16.1 表情追踪 `_track_expression_state`

机制：

1. 仅在 `PEEKING/ENGAGED + IDLE mode` 生效。
2. 对情绪标签做投票衰减+累加，避免抖动。
3. 得票超过阈值才更新稳定表情。
4. 最终映射到 `state1/4/5/6` 之一。

## 16.2 悲伤安慰 `_maybe_trigger_sad_comfort`

触发条件全部满足才生效：

1. 摄像头启用且检测到人脸
2. 标签为 sad 且分数过阈值
3. 当前稳定表情也是 sad
4. 不在逃跑/轨迹播放中
5. 冷却时间已过

动作：

1. `QTimer.singleShot(0, _trigger_sad_comfort)` 异步入队
2. `_trigger_sad_comfort` 中做 TTS 和轨迹召唤尝试

## 16.3 无人脸测试 `_maybe_trigger_no_face_test`

逻辑类似：

1. 追踪“无人脸持续时长”
2. 达到最小时长与冷却条件后触发
3. 同样走 `singleShot(0, _trigger_no_face_test)`

---

## 17. 时序链路 L：状态变化到粒子效果

入口：`_on_state_changed_for_particles`（`src/core/director.py:955`）

映射规则：

1. `new_state in (PEEKING, ENGAGED)` -> `gif_state_mapper.on_engaged()`
2. `new_state == FLEEING` -> `gif_state_mapper.on_fleeing()`
3. `new_state == HIDDEN` -> `gif_state_mapper.on_hidden()`

注意：

1. 这里只是“转发决策”，不直接操作粒子。
2. 真正粒子生成在 `GifStateMapper -> GifParticleManager`。

---

## 18. 时序链路 M：长时 idle 粒子

入口：`_on_prolonged_idle`（`src/core/director.py:967`）

逻辑：

1. 只有当前 `HIDDEN` 才触发。
2. 调 `gif_state_mapper.on_prolonged_idle()`。
3. 触发后再次 `start()` 定时器，形成周期。

这是一条“隐藏态背景动效”链路。

---

## 19. 时序链路 N：剧本轨迹式召唤

主入口：`_try_start_voice_scripted_entrance`（`src/core/director.py:974`）

你可以把它看成 6 步：

## 19.1 轨迹文件解析准备

1. `_resolve_voice_trajectory_path()` 找文件。
2. `_load_trajectory_data(path)` 解析 JSON。
3. 校验 schema（`points` 或 `keyframes`）。

## 19.2 动画资源映射准备

`_build_voice_trajectory_gif_map()` 构建 `state1..7 + aemeath` 映射。

## 19.3 播放前清场

1. 隐藏实体窗口。
2. 停自动消失定时器。
3. 关闭自治。
4. 行为模式设为 `SUMMONING`。

## 19.4 创建轨迹播放器

1. `player = TrajectoryPlayer(...)`
2. `player.finished.connect(_on_voice_trajectory_finished)`
3. 启动 `_voice_trajectory_timeout`
4. `player.start()`

## 19.5 结束回调

`_on_voice_trajectory_finished`：

1. 清理 player 引用与超时计时器。
2. 调 `_complete_voice_scripted_entrance()`。

## 19.6 结束后行为 `_complete_voice_scripted_entrance`

1. `gif_state_mapper.on_summoned()`（若可用）
2. 停自动消失、关自治
3. 行为模式回 `BUSY`
4. 若当前可见态（`PEEKING/ENGAGED`）则迁移到 `HIDDEN`

这条链路强调“轨迹登场后的状态收敛”。

---

## 20. 时序链路 O：运行时配置热更新

入口：`apply_runtime_config`（`src/core/director.py:391`）

这函数很关键，因为它是“设置面板改配置后”的统一应用入口。

主要操作：

1. 覆盖 trigger/behavior/appearance/vision 配置字段。
2. 根据 camera 开关决定启动或停止追踪。
3. 根据 audio reactive 开关决定是否退出媒体态。
4. 若监控器已在播放且符合条件，主动调用 `_on_audio_output_started()` 同步状态。
5. 重新抖动 idle 阈值。
6. 重同步 auto commentary 定时器。

改配置相关需求时，优先看这里。

---

## 21. 时序链路 P：切换角色

入口：`switch_character`（`src/core/director.py:377`）

步骤：

1. 替换 `asset_manager` 与 `script_engine` 来源。
2. 可选替换 `ascii_renderer` 与 voice。
3. 清空待处理脚本缓存。
4. 若当前在 `ENGAGED`，立即按新角色资源刷新视觉。

这保证角色切换无需重启应用。

---

## 22. 时序链路 Q：优雅关闭

入口：`shutdown`（`src/core/director.py:441`）

清理顺序：

1. 停止轨迹播放
2. 停止自动定时器（dismiss/commentary/mood/prolonged idle）
3. 停止摄像头追踪
4. 停音频监控
5. 关闭 gif mapper
6. 关闭实体自治

`main.py` 里由 `app.aboutToQuit.connect(_shutdown)` 触发顶层关闭流程。

---

## 23. `Director` 与 `StateMachine` 的契约

`StateMachine.transition_to`（`src/core/state_machine.py:52`）会做四步：

1. 校验迁移合法性。
2. 调旧状态 `on_exit`。
3. 更新 `current_state`。
4. 调新状态 `on_enter`。
5. 发 `state_changed(old, new)` 信号。

`Director` 的实践价值：

1. 把副作用聚合到 `_enter_*` 方法中。
2. 任何状态切换都能追踪日志和粒子映射。
3. 非法迁移会被拒绝，减少并发交错错误。

---

## 24. `Director` 与 `EntityWindow` 的调用契约

`Director` 主要调用窗口的这些能力：

1. `summon(edge, y, script)`：从隐藏到展示的组合动画。
2. `enter(script)`：从探头进入 fully engaged。
3. `flee()`：逃离并回到隐藏。
4. `hide_now()`：立即隐藏。
5. `set_state_by_name(...)`：切换状态 GIF 语义。
6. `set_ascii_content(...)` 或 `set_sprite_content(...)`：内容渲染。
7. `set_autonomous_enabled(...)`：自治运动开关。

这是一条“中枢调 UI 执行层”的同步调用关系。

---

## 25. `Director` 与 `GifStateMapper` 的调用契约

`Director` 触发映射器的入口：

1. `_on_audio_output_started -> on_audio_started`
2. `_on_audio_output_stopped -> on_audio_stopped`
3. `_on_state_changed_for_particles -> on_engaged/on_fleeing/on_hidden`
4. `_on_prolonged_idle -> on_prolonged_idle`
5. `_complete_voice_scripted_entrance -> on_summoned`

`GifStateMapper` 再调用 `GifParticleManager`：

1. `spawn_particle`
2. `spawn_wave`
3. `dismiss_all`

---

## 26. 同步/异步边界图（按线程和事件循环看）

## 26.1 主要同步段

1. `Director` 内部状态判断与方法调用。
2. `StateMachine.transition_to` 执行 enter/exit 回调。
3. 对 `EntityWindow` 的直接调用动作。

## 26.2 主要异步段

1. IdleMonitor 线程信号 -> Director 槽函数。
2. AudioDetector/AudioOutputMonitor 定时轮询 -> 信号。
3. Screen commentary 后台线程 `_worker`。
4. `QTimer.singleShot` 延迟触发（sad/no-face）。
5. TrajectoryPlayer finished 回调。

工程认知重点：

1. 异步事件进入主线程时序可能交错。
2. `Director` 需要具备幂等和状态防护。
3. 当前代码确实大量采用 early-return 防护分支。

---

## 27. `Director` 里最值得你优先理解的 12 个函数

如果你只想先学最关键 20%，按这个顺序读：

1. `__init__`（接线总览）
2. `bind_idle_monitor`（idle入口）
3. `on_user_idle`（核心触发）
4. `on_user_active`（收敛触发）
5. `summon_now`（显式召唤入口）
6. `_enter_engaged`（展示主逻辑）
7. `_enter_fleeing`（撤退主逻辑）
8. `_on_audio_output_started`（媒体触发）
9. `_on_audio_output_stopped`（媒体结束）
10. `_on_gaze_updated`（视觉复合触发）
11. `_try_start_voice_scripted_entrance`（复杂链路）
12. `shutdown`（生命周期闭环）

---

## 28. 你未来改需求时的定位策略

这里给你 8 种常见需求和首看函数。

需求 A：改“多久自动收起”  
看：`_auto_dismiss_ms`、`_enter_engaged`、`_on_auto_dismiss_timeout`

需求 B：改“音频播放时角色行为”  
看：`_on_audio_output_started`、`_on_audio_output_stopped`、`_set_behavior_mode`

需求 C：改“idle 判定策略”  
看：`bind_idle_monitor`、`on_user_idle`、`_arm_idle_threshold_with_jitter`

需求 D：改“托盘点击行为”  
看：`main.py` 连接 + `Director.toggle_visibility/summon_now`

需求 E：改“表情映射规则”  
看：`EXPRESSION_STATE_MAP`、`_track_expression_state`

需求 F：改“屏幕解读频率/并发控制”  
看：`_sync_auto_screen_commentary_timer`、`request_screen_commentary`

需求 G：改“轨迹召唤文件查找规则”  
看：`_resolve_voice_trajectory_path`

需求 H：改“退出顺序”  
看：`shutdown` + `main.py _shutdown`

---

## 29. 风险点清单（函数级）

## 29.1 高风险函数

1. `on_user_idle`：入口高频、分支多。
2. `_enter_engaged`：副作用密集（UI/音频/计时器）。
3. `_on_audio_output_started/stopped`：涉及外部事件抖动。
4. `_on_gaze_updated`：多条件复合触发。
5. `_try_start_voice_scripted_entrance`：I/O + UI + timeout 联动。

## 29.2 修改时必做自检

1. 有没有破坏状态机合法迁移？
2. 有没有让同一事件重复触发副作用？
3. 是否影响 `_audio_forced_visible` 收敛逻辑？
4. 计时器是否有 start/stop 配对？
5. 异常分支是否会留下脏状态？

---

## 30. 测试建议（围绕 Director）

优先补的测试集合：

1. `summon_now` 在不同状态下的返回值和副作用。
2. `on_user_active` 是否在可见态触发 `FLEEING`。
3. `_on_audio_output_started/stopped` 在 `_self_playback_active` 条件下是否正确抑制。
4. `_on_state_changed_for_particles` 映射是否完整。
5. `_on_auto_dismiss_timeout` 是否强制收敛到隐藏。
6. `apply_runtime_config` 对功能开关的热切换是否正确。

如果写单测困难，先用“桩对象 + 事件记录器”模拟：

1. 假 `entity_window` 记录被调用方法。
2. 假 `audio_manager` 记录播放请求。
3. 假 `gif_state_mapper` 记录映射调用。

---

## 31. 常见误解纠正

误解 1：`Director` 只控制 UI。  
纠正：它同时控制状态机、音频、视觉触发、定时器和轨迹生命周期。

误解 2：音频监控就是一个类固定实现。  
纠正：当前是接口兼容策略，`AudioDetector` 和 `AudioOutputMonitor` 都可接入。

误解 3：只要信号槽连上就不会有并发问题。  
纠正：信号槽简化线程通信，但状态竞争和时序交错仍要靠状态机与分支防护解决。

误解 4：`summon_now` 一定会立刻显示。  
纠正：若处于 `FLEEING` 或轨迹文件无效，可能失败或被拒绝。

---

## 32. 函数级阅读练习（建议 3 轮）

## 32.1 第一轮：只跟“idle 到出现再到隐藏”

从这条链开始：

1. `bind_idle_monitor`
2. `on_user_idle`
3. `_enter_engaged`
4. `_on_auto_dismiss_timeout`
5. `_enter_fleeing`
6. `_on_flee_finished`
7. `_enter_hidden`

## 32.2 第二轮：只跟“音频触发”

1. `_on_audio_output_started`
2. `_set_behavior_mode`
3. `_apply_behavior_mode_visual`
4. `_on_audio_output_stopped`
5. `_audio_forced_visible` 分支

## 32.3 第三轮：只跟“轨迹召唤”

1. `summon_now`
2. `_try_start_voice_scripted_entrance`
3. `_resolve_voice_trajectory_path`
4. `_on_voice_trajectory_finished`
5. `_complete_voice_scripted_entrance`

每轮完成后写 10 行总结，你会很快建立函数级全局感。

---

## 33. 迷你速查表（你改代码时可直接看）

### 33.1 公开入口（通常是产品功能入口）

1. `bind_idle_monitor`
2. `on_user_idle`
3. `on_user_active`
4. `request_screen_commentary`
5. `summon_now`
6. `toggle_visibility`
7. `switch_character`
8. `apply_runtime_config`
9. `get_status_summary`
10. `shutdown`

### 33.2 状态迁移入口

1. `_enter_hidden`
2. `_enter_peeking`
3. `_enter_engaged`
4. `_enter_fleeing`
5. `_on_flee_finished`

### 33.3 音频相关入口

1. `_on_audio_output_started`
2. `_on_audio_output_stopped`
3. `_on_self_playback_started`
4. `_on_self_playback_finished`

### 33.4 视觉相关入口

1. `_on_gaze_updated`
2. `_track_expression_state`
3. `_maybe_trigger_sad_comfort`
4. `_maybe_trigger_no_face_test`

### 33.5 轨迹相关入口

1. `_try_start_voice_scripted_entrance`
2. `_on_voice_trajectory_finished`
3. `_on_voice_trajectory_timeout`
4. `_complete_voice_scripted_entrance`
5. `_resolve_voice_trajectory_path`
6. `_load_trajectory_data`

---

## 34. 一份“改 Director 前 90 秒检查清单”

1. 你改的是入口层、状态层，还是副作用层？
2. 当前分支有没有对 `HIDDEN/PEEKING/ENGAGED/FLEEING` 全覆盖？
3. 行为模式变化后是否要同步视觉？
4. 是否影响 `_audio_forced_visible`、`_self_playback_active` 这两个保护变量？
5. 定时器是否需要重置、暂停或重启？
6. 是否要更新粒子映射行为？
7. 异常分支会不会遗漏清理？
8. 退出路径 `shutdown` 会不会受影响？

---

## 35. 结语：把 Director 读懂后你会获得什么

当你真正读懂这一个文件，你会同时获得：

1. Qt 桌面应用事件编排能力。
2. 多信号源融合下的状态机思维。
3. 同步/异步混合系统的风险意识。
4. “先定位入口再改动”的工程化习惯。

你之后再看同类项目，基本都能快速拆出：

1. 事件入口
2. 决策中枢
3. 执行末端
4. 生命周期闭环

这就是你从“会写功能”到“会控系统复杂度”的关键一步。

