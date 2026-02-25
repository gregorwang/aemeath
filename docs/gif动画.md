# GIF 动画轨迹系统修复方案

> 本文档描述了当前轨迹动画系统的三个关键问题及其修复方案。
> 修改涉及的文件：`src/core/director.py`、`src/tools/trajectory_to_qt_timeline.py`、`src/tools/trajectory_recorder.py`、`src/ui/gif_particle.py`
> 以及 `recorded_paths/` 目录下的轨迹文件。

---

## 修复 1：接入 `_try_start_voice_scripted_entrance` —— 轨迹播放死代码修复

### 问题描述

`Director` 类中定义了 `_try_start_voice_scripted_entrance()` 方法（director.py 第1585行），该方法实现了完整的"剧本式登场"功能：
- 从 `recorded_paths/` 加载轨迹 JSON
- 创建 `TrajectoryPlayer` 实例
- 播放轨迹动画（角色沿路径移动 + 切换 GIF 状态）
- 播放完成后转入 HIDDEN 状态

**但这个方法从未被任何地方调用。** 它是死代码。同时：
- `VOICE_TRAJECTORY_FILE = "trajectory_1771029879_qt_animation.json"` 常量虽然定义了，但永远不会被使用
- `_resolve_voice_trajectory_path()` 方法的复杂路径搜索逻辑也永远不会执行
- `_on_voice_trajectory_finished` 和 `_on_voice_trajectory_timeout` 回调虽然写好了，但因为没有入口点所以也是死代码

### 修复方案

需要把 `_try_start_voice_scripted_entrance()` 接入到合适的触发点。根据代码分析，有以下几个应该触发轨迹动画的场景：

#### 方案 A：作为 `summon_now()` 的增强（推荐）

在 `Director.summon_now()` 方法中，当从 HIDDEN 状态被召唤时，**尝试先使用轨迹登场**，如果没有轨迹文件才 fallback 到普通的 `transition_to(ENGAGED)`。

修改 `Director.summon_now()` 方法（director.py 第552-567行）：

```python
def summon_now(self) -> bool:
    """Force summon regardless of idle threshold."""
    state = self._state_machine.current_state
    if state == EntityState.FLEEING:
        return False
    if state == EntityState.HIDDEN:
        self._set_behavior_mode(BehaviorMode.IDLE, apply_visual=False)
        # 尝试剧本式登场，失败则 fallback 到普通登场
        try:
            if self._try_start_voice_scripted_entrance():
                return True
        except ScriptedEntranceError:
            self.LOGGER.info("[Summon] 剧本式登场不可用，使用普通登场。")
        return self._state_machine.transition_to(EntityState.ENGAGED)
    if state == EntityState.PEEKING:
        return self._state_machine.transition_to(EntityState.ENGAGED)
    if state == EntityState.ENGAGED:
        self._auto_dismiss_timer.start(self._auto_dismiss_ms)
        return True
    return False
```

#### 方案 B：只在特定条件下使用轨迹登场

如果不希望每次召唤都走轨迹，可以加一个配置开关，比如在 `config` 中新增：

```python
# config_manager.py 中增加
@dataclass
class BehaviorConfig:
    # ... 现有字段 ...
    scripted_entrance_enabled: bool = False  # 是否使用剧本式登场轨迹
```

然后在 `summon_now()` 中根据配置决定是否尝试轨迹登场。

#### 方案 C：增加调试入口

在右键菜单和托盘菜单中增加一个"调试轨迹登场"按钮，类似现有的"调试空闲入侵"、"调试悲伤安慰"等：

在 `Director` 类中增加：
```python
def trigger_trajectory_entrance_debug(self, *, source: str = "manual") -> bool:
    source_name = (source or "manual").strip().lower() or "manual"
    if self._voice_trajectory_playing:
        self.LOGGER.info("[TrajectoryEntrance] Debug trigger skipped: already playing source=%s", source_name)
        return False
    if self._state_machine.current_state == EntityState.FLEEING:
        self.LOGGER.info("[TrajectoryEntrance] Debug trigger skipped: state=fleeing source=%s", source_name)
        return False
    try:
        return self._try_start_voice_scripted_entrance()
    except ScriptedEntranceError as exc:
        self.LOGGER.warning("[TrajectoryEntrance] Debug trigger failed source=%s: %s", source_name, exc)
        return False
```

