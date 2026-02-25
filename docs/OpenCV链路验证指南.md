# OpenCV 链路修复验证手册（P0 + P1 + 高/中优先）

本文档用于回答两个问题：
1. 现在的触发逻辑是什么（你做了什么会触发什么）？
2. 我该怎么验证这些改动是生效的？

## 1. 改动总览

本轮已落地：

1. P0：
- `PresenceDetector.determine_presence()` 正式接入 `Director`（idle/gaze 双入口）。
- 表情稳定投票结果会驱动角色状态切换（`EXPRESSION_STATE_MAP`）并影响 mood。

2. P1：
- `PRESENT_PASSIVE`：安静陪伴 30 秒，自动静默消失。
- `ABSENT`：进入深睡逻辑（优先 `sleep` 状态，回退 `state5`）。
- 从深睡/被动回到 `PRESENT_ACTIVE`：执行唤醒恢复（`state2`，恢复自治与自动 dismiss）。

3. 高优先（PresenceDetector 增强）：
- 将 `motion_score`（帧间差分）和 `brightness`（亮度）接入缺席判断。

4. 中优先（截图编码性能）：
- `ScreenCommentator._image_to_base64()` 改为 OpenCV 优先：
  - `cv2.resize`
  - `cv2.imencode`
- OpenCV 不可用时自动回退 PIL。

## 2. 触发条件矩阵（输入条件 -> 系统行为）

### 2.1 PresenceDetector 判定层

输入来源：
- `idle_time_ms`（来自 IdleMonitor）
- `gaze_data.face_detected`
- `gaze_data.motion_score`
- `gaze_data.brightness`

核心阈值：
- `idle_time_ms < 60_000` -> `PRESENT_ACTIVE`
- 长 idle 判定起点：`idle_time_ms >= 300_000`（5 分钟）
- `FACE_ABSENT_FRAMES = 30`
- `STILL_NO_FACE_FRAMES = 20`（`motion_score <= 5.0`）
- `DARK_NO_FACE_FRAMES = 24`（`brightness <= 30.0`）

判定逻辑（长 idle 前提下）：
1. 无人脸帧数 >= 30 -> `ABSENT`
2. 低运动 + 无人脸累计 >= 20 -> `ABSENT`
3. 暗光 + 无人脸累计 >= 24 -> `ABSENT`
4. 否则如果有人脸 -> `PRESENT_PASSIVE`
5. 其他 -> `UNKNOWN`

### 2.2 Director 行为层

`_apply_presence_state(state)` 的行为：

1. `PRESENT_ACTIVE`
- 如果此前在深睡/被动：执行恢复
- 深睡恢复时：
  - 切 `state2`（唤醒感）
  - 开启自治
  - 重新启动 auto-dismiss 计时器

2. `PRESENT_PASSIVE`
- 进入安静陪伴：
  - 若当前 `HIDDEN`，先静默进入 `ENGAGED`（不播脚本）
  - 关闭自治
  - 切 `state5`
  - 启动 30 秒计时器
- 30 秒到时：自动转回 `HIDDEN`

3. `ABSENT`
- 进入深睡：
  - 停止被动计时器
  - 关闭自治
  - 设行为为 `BUSY`（避免主动行为）
  - 切 `sleep` 状态；若角色资源无 `sleep`，回退 `state5`

4. 用户主动活动（`on_user_active`）
- 若当前处于被动或深睡：直接隐藏（不走 flee/panic 音频）。

### 2.3 表情链路

稳定表情触发条件（`_track_expression_state`）：
1. 当前状态在 `PEEKING/ENGAGED`
2. 有效行为模式为 `IDLE`
3. 投票赢家分值 >= 3
4. 与上次视觉更新满足节流条件

触发结果：
- 视觉：`happy->state6`, `neutral->state1`, `angry->state4`, `sad->state5`
- mood：`happy +0.05`, `angry -0.05`, `sad -0.03`

## 3. 手动验证步骤（黑盒）

## 3.1 前置

在设置中开启：
1. `vision.camera_enabled = true`
2. `vision.camera_consent_granted = true`

然后启动应用：
```powershell
python src/main.py
```

## 3.2 验证 PASSIVE（安静陪伴）

1. 让系统进入“长 idle”场景（>=5 分钟）。
2. 保持人脸在摄像头内。
3. 预期：
- 角色进入安静陪伴（`state5` 风格）
- 不主动播报
- 约 30 秒后自动静默消失（转 `HIDDEN`）

## 3.3 验证 ABSENT（深睡）

1. 同样先满足“长 idle”。
2. 离开镜头或遮挡人脸，保持一段时间。
3. 预期：
- 角色进入深睡表现（优先 `sleep`，无该状态则 `state5`）
- 主动行为被抑制

## 3.4 验证从深睡恢复

1. 在深睡状态下重新回到镜头并触发活动。
2. 预期：
- 出现唤醒状态（`state2`）
- 恢复自治和 auto-dismiss 计时

## 3.5 验证截图编码链路（中优先）

触发一次屏幕解说（手动或定时）：
1. 预期功能正常返回解说
2. 编码路径优先 OpenCV；OpenCV 不可用时自动回退 PIL（功能不中断）

## 4. 自动化验证（白盒）

执行：
```powershell
python -m pytest tests/ -q
```

当前基线结果：
- `129 passed, 7 skipped`

与本次链路直接相关的关键用例：
1. `tests/test_director_cv_chain.py`
2. `tests/test_director_presence_p1.py`
3. `tests/test_presence_detector.py`
4. `tests/test_screen_commentator.py`
5. `tests/test_gaze_tracker.py`

## 5. 关键文件索引

1. `src/core/director.py`
2. `src/core/presence_detector.py`
3. `src/ai/gaze_tracker.py`
4. `src/ai/screen_commentator.py`
5. `src/core/mood_system.py`
