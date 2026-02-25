📋 OpenCV 与摄像头视觉功能 Review 报告
—— 设计初衷 vs 实际实现 vs 缺失分析
审查日期: 2026-02-24
审查范围:

src/ai/gaze_tracker.py
,

src/core/presence_detector.py
,

src/core/director.py
,

src/core/gif_state_mapper.py
,

src/core/mood_system.py
,

src/core/state_machine.py
,

src/core/config_manager.py
,

config.json
,

docs/phrase4.MD

1. 你的设计初衷（原始构想）
   根据

docs/phrase4.MD
文档和项目整体架构，你最初对 OpenCV + 摄像头的构想包含以下核心功能：

# 功能构想 技术方案 预期效果

1 人脸检测 MediaPipe Face Mesh 468 关键点 知道用户是否在电脑前
2 视线跟随 鼻尖坐标归一化 → ASCII 眼球偏移 角色的眼睛跟着你移动
3 存在检测 摄像头 + 键鼠融合判定 区分"真的离开"和"在看视频"
4 表情识别 面部关键点几何分析 识别开心/生气/难过/中性
5 表情驱动行为 表情 → GIF 状态映射 → 角色做特定动作 用户笑了角色也开心，用户难过角色安慰
6 无人值守行为 ABSENT 状态 → 角色进入深度睡眠 用户离开后角色缩成一团睡觉
7 被动在场行为 PRESENT_PASSIVE → 角色安静陪伴 用户看视频时角色安静待着不打扰 2. 实际实现情况逐项审查
✅ 2.1 人脸检测 — 已实现
文件:

src/ai/gaze_tracker.py
L62-126

实现方式: OpenCV 打开摄像头 → MediaPipe FaceMesh 检测人脸 → 通过

gaze_updated
信号发送

GazeData

OpenCV 角色: 仅用于 VideoCapture（打开摄像头）、cvtColor（BGR→RGB 颜色转换）、CAP_PROP 设置分辨率。OpenCV 没有参与任何实际的"检测"工作，所有检测都由 MediaPipe 完成。

状态: ✅ 功能完整，但 OpenCV 仅当"摄像头驱动"，未参与视觉处理。

⚠️ 2.2 视线跟随 — 已实现但仅限 ASCII 模式
文件:

src/core/director.py
L772-775, L556-563

python
def \_apply_current_gaze(self, ascii_template: str) -> str:
if self.\_ascii_renderer is None or not self.\_eye_tracking_enabled:
return ascii_template
return self.\_ascii_renderer.apply_eye_tracking(
ascii_template, self.\_latest_gaze_data.face_x, eye_width=5
)
关键问题:

phrase4.MD
第 250 行已明确说明：

"当角色使用 GIF Sprite 显示时，视线追踪不生效，因为 GIF 不支持字符级别的动态替换。此时 \_current_ascii_template 为空字符串，

\_on_gaze_updated
方法会跳过渲染更新。"

由于你的项目现在主要使用 GIF 模式而非 ASCII 字符画模式，所以视线跟随实际上在 GIF 模式下是完全无效的。

状态: ⚠️ 代码存在但在当前 GIF 模式下不生效。

⚠️ 2.3 存在检测 — 代码已实现但未被调用
文件:

src/core/presence_detector.py
（完整 46 行）

python
class PresenceDetector:
def determine_presence(self, idle_time_ms, gaze_data) -> PresenceState:
if idle_time_ms < 60_000:
return PresenceState.PRESENT_ACTIVE
if gaze_data is None:
return PresenceState.UNKNOWN
if not gaze_data.face_detected:
self.\_face_absent_count += 1
else:
self.\_face_absent_count = 0
if idle_time_ms >= self.IDLE_THRESHOLD_MS: # 5分钟
if self.\_face_absent_count >= self.FACE_ABSENT_FRAMES: # 30帧
return PresenceState.ABSENT
if gaze_data.face_detected:
return PresenceState.PRESENT_PASSIVE
return PresenceState.UNKNOWN
关键问题: 我全文搜索了

Director
，发现 self.\_presence_detector 虽然在

init
中被初始化（L144），但

determine_presence()
方法从未被任何地方调用。

python

# Director.**init** L144

self.\_presence_detector = presence_detector or PresenceDetector()

# 之后再也没有出现 self.\_presence_detector 的调用

这意味着:

PresenceState.ABSENT（用户真的离开了）→ 从未被判定
PresenceState.PRESENT_PASSIVE（用户在看视频）→ 从未被判定
融合检测矩阵 → 完全未生效
状态: ❌ 代码存在但从未被调用，存在检测功能完全不工作。