在 `main.py` 中注册到右键菜单和托盘菜单（参照现有的 `_trigger_idle_invasion_debug` 模式）。

### 推荐

**建议同时实施方案 A + C**：方案 A 让正常流程可以使用轨迹，方案 C 让开发者可以随时调试验证。方案 B 的配置开关也可以加上，但不是必须的。

---

## 修复 2：轨迹平滑处理 —— 消除手绘抖动和运动生硬问题

### 问题描述

当前轨迹录制和播放存在三个质量问题：

1. **手绘抖动**：鼠标录制的轨迹有微小的随机抖动（因为人手不稳），导致角色移动时有明显的颤动
2. **线性插值**：`trajectory_to_qt_timeline.py` 的 `_interpolate_points()` 函数只做简单线性插值，没有曲线平滑，运动看起来是"折线"而不是"弧线"
3. **断点空白**：多段录制时断点之间有几秒的空白（如 `trajectory_1771029879.json` 中第一段结束于 0.89s，第二段开始于 7.89s，中间有 7 秒空白），播放时角色会在原地卡住不动

### 修复方案

#### 2.1 在 `trajectory_to_qt_timeline.py` 中增加路径平滑

在 `_interpolate_points()` 之前，对原始点做 **Catmull-Rom 样条平滑**。这是最适合路径平滑的算法：给定 4 个控制点，在中间两个点之间生成平滑曲线。

在 `trajectory_to_qt_timeline.py` 中增加新函数：

```python
def _smooth_points_catmull_rom(
    points: list[dict[str, float | int]],
    subdivisions: int = 4,
) -> list[dict[str, float | int]]:
    """
    对原始路径点做 Catmull-Rom 样条插值，平滑手绘抖动。
    
    subdivisions: 每两个原始点之间插入多少个中间点（越大越平滑，也越慢）。
    建议值 3-6。
    """
    if len(points) < 3:
        return points

    smoothed: list[dict[str, float | int]] = []
    n = len(points)

    for i in range(n - 1):
        p0 = points[max(i - 1, 0)]
        p1 = points[i]
        p2 = points[min(i + 1, n - 1)]
        p3 = points[min(i + 2, n - 1)]

        for sub in range(subdivisions):
            t = sub / subdivisions
            t2 = t * t
            t3 = t2 * t

            # Catmull-Rom 样条公式
            x = 0.5 * (
                (2 * p1["x"])
                + (-p0["x"] + p2["x"]) * t
                + (2 * p0["x"] - 5 * p1["x"] + 4 * p2["x"] - p3["x"]) * t2
                + (-p0["x"] + 3 * p1["x"] - 3 * p2["x"] + p3["x"]) * t3
            )
            y = 0.5 * (
                (2 * p1["y"])
                + (-p0["y"] + p2["y"]) * t
                + (2 * p0["y"] - 5 * p1["y"] + 4 * p2["y"] - p3["y"]) * t2
                + (-p0["y"] + 3 * p1["y"] - 3 * p2["y"] + p3["y"]) * t3
            )
            # 时间也做线性插值
            time_val = p1["t"] + (p2["t"] - p1["t"]) * t
            # 状态取当前段的状态
            state = int(p1.get("s", 1))

            smoothed.append({"x": x, "y": y, "t": time_val, "s": state})

    # 添加最后一个点
    smoothed.append(points[-1].copy())
    return smoothed
```

然后在 `convert_payload()` 函数中调用：

```python
def convert_payload(
    payload: dict[str, Any], *, source_file: str, fps: int = 60
) -> dict[str, Any]:
    normalized_fps = max(1, min(120, int(fps)))
    points = _sanitize_points(_extract_points(payload))
    if not points:
        raise ValueError("Input trajectory has no valid points/keyframes.")

    # ↓ 新增：拆分成段，每段分别平滑，再合并
    points = _smooth_segments(points, subdivisions=4)

    duration_s = _resolve_total_duration_s(payload, points)
    keyframes = _interpolate_points(points, duration_s, normalized_fps)
    # ... 后续不变
```

#### 2.2 处理断点空白：压缩或删除空白段

需要新增一个函数来处理多段录制间的空白：

```python
def _compress_gaps(
    points: list[dict[str, float | int]],
    max_gap_seconds: float = 0.5,
) -> list[dict[str, float | int]]:
    """
    检测并压缩轨迹中多段录制间的空白时间。
    
    如果相邻两个点之间的时间间隔超过 max_gap_seconds，
    将间隔压缩到 max_gap_seconds。
    
    这样断点之间不会有角色卡住不动的情况。
    """
    if len(points) < 2:
        return points

    compressed = [points[0].copy()]
    time_shift = 0.0

    for i in range(1, len(points)):
        gap = points[i]["t"] - points[i - 1]["t"]
        if gap > max_gap_seconds:
            # 超长间隔，压缩到 max_gap_seconds
            time_shift += gap - max_gap_seconds

        new_point = points[i].copy()
        new_point["t"] = float(points[i]["t"]) - time_shift
        compressed.append(new_point)

    return compressed
```

在 `convert_payload()` 中调用顺序：
```python
points = _sanitize_points(_extract_points(payload))
points = _compress_gaps(points, max_gap_seconds=0.5)  # 先压缩空白
points = _smooth_segments(points, subdivisions=4)       # 再平滑
```

#### 2.3 分段平滑辅助函数

由于多段录制之间可能有位置跳跃（断点），需要按段拆分后分别平滑：

```python
def _split_into_segments(
    points: list[dict[str, float | int]],
    jump_threshold_px: float = 100.0,
) -> list[list[dict[str, float | int]]]:
    """
    按位置跳跃把轨迹拆分成多段。
    如果相邻两个点的距离超过 jump_threshold_px，认为是一个新段的开始。
    """
    if not points:
        return []
    
    segments: list[list[dict[str, float | int]]] = [[points[0]]]
    
    for i in range(1, len(points)):
        dx = float(points[i]["x"]) - float(points[i - 1]["x"])
        dy = float(points[i]["y"]) - float(points[i - 1]["y"])
        dist = (dx * dx + dy * dy) ** 0.5
        
        if dist > jump_threshold_px:
            segments.append([points[i]])
        else:
            segments[-1].append(points[i])
    
    return segments


def _smooth_segments(
    points: list[dict[str, float | int]],
    subdivisions: int = 4,
    jump_threshold_px: float = 100.0,
) -> list[dict[str, float | int]]:
    """
    按段拆分 → 每段分别做 Catmull-Rom 平滑 → 合并结果。
    """
    segments = _split_into_segments(points, jump_threshold_px)
    result: list[dict[str, float | int]] = []
    
    for segment in segments:
        if len(segment) >= 3:
            smoothed = _smooth_points_catmull_rom(segment, subdivisions)
            result.extend(smoothed)
        else:
            result.extend(segment)
    
    return result
```

#### 2.4 在 TrajectoryPlayer 中增加缓动效果

修改 `gif_particle.py` 中 `TrajectoryPlayer.__init__()` 的时间轴设置：

```python
# 当前代码：
self._timeline.setEasingCurve(QEasingCurve.Type.Linear)

# 改为：使用 InOutQuad 让整个动画有"启动加速、结束减速"的感觉
self._timeline.setEasingCurve(QEasingCurve.Type.InOutQuad)
```

这是最简单的改动，但效果会比纯线性好很多。如果想要更精细的控制，可以用 `InOutCubic` 或者自定义缓动曲线。

---