⚠️ 2.4 表情识别 — 已实现
文件:

src/ai/gaze_tracker.py
L145-180

python
@staticmethod
def \_estimate_expression(landmarks) -> tuple[str, float]: # 通过面部关键点的几何关系分析表情 # smile_ratio (嘴角宽度/脸宽) → happy # brow_drop (眉毛下降) → angry # corner_drop + brow_raise → sad # 默认 → neutral
实现质量: 纯粹基于硬编码阈值的几何启发式方法，没有使用任何 OpenCV 图像处理能力来增强（如直方图均衡化改善光照、边缘检测辅助特征提取等）。

阈值问题:

smile_ratio >= 0.38 判定开心
brow_drop >= 0.038 判定生气
corner_drop >= 0.010 && brow_raise >= 0.018 判定难过
这些阈值都是手工硬编码的魔法数字，没有自适应能力，在不同人脸、不同光照、不同摄像头距离下可能不准。

状态: ✅ 基本实现，但鲁棒性不足。

⚠️ 2.5 表情驱动行为 — 部分实现
文件:

src/core/director.py

已实现的表情响应链路：

GazeTracker.\_estimate_expression()
→ GazeData(emotion_label, emotion_score)
→ Director.\_on_gaze_updated()
→ Director.\_track_expression_state() ← 投票平滑机制 ✅
→ Director.\_maybe_trigger_sad_comfort() ← 难过安慰 ✅
→ Director.\_maybe_trigger_no_face_test() ← 无人脸检测 ✅
已实现的具体行为:

表情 行为 状态
sad (难过) 说"别难过"（TTS） ✅ 已实现，有 5 分钟冷却
无人脸 说"我暂时看不到你，等你回来" ✅ 已实现，3 秒延迟 + 20 秒冷却
未实现的行为:

表情 设计构想 实际状态
happy (开心) 角色切换到 state6.gif (开心/打招呼) ❌ EXPRESSION_STATE_MAP 定义了映射但

\_track_expression_state()
从未调用

\_set_entity_state()
angry (生气) 角色切换到 state4.gif (害羞/逃跑) ❌ 同上
neutral (中性) 角色保持 state1.gif ❌ 同上
关键代码问题 —

\_track_expression_state()
只做了投票统计和日志，但没有任何代码将表情状态应用到角色视觉上：

python
def \_track_expression_state(self, gaze_data: GazeData) -> None: # ... 投票统计逻辑 ...
self.\_stable_expression = winner
self.\_last_expression_visual_at = now
self.LOGGER.debug("[Vision] Stable expression=%s score=%s", winner, ...) # ❌ 此处缺失: self.\_set_entity_state(self.EXPRESSION_STATE_MAP[winner], as_base=False) # ❌ 也未触发 GIF 粒子效果 # ❌ 也未影响 MoodSystem
状态: ❌ 表情投票机制实现了，但表情→角色动作的最后一步"执行"缺失。

❌ 2.6 无人值守行为（深度睡眠模式）— 未实现

phrase4.MD
中的设计：

ABSENT (用户真的离开了):
├── 等待额外 2 分钟确认
├── 角色进入"深度睡眠"模式:
│ ├── 显示专用 sleep.gif ASCII 动画 (缩成一团)
│ ├── 停留在屏幕角落
│ └── 停止所有主动弹窗对话
└── 用户回来时: 播放"打哈欠醒来"动画
实际情况:

PresenceDetector.determine_presence() 从未被调用 → ABSENT 从未被判定
没有 sleep.gif 资源
没有"深度睡眠"状态的任何视觉或行为代码
没有"用户回来时打哈欠醒来"的唤醒动画
状态: ❌ 完全未实现。

❌ 2.7 被动在场行为（安静陪伴模式）— 未实现

phrase4.MD
中的设计：

PRESENT_PASSIVE (用户在看视频/思考):
├── 角色悄悄滑入, 但不说话
├── 播放"安静陪伴"动画
├── 不触发任何 TTS
└── 30 秒后静默滑出
实际情况:

PRESENT_PASSIVE 从未被判定（

determine_presence()
未被调用）
没有"安静陪伴"动画
系统目前使用"键鼠空闲"作为唯一的用户不在场判断依据
状态: ❌ 完全未实现。