## 修复 3：轨迹与状态映射文档化 + Metadata 系统

### 问题描述

当前系统中存在严重的映射混乱：

1. **State 数字 ↔ GIF 文件的映射**只存在于 `trajectory_recorder.py` 的 `gif_key_mapping` 字典和 `director.py` 的 `_build_voice_trajectory_gif_map()` 方法中，但两者的定义不完全一致，且没有文档说明每个 state 代表什么
2. **State 数字 ↔ 角色情绪/动作的映射**：`director.py` 中的 `EXPRESSION_STATE_MAP` 定义了 happy→state6、neutral→state1、angry→state4、sad→state5，但 state2、state3、state7 没有对应的情绪定义
3. **轨迹文件没有元数据**：不知道哪个轨迹对应什么场景、从哪个方向入场、包含哪些状态切换

### 修复方案

#### 3.1 创建统一的状态映射定义

在 `src/core/` 下新建 `character_states.py`（或在现有的 `gif_state_mapper.py` 中增加）：

```python
"""统一的角色状态定义。

所有涉及 state ID 的模块都应该引用此处的定义，
避免散落在各处的硬编码映射。
"""

from dataclasses import dataclass


@dataclass(frozen=True)
class CharacterStateInfo:
    state_id: int
    gif_filename: str
    label: str          # 人类可读的标签
    description: str    # 详细描述
    emotion: str | None = None  # 对应的情绪关键字（如果有）


# 所有已知的角色状态
CHARACTER_STATES: dict[int, CharacterStateInfo] = {
    1: CharacterStateInfo(
        state_id=1,
        gif_filename="state1.gif",
        label="默认/中性",
        description="角色的默认站立状态，面部表情平静",
        emotion="neutral",
    ),
    2: CharacterStateInfo(
        state_id=2,
        gif_filename="state2.gif",
        label="行走",
        description="角色行走动画",
        emotion=None,
    ),
    3: CharacterStateInfo(
        state_id=3,
        gif_filename="state3.gif",
        label="思考/等待",
        description="角色思考或等待中动画",
        emotion=None,
    ),
    4: CharacterStateInfo(
        state_id=4,
        gif_filename="state4.gif",
        label="生气",
        description="角色生气的表情动画",
        emotion="angry",
    ),
    5: CharacterStateInfo(
        state_id=5,
        gif_filename="state5.gif",
        label="悲伤/观察",
        description="角色悲伤或者在观察屏幕的动画",
        emotion="sad",
    ),
    6: CharacterStateInfo(
        state_id=6,
        gif_filename="state6.gif",
        label="开心",
        description="角色开心的表情动画",
        emotion="happy",
    ),
    7: CharacterStateInfo(
        state_id=7,
        gif_filename="state7.gif",
        label="惊讶/特殊",
        description="角色惊讶或特殊反应动画",
        emotion=None,
    ),
    8: CharacterStateInfo(
        state_id=8,
        gif_filename="aemeath.gif",
        label="主角色",
        description="主角色 aemeath 标准动画",
        emotion=None,
    ),
}


def get_state_label(state_id: int) -> str:
    """获取状态的人类可读标签。"""
    info = CHARACTER_STATES.get(state_id)
    return info.label if info else f"未知状态 {state_id}"


def get_gif_filename(state_id: int) -> str:
    """获取状态对应的 GIF 文件名。"""
    info = CHARACTER_STATES.get(state_id)
    return info.gif_filename if info else f"state{state_id}.gif"
```

> **注意：上面 state2/3/7 的 label 和 description 是我根据代码推断的猜测值。你需要根据你实际的 GIF 内容来填写正确的标签。**

#### 3.2 轨迹文件增加 Metadata

修改 `trajectory_recorder.py` 的 `save_trajectory()` 方法，在保存时增加元数据：

```python
def save_trajectory(self):
    if not self.current_path:
        print("没有路径可保存！")
        return

    timestamp = int(time.time())
    filename = f"trajectory_{timestamp}.json"

    save_dir = Path("recorded_paths")
    save_dir.mkdir(exist_ok=True)
    filepath = save_dir / filename

    # 分析轨迹的基本信息
    states_used = sorted(set(p["s"] for p in self.current_path))
    start_pos = self.current_path[0]
    end_pos = self.current_path[-1]

    # 判断入场方向
    screen_w = self.width()
    screen_h = self.height()
    entry_direction = "unknown"
    if start_pos["x"] <= 5:
        entry_direction = "left"
    elif start_pos["x"] >= screen_w - 5:
        entry_direction = "right"
    elif start_pos["y"] <= 5:
        entry_direction = "top"
    elif start_pos["y"] >= screen_h - 5:
        entry_direction = "bottom"

    data = {
        "metadata": {
            "created_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "description": "",  # 用户可以手动填写
            "entry_direction": entry_direction,
            "states_used": states_used,
            "state_labels": {
                str(s): self.gif_key_mapping.get(s, f"state{s}.gif")
                for s in states_used
            },
            "screen_resolution": {
                "width": screen_w,
                "height": screen_h,
            },
            "segments": self._count_segments(),
        },
        "total_points": len(self.current_path),
        "total_duration": self.current_path[-1]["t"],
        "points": self.current_path,
    }

    try:
        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        print(f"✅ 成功保存轨迹到: {filepath.absolute()}")
        QMessageBox.information(
            self,
            "保存成功",
            f"文件已保存:\n{filepath.name}\n"
            f"路径点: {len(self.current_path)}\n"
            f"入场方向: {entry_direction}\n"
            f"使用状态: {states_used}",
        )
    except Exception as e:
        print(f"❌ 保存失败: {e}")


def _count_segments(self) -> int:
    """统计断点数量（即多段录制的段数）。"""
    if len(self.current_path) < 2:
        return 1
    segments = 1
    for i in range(1, len(self.current_path)):
        dx = self.current_path[i]["x"] - self.current_path[i - 1]["x"]
        dy = self.current_path[i]["y"] - self.current_path[i - 1]["y"]
        dist = (dx * dx + dy * dy) ** 0.5
        if dist > 100:
            segments += 1
    return segments
```

#### 3.3 创建轨迹清单文件

在 `recorded_paths/` 目录下创建 `manifest.json`，索引所有轨迹文件。
可以手动创建，也可以在 `trajectory_recorder.py` 保存时自动更新。

手动创建的格式示例：

```json
{
  "trajectories": [
    {
      "filename": "trajectory_1771029879_qt_animation.json",
      "source": "trajectory_1771029879.json",
      "description": "三段从左侧入场的复合轨迹（aemeath→state2→state1）",
      "entry_direction": "left",
      "duration_seconds": 35.37,
      "states_used": [1, 2, 8],
      "trigger_condition": "voice_summon",
      "notes": "断点之间有较长空白，需要 _compress_gaps 处理"
    },
    {
      "filename": "trajectory_1770800738_qt_animation.json",
      "source": "trajectory_1770800738_optimized.json（原始文件已丢失）",
      "description": "从左下角斜向右上入场的轨迹",
      "entry_direction": "bottom-left",
      "duration_seconds": 12.27,
      "states_used": [1, 2],
      "trigger_condition": "未绑定",
      "notes": "经过 codex 优化的版本"
    }
  ],
  "state_mapping": {
    "1": { "gif": "state1.gif", "label": "默认/中性" },
    "2": { "gif": "state2.gif", "label": "行走" },
    "3": { "gif": "state3.gif", "label": "思考/等待" },
    "4": { "gif": "state4.gif", "label": "生气" },
    "5": { "gif": "state5.gif", "label": "悲伤/观察" },
    "6": { "gif": "state6.gif", "label": "开心" },
    "7": { "gif": "state7.gif", "label": "惊讶/特殊" },
    "8": { "gif": "aemeath.gif", "label": "主角色" }
  }
}
```

#### 3.4 修改 `trajectory_recorder.py` 的控制台提示