3. OpenCV 利用程度总结
   3.1 OpenCV API 使用清单
   API 用途 文件 行号
   cv2.VideoCapture() 打开摄像头 gaze_tracker.py 73, 76
   cv2.CAP_DSHOW Windows DirectShow 后端 gaze_tracker.py 72
   cv2.CAP_PROP_FRAME_WIDTH 设置宽度 320px gaze_tracker.py 84
   cv2.CAP_PROP_FRAME_HEIGHT 设置高度 240px gaze_tracker.py 85
   cv2.cvtColor(COLOR_BGR2RGB) 颜色空间转换 gaze_tracker.py 107
   总计 5 个 API 调用，全部是"摄像头驱动层"功能，零图像处理功能。

3.2 未使用的 OpenCV 能力
能力 你项目中的应用场景 价值
cv2.resize() 截图缩放（替代当前 PIL） 性能更好
cv2.imencode() 截图压缩为 JPEG（替代当前 PIL） 性能更好
cv2.equalizeHist() 摄像头画面光照均衡，改善表情识别准确度 提升表情识别鲁棒性
cv2.absdiff() 帧间差分检测屏幕/用户是否有变化 辅助存在检测
cv2.calcHist() 画面亮度/色调分析 环境感知（是否关灯）
cv2.mean() 画面平均亮度 判断用户是否在暗光环境
cv2.Canny() 边缘检测辅助屏幕内容分析 辅助 ScreenCommentator
cv2.GaussianBlur() 隐私模糊处理 截图隐私保护 4. 断裂链路图：从摄像头到角色行为
以下是完整的数据流，标注了每个环节的实现状态：

┌──────────────────────────────────────────────────────────────────┐
│ 摄像头 → 角色行为 完整链路 │
├──────────────────────────────────────────────────────────────────┤
│ │
│ [OpenCV VideoCapture] ──→ [帧读取] ──→ [BGR→RGB] │
│ ✅ 已实现 ✅ ✅ │
│ │ │
│ ▼ │
│ [MediaPipe FaceMesh] ──→ [468 关键点] │
│ ✅ 已实现 ✅ │
│ │ │
│ ├──→ [鼻尖坐标归一化] ──→ [face_x/face_y] │
│ │ ✅ ✅ │
│ │ │ │
│ │ ▼ │
│ │ [ASCII 眼球偏移] │
│ │ ⚠️ 仅 ASCII 模式有效，GIF 模式下无效 │
│ │ │
│ ├──→ [表情估算] ──→ [emotion_label + score] │
│ │ ✅ ✅ │
│ │ │ │
│ │ ├──→ [投票平滑] ──→ [stable_expression] │
│ │ │ ✅ ✅ │
│ │ │ │ │
│ │ │ ▼ │
│ │ │ [表情 → GIF 状态切换] │
│ │ │ ❌ 未实现！EXPRESSION_STATE_MAP │
│ │ │ 定义了但从未执行 set_entity_state() │
│ │ │ │
│ │ │ [表情 → MoodSystem 影响] │
│ │ │ ❌ 未实现！表情未影响心情值 │
│ │ │ │
│ │ │ [表情 → GIF 粒子效果] │
│ │ │ ❌ 未实现！GifStateMapper 无表情事件 │
│ │ │ │
│ │ ├──→ [sad 安慰触发] │
│ │ │ ✅ 已实现 (5分钟冷却时间) │
│ │ │ │
│ │ └──→ [无人脸检测触发] │
│ │ ✅ 已实现 (3秒延迟 + 20秒冷却) │
│ │ │
│ └──→ [face_detected] ──→ [PresenceDetector] │
│ ✅ ❌ 未被调用！ │
│ │ │
│ ├→ ABSENT → 深度睡眠模式 │
│ │ ❌ 未实现 │
│ │ │
│ ├→ PASSIVE → 安静陪伴模式 │
│ │ ❌ 未实现 │
│ │ │
│ └→ UNKNOWN → 退回键鼠检测 │
│ ❌ 未实现（因为整个 │
│ Detector 未被调用） │
└──────────────────────────────────────────────────────────────────┘ 5. 配置层面的问题

config.json
中摄像头默认关闭：

json
"vision": {
"camera_enabled": false,
"camera_consent_granted": false,
"camera_index": 0,
"target_fps": 15,
"eye_tracking_enabled": true
}
这意味着即使上述功能都实现了，用户也需要手动在设置中同时打开 camera_enabled 和 camera_consent_granted 才能启用。这是隐私设计的正确做法，但需要在 UI 中有明确的引导。

6. 结论与建议
   6.1 实现完成度评分
   功能 设计文档 代码骨架 功能完整 端到端测试
   摄像头打开/关闭 ✅ ✅ ✅ ⚠️ 默认关闭
   人脸检测 ✅ ✅ ✅ ⚠️ 默认关闭
   视线跟随 (ASCII) ✅ ✅ ✅ ⚠️ GIF 模式无效
   表情识别 ✅ ✅ ⚠️ 仅启发式 ❌
   表情→角色 GIF 状态 ✅ ⚠️ 映射定义了 ❌ 未执行 ❌
   表情→心情系统 隐含 ❌ ❌ ❌
   表情→GIF 粒子效果 隐含 ❌ ❌ ❌
   存在检测 (融合) ✅ 详细 ✅ 完整 ❌ 未被调用 ❌
   ABSENT 深度睡眠 ✅ 详细 ❌ ❌ ❌
   PASSIVE 安静陪伴 ✅ 详细 ❌ ❌ ❌
   用户回来唤醒动画 ✅ ❌ ❌ ❌
   总评: 摄像头相关功能的整体完成度约 30%。基础设施（摄像头打开、人脸检测、表情识别、投票平滑）都有了，但从"感知"到"行为"的最后一公里执行层几乎全部缺失。

6.2 需要补全的关键缺失项（按优先级排序）
P0 — 必须修复（已有代码但断裂的链路）:

在 Director 中调用 PresenceDetector.determine_presence()

位置：

\_on_gaze_updated()
或

\_on_idle_time_updated()
中
将 \_latest_idle_time_ms 和 \_latest_gaze_data 传入
根据返回的

PresenceState
执行对应行为
在

\_track_expression_state()
末尾添加执行代码

当 stable_expression 变化时，调用

\_set_entity_state(EXPRESSION_STATE_MAP[winner])
同时考虑将表情变化传递给

MoodSystem
（开心 → mood+0.05，生气 → mood-0.05）
P1 — 高价值新功能:

实现 ABSENT 状态行为

新增 sleep.gif 资源
在 Director 中添加 \_enter_deep_sleep() 方法
5 分钟无人脸+无键鼠 → 切换到睡眠 GIF → 停止所有主动行为
实现 PRESENT_PASSIVE 状态行为

角色静默出现在角落，使用 state5.gif（思考/发呆）
不触发任何 TTS
30 秒后自动静默消失
P2 — OpenCV 增强:

摄像头画面预处理（提升表情识别准确度）

python

# 在 gaze_tracker.py 的 run() 循环中加入：

gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
gray = cv2.equalizeHist(gray) # 光照均衡
rgb_frame = cv2.cvtColor(
cv2.merge([gray, gray, gray]), cv2.COLOR_BGR2RGB
)
帧间差分辅助存在检测

python

# 比较连续两帧的差异，用户完全不动则增加 absent 权重

diff = cv2.absdiff(prev_frame, curr_frame)
motion_score = cv2.mean(diff)[0]

# motion_score < 5.0 → 用户可能已离开

环境亮度感知

python
brightness = cv2.mean(cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY))[0]
if brightness < 30: # 用户可能关灯了，角色可以说"该休息了"
6.3 关于 OpenCV 定位的建议
当前项目中 OpenCV 的角色是纯粹的摄像头驱动层（仅 VideoCapture + cvtColor），你安装了 opencv-contrib-python（约 130MB）却只用了不到 1% 的能力。

有两个方向可以选择：

方向 做法 适用场景
方向 A: 减负 将 opencv-python 替换为 opencv-python-headless（30MB），保持当前最小使用 如果你不打算深度使用 OpenCV
方向 B: 深挖 利用 OpenCV 的图像处理能力增强表情识别、存在检测、环境感知 如果你想让角色更"有灵性"
我的建议: 先完成 P0（修复断裂链路），让已有代码真正跑起来。这不需要任何新的 OpenCV API，只需要在 Director 中补全 2 处调用就能让整个"摄像头→角色行为"链路贯通。之后再考虑 P2 的 OpenCV 增强。

文档结束

以上就是完整的 review 文档。核心发现是：你的代码基础设施（感知层）大部分都写好了，但"感知→行为"的最后一步执行缺失。主要断裂点在于：

PresenceDetector.determine_presence() 写好了但从没被调用过

\_track_expression_state()
做了表情投票统计但没有执行角色状态切换
EXPRESSION_STATE_MAP 定义了表情→GIF 的映射关系但从未被使用
这三个问题修复后，你的"摄像头识别用户 → 角色做出反应"的核心功能链路就能贯通了。