将状态切换提示从无意义的数字改为有标签的文本：

```python
# 修改 __init__ 中的 gif_key_mapping，增加标签
self.gif_key_mapping = {
    1: ("state1.gif", "默认/中性"),
    2: ("state2.gif", "行走"),
    3: ("state3.gif", "思考/等待"),
    4: ("state4.gif", "生气"),
    5: ("state5.gif", "悲伤/观察"),
    6: ("state6.gif", "开心"),
    7: ("state7.gif", "惊讶/特殊"),
    8: ("aemeath.gif", "主角色"),
    9: ("state1.gif", "自定义9"),
    0: ("state1.gif", "自定义0"),
}

# 对应地修改使用 gif_key_mapping 的所有地方：
# 现在值是 (gif_name, label) 元组而不是纯字符串
self.current_gif_name = self.gif_key_mapping.get(
    self.current_state_id, ("state1.gif", "未知")
)[0]

# keyPressEvent 中的切换提示改为：
if num in self.gif_key_mapping:
    gif_name, label = self.gif_key_mapping[num]
    self.current_state_id = num
    self.current_gif_name = gif_name
    print(f"-> 切换到按键 {num}: {gif_name} ({label})")
```

同时修改 `paintEvent` 中左上角的状态提示文字，显示标签名而不只是文件名。

#### 3.5 `Director` 中引用统一映射

修改 `director.py` 的 `_build_voice_trajectory_gif_map()` 方法和 `EXPRESSION_STATE_MAP`，从 `character_states.py` 引用统一定义，而不是各自硬编码：

```python
# 在 director.py 顶部 import
from .character_states import CHARACTER_STATES, get_gif_filename

# _build_voice_trajectory_gif_map 改为：
def _build_voice_trajectory_gif_map(self) -> dict[int, str]:
    characters_root = self._base_dir / "characters"
    mapping: dict[int, str] = {}
    for state_id, info in CHARACTER_STATES.items():
        path = characters_root / info.gif_filename
        if path.exists():
            mapping[state_id] = str(path)
    return mapping

# EXPRESSION_STATE_MAP 改为从 CHARACTER_STATES 中自动生成：
EXPRESSION_STATE_MAP = {
    info.emotion: f"state{info.state_id}"
    for info in CHARACTER_STATES.values()
    if info.emotion is not None
}
```

---

## 实施顺序建议

1. **先做修复 3**（状态映射文档化）→ 让所有映射关系清晰，后续修改有据可依
2. **再做修复 2**（轨迹平滑）→ 让已有轨迹的播放质量提升
3. **最后做修复 1**（接入触发）→ 让轨迹真正能被触发播放

每个修复完成后都应该运行一次应用验证效果。

---

## 当前轨迹文件分析备忘

### trajectory_1771029879.json（原始录制）
- **总点数**: 465
- **总时长**: 35.37 秒
- **包含 3 个断点段**:
  - 段 1 (0-0.89s): state 8 (aemeath.gif)，从屏幕最左侧 x=1 y=700 水平向右移入到 x=156
  - 段 2 (7.89-8.50s): state 2 (state2.gif)，从屏幕最左侧 x=3 y=425 水平向右移入到 x=193
  - 段 3 (13.68-35.37s): state 1 (state1.gif)，从屏幕最左侧 x=1 y=139 向右移入
- **问题**: 段与段之间分别有约 7 秒和 5 秒的空白间隔
- **已被 director.py 的 `VOICE_TRAJECTORY_FILE` 常量引用**（但相关代码是死代码）

### trajectory_1770800738_qt_animation.json（已优化版本）
- **来源**: trajectory_1770800738_optimized.json（原始文件已不存在）
- **总时长**: 12.27 秒
- **开始于**: x=12 y=1020（左下角）
- **轨迹方向**: 从左下角斜向右上方移动
- **状态切换**: state 1 → state 2 (在约 0.83s 处)
- **未被任何代码引用**，是孤立文件
