# Sprite Sheet (精灵图) 动画方案 —— 完整实施指南

> **目标读者**: 对 Sprite Sheet 概念不熟悉的开发者
> **前置条件**: 已有 GIF 格式的角色动画素材（state1.gif ~ state7.gif、aemeath.gif）
> **不需要**: Live2D、Spine、DragonBones 等专业动画工具
> **需要安装**: `Pillow` (Python 图像处理库，项目可能已安装)

---

## 目录

1. [什么是 Sprite Sheet](#1-什么是-sprite-sheet)
2. [为什么要从 GIF 切换到 Sprite Sheet](#2-为什么要从-gif-切换到-sprite-sheet)
3. [整体架构对比](#3-整体架构对比)
4. [第一步：安装依赖](#4-第一步安装依赖)
5. [第二步：从 GIF 提取帧为 PNG](#5-第二步从-gif-提取帧为-png)
6. [第三步：将 PNG 帧合并为 Sprite Sheet](#6-第三步将-png-帧合并为-sprite-sheet)
7. [第四步：Sprite Sheet 元数据文件](#7-第四步sprite-sheet-元数据文件)
8. [第五步：Qt 中的 Sprite Sheet 播放器](#8-第五步qt-中的-sprite-sheet-播放器)
9. [第六步：替换现有的 QMovie 播放](#9-第六步替换现有的-qmovie-播放)
10. [第七步：方向翻转（左右镜像）](#10-第七步方向翻转左右镜像)
11. [第八步：与轨迹系统集成](#11-第八步与轨迹系统集成)
12. [第九步：动态速度同步](#12-第九步动态速度同步)
13. [完整工具脚本：一键 GIF → Sprite Sheet](#13-完整工具脚本一键-gif--sprite-sheet)
14. [文件结构规划](#14-文件结构规划)
15. [迁移策略：渐进式替换](#15-迁移策略渐进式替换)
16. [常见问题 FAQ](#16-常见问题-faq)

---

## 1. 什么是 Sprite Sheet

**Sprite Sheet（精灵图/雪碧图）** 是把一个动画的所有帧排列在一张大图片上的技术。

举个例子，假设你的 `state2.gif`（行走动画）有 8 帧，每帧 64×64 像素：

```
GIF 方式：一个 .gif 文件，内部包含 8 帧，由 QMovie 自动播放
          帧率固定在 GIF 内部定义的值，你无法控制

Sprite Sheet 方式：一张 512×64 的 PNG 图片，8 帧横向排列
          [帧1][帧2][帧3][帧4][帧5][帧6][帧7][帧8]
          你的代码通过定时器手动切换显示哪一帧
```

视觉上：
```
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ F1 │ F2 │ F3 │ F4 │ F5 │ F6 │ F7 │ F8 │  ← 一张大 PNG
└────┴────┴────┴────┴────┴────┴────┴────┘
  ↑
  通过 QPixmap.copy(frame_index * 64, 0, 64, 64) 切出当前帧
```

**关键点：你不需要自己画任何新素材。** Sprite Sheet 可以从你现有的 GIF 文件自动生成。

---

## 2. 为什么要从 GIF 切换到 Sprite Sheet

| 维度 | GIF (QMovie) | Sprite Sheet (QPixmap) |
|------|-------------|----------------------|
| **帧率控制** | ❌ GIF 内嵌帧率，无法运行时改变 | ✅ 完全由代码控制，可随意加速/减速 |
| **透明度** | ❌ 只有 1-bit 透明（全透或全不透），边缘锯齿 | ✅ PNG 支持 32-bit RGBA，平滑半透明 |
| **方向翻转** | ❌ 需要额外准备一套镜像 GIF | ✅ 运行时 `QPixmap.transformed(QTransform().scale(-1,1))` |
| **缩放质量** | ❌ GIF 缩放后像素化严重 | ✅ PNG 缩放可用双线性/双三次插值 |
| **播放/暂停** | ⚠️ QMovie 有 API 但不精确 | ✅ 精确到帧级别控制 |
| **速度同步** | ❌ 无法让走路帧率匹配移动速度 | ✅ 根据移动速度动态调整帧间隔 |
| **内存** | ⚠️ 每个 GIF 解码后占独立缓存 | ✅ 一张大图，GPU 友好 |
| **文件大小** | ❌ GIF 压缩率差（256色限制） | ✅ PNG 无损压缩，质量更好体积更小 |

**一句话总结：Sprite Sheet 让你对动画有 100% 的控制权。**

---

## 3. 整体架构对比

### 现在的架构（GIF + QMovie）

```
characters/state1.gif  ──→  QMovie  ──→  QLabel.setMovie()
characters/state2.gif  ──→  QMovie  ──→  角色自动播放，你无法干预帧
...
TrajectoryPlayer 移动整个 QWidget 位置
```

### 目标架构（Sprite Sheet + QPixmap）

```
characters/sprites/
  state1_sheet.png  ──→  QPixmap 加载整张大图
  state1_sheet.json ──→  读取帧数、帧大小、帧率等元数据
                          │
                    SpriteAnimator 类
                          │
                    定时器驱动 → 每个 tick:
                      1. 计算当前帧索引
                      2. QPixmap.copy() 切出当前帧
                      3. QLabel.setPixmap(当前帧)
                          │
        TrajectoryPlayer 继续负责移动 QWidget 位置
        但现在可以同步控制帧率（走快→动画快）
```

---

## 4. 第一步：安装依赖

只需要一个 Python 库：**Pillow**（PIL 的现代版本）。

```bash
pip install Pillow
```

你的项目可能已经间接安装了（很多库依赖 Pillow）。验证：

```bash
python -c "from PIL import Image; print('Pillow OK')"
```

**不需要安装任何其他动画工具。**

---

## 5. 第二步：从 GIF 提取帧为 PNG

### 原理

GIF 文件内部存储了多帧图像。Pillow 可以逐帧读取并导出为带 alpha 通道的 PNG。

### 脚本：`src/tools/gif_to_frames.py`

```python
"""
从 GIF 文件提取所有帧，保存为带 alpha 通道的 PNG 文件。

用法:
    python gif_to_frames.py characters/state1.gif
    python gif_to_frames.py characters/state1.gif --output-dir frames/state1
    python gif_to_frames.py characters/*.gif  # 批量处理
"""

import argparse
import sys
from pathlib import Path
from PIL import Image


def extract_frames(gif_path: Path, output_dir: Path) -> list[dict]:
    """
    从 GIF 提取所有帧。

    返回帧信息列表：[{"index": 0, "path": "frame_000.png",
                        "width": 64, "height": 64, "duration_ms": 100}, ...]
    """
    output_dir.mkdir(parents=True, exist_ok=True)

    img = Image.open(gif_path)

    frames_info = []
    frame_index = 0

    # GIF 可能使用增量帧（只存储变化部分）
    # 需要用 "previous frame + current delta" 的方式合成完整帧
    previous_frame = None

    try:
        while True:
            # 获取当前帧的显示时长（毫秒）
            duration_ms = img.info.get("duration", 100)
            if duration_ms <= 0:
                duration_ms = 100  # 默认 100ms = 10fps

            # 将当前帧转为 RGBA（带 alpha 通道）
            current = img.convert("RGBA")

            # 处理 GIF dispose 方法
            # 有些 GIF 帧是增量的，需要叠加到前一帧上
            if previous_frame is not None and img.info.get("disposal", 0) != 2:
                # disposal=0: 不处理（叠加）
                # disposal=1: 不处理（叠加）
                # disposal=2: 恢复背景
                composite = previous_frame.copy()
                composite.paste(current, (0, 0), current)
                current = composite

            previous_frame = current.copy()

            # 保存为 PNG
            frame_filename = f"frame_{frame_index:03d}.png"
            frame_path = output_dir / frame_filename
            current.save(frame_path, "PNG")

            frames_info.append({
                "index": frame_index,
                "filename": frame_filename,
                "width": current.width,
                "height": current.height,
                "duration_ms": duration_ms,
            })

            frame_index += 1

            try:
                img.seek(img.tell() + 1)
            except EOFError:
                break

    finally:
        img.close()

    print(f"✅ 从 {gif_path.name} 提取了 {len(frames_info)} 帧到 {output_dir}")
    return frames_info


def main():
    parser = argparse.ArgumentParser(description="从 GIF 提取帧为 PNG")
    parser.add_argument("gif_files", nargs="+", help="GIF 文件路径")
    parser.add_argument(
        "--output-dir", "-o", default="",
        help="输出目录（默认: <gif文件名>_frames/）"
    )
    args = parser.parse_args()

    for gif_file in args.gif_files:
        gif_path = Path(gif_file).resolve()
        if not gif_path.exists():
            print(f"❌ 文件不存在: {gif_path}")
            continue
        if args.output_dir:
            output_dir = Path(args.output_dir).resolve()
        else:
            output_dir = gif_path.parent / f"{gif_path.stem}_frames"

        extract_frames(gif_path, output_dir)


if __name__ == "__main__":
    main()
```

### 使用方法

```bash
# 提取单个 GIF
python src/tools/gif_to_frames.py characters/state1.gif

# 输出到指定目录
python src/tools/gif_to_frames.py characters/state2.gif -o characters/sprites/state2_frames

# 批量提取所有 state GIF
python src/tools/gif_to_frames.py characters/state1.gif characters/state2.gif characters/state3.gif
```

### 输出结果

```
characters/
  state1_frames/
    frame_000.png
    frame_001.png
    frame_002.png
    ...
  state2_frames/
    frame_000.png
    ...
```

---

## 6. 第三步：将 PNG 帧合并为 Sprite Sheet

### 原理

把所有单帧 PNG 横向（或网格排列）拼接成一张大图。

### 脚本：`src/tools/frames_to_spritesheet.py`

```python
"""
将一组 PNG 帧合并为 Sprite Sheet（精灵图）。

用法:
    python frames_to_spritesheet.py characters/state1_frames/
    python frames_to_spritesheet.py characters/state1_frames/ --columns 4
"""

import argparse
import json
import math
from pathlib import Path
from PIL import Image


def create_spritesheet(
    frames_dir: Path,
    output_path: Path,
    *,
    columns: int = 0,
    padding: int = 0,
) -> dict:
    """
    将目录中的 PNG 帧合并为 Sprite Sheet。

    参数:
        frames_dir: 包含 frame_000.png, frame_001.png ... 的目录
        output_path: 输出的 sprite sheet PNG 路径
        columns: 每行放多少帧（0=自动=全部放一行）
        padding: 帧之间的像素间距

    返回:
        元数据字典，包含帧数、帧大小、布局信息
    """
    # 收集所有 PNG 帧，按文件名排序
    frame_files = sorted(frames_dir.glob("frame_*.png"))
    if not frame_files:
        raise FileNotFoundError(f"目录 {frames_dir} 中没有找到 frame_*.png 文件")

    # 读取所有帧并获取尺寸
    frames: list[Image.Image] = []
    for f in frame_files:
        img = Image.open(f).convert("RGBA")
        frames.append(img)

    # 所有帧应该尺寸相同（来自同一个 GIF）
    frame_width = frames[0].width
    frame_height = frames[0].height
    frame_count = len(frames)

    # 计算布局
    if columns <= 0:
        # 默认：如果帧数 <= 16，全部放一行
        # 否则使用接近正方形的网格
        if frame_count <= 16:
            cols = frame_count
        else:
            cols = math.ceil(math.sqrt(frame_count))
    else:
        cols = columns

    rows = math.ceil(frame_count / cols)

    # 计算 Sprite Sheet 尺寸
    sheet_width = cols * (frame_width + padding) - padding
    sheet_height = rows * (frame_height + padding) - padding

    # 创建空白画布（全透明）
    sheet = Image.new("RGBA", (sheet_width, sheet_height), (0, 0, 0, 0))

    # 逐帧粘贴到画布上
    for idx, frame in enumerate(frames):
        col = idx % cols
        row = idx // cols
        x = col * (frame_width + padding)
        y = row * (frame_height + padding)
        sheet.paste(frame, (x, y))

    # 保存 Sprite Sheet
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path, "PNG", optimize=True)

    # 关闭所有帧
    for frame in frames:
        frame.close()
    sheet.close()

    metadata = {
        "image": output_path.name,
        "frame_width": frame_width,
        "frame_height": frame_height,
        "frame_count": frame_count,
        "columns": cols,
        "rows": rows,
        "padding": padding,
        "sheet_width": sheet_width,
        "sheet_height": sheet_height,
    }

    print(f"✅ Sprite Sheet 已生成: {output_path}")
    print(f"   帧数: {frame_count}, 帧大小: {frame_width}x{frame_height}")
    print(f"   布局: {cols}列 × {rows}行, 总大小: {sheet_width}x{sheet_height}")

    return metadata


def main():
    parser = argparse.ArgumentParser(description="将 PNG 帧合并为 Sprite Sheet")
    parser.add_argument("frames_dir", help="包含 frame_*.png 的目录")
    parser.add_argument("--output", "-o", default="", help="输出路径")
    parser.add_argument("--columns", "-c", type=int, default=0, help="每行帧数（0=自动）")
    parser.add_argument("--padding", "-p", type=int, default=0, help="帧间距（像素）")
    args = parser.parse_args()

    frames_dir = Path(args.frames_dir).resolve()
    if not frames_dir.is_dir():
        print(f"❌ 目录不存在: {frames_dir}")
        return

    if args.output:
        output_path = Path(args.output).resolve()
    else:
        output_path = frames_dir.parent / f"{frames_dir.stem.replace('_frames', '')}_sheet.png"

    metadata = create_spritesheet(
        frames_dir, output_path,
        columns=args.columns, padding=args.padding,
    )

    # 保存元数据 JSON
    meta_path = output_path.with_suffix(".json")
    meta_path.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"   元数据: {meta_path}")


if __name__ == "__main__":
    main()
```

### 使用方法

```bash
python src/tools/frames_to_spritesheet.py characters/state1_frames/
# 输出: characters/state1_sheet.png + characters/state1_sheet.json
```

---

## 7. 第四步：Sprite Sheet 元数据文件

每个 Sprite Sheet 旁边都应有一个同名的 `.json` 元数据文件。
这个文件由上面的工具自动生成，格式如下：

```json
{
  "image": "state1_sheet.png",
  "frame_width": 64,
  "frame_height": 64,
  "frame_count": 12,
  "columns": 12,
  "rows": 1,
  "padding": 0,
  "sheet_width": 768,
  "sheet_height": 64,
  "default_fps": 10,
  "loop": true,
  "state_id": 1,
  "label": "默认/中性"
}
```

你可以手动编辑这个文件来调整 `default_fps`（默认播放帧率）和 `loop`（是否循环）。

---

## 8. 第五步：Qt 中的 Sprite Sheet 播放器

### 核心类：`SpriteAnimator`

这是替代 `QMovie` 的关键组件。它加载一张 Sprite Sheet PNG，
通过定时器驱动逐帧切换显示。

```python
"""
Sprite Sheet 动画播放器。

替代 QMovie，提供完全可控的帧级别动画播放。
放置于 src/ui/sprite_animator.py
"""

from __future__ import annotations

import json
from pathlib import Path

from PySide6.QtCore import QRect, QTimer, Signal, Qt
from PySide6.QtGui import QPixmap, QTransform, QPainter
from PySide6.QtWidgets import QWidget, QLabel


class SpriteAnimator(QWidget):
    """
    基于 Sprite Sheet 的动画播放器。

    核心能力：
    - 从 PNG sprite sheet + JSON 元数据加载动画
    - 可控帧率（运行时可随时改变）
    - 支持左右镜像翻转
    - 支持暂停/恢复/跳转到指定帧
    - 支持缩放（保持高质量）

    信号：
    - frame_changed(int): 当前帧索引变化时发出
    - animation_finished(): 非循环动画播放到最后一帧时发出
    - loop_completed(): 循环动画完成一圈时发出
    """

    frame_changed = Signal(int)
    animation_finished = Signal()
    loop_completed = Signal()

    def __init__(self, parent: QWidget | None = None):
        super().__init__(parent)
        # --- 数据 ---
        self._sheet_pixmap: QPixmap | None = None    # 整张 sprite sheet
        self._frame_width = 0
        self._frame_height = 0
        self._frame_count = 0
        self._columns = 1
        self._rows = 1
        self._padding = 0

        # --- 播放状态 ---
        self._current_frame = 0
        self._fps = 10.0
        self._loop = True
        self._playing = False
        self._flipped_h = False   # 水平翻转

        # --- 缓存 ---
        self._frame_cache: dict[int, QPixmap] = {}
        self._flipped_cache: dict[int, QPixmap] = {}
        self._scale_factor = 1.0

        # --- UI ---
        self._label = QLabel(self)
        self._label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._label.setStyleSheet("background: transparent;")

        # --- 定时器 ---
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._advance_frame)

    # ─────────────────────── 加载 ───────────────────────

    def load_from_files(self, sheet_path: str, meta_path: str = "") -> bool:
        """
        加载 Sprite Sheet。

        参数:
            sheet_path: PNG sprite sheet 的路径
            meta_path: JSON 元数据文件路径（默认: 同名 .json）
        """
        sheet_file = Path(sheet_path)
        if not sheet_file.exists():
            return False

        pixmap = QPixmap(str(sheet_file))
        if pixmap.isNull():
            return False

        # 加载元数据
        if not meta_path:
            meta_file = sheet_file.with_suffix(".json")
        else:
            meta_file = Path(meta_path)

        if meta_file.exists():
            try:
                meta = json.loads(meta_file.read_text(encoding="utf-8"))
            except Exception:
                meta = {}
        else:
            meta = {}

        self._sheet_pixmap = pixmap
        self._frame_width = int(meta.get("frame_width", pixmap.height()))
        self._frame_height = int(meta.get("frame_height", pixmap.height()))
        self._frame_count = int(meta.get("frame_count", 1))
        self._columns = int(meta.get("columns", self._frame_count))
        self._rows = int(meta.get("rows", 1))
        self._padding = int(meta.get("padding", 0))
        self._fps = float(meta.get("default_fps", 10))
        self._loop = bool(meta.get("loop", True))

        # 清空帧缓存
        self._frame_cache.clear()
        self._flipped_cache.clear()
        self._current_frame = 0

        # 预切所有帧
        self._precut_frames()

        # 设置初始大小
        self._update_size()
        self._show_current_frame()

        return True

    def _precut_frames(self) -> None:
        """预先从 sprite sheet 切出所有帧，缓存到字典中。"""
        if self._sheet_pixmap is None:
            return

        for idx in range(self._frame_count):
            col = idx % self._columns
            row = idx // self._columns
            x = col * (self._frame_width + self._padding)
            y = row * (self._frame_height + self._padding)

            frame = self._sheet_pixmap.copy(
                QRect(x, y, self._frame_width, self._frame_height)
            )
            self._frame_cache[idx] = frame

    # ─────────────────────── 播放控制 ───────────────────────

    def play(self) -> None:
        """开始播放动画。"""
        if self._frame_count < 2:
            return
        self._playing = True
        interval_ms = max(1, int(1000.0 / self._fps))
        self._timer.start(interval_ms)

    def pause(self) -> None:
        """暂停播放。"""
        self._playing = False
        self._timer.stop()

    def stop(self) -> None:
        """停止播放并重置到第一帧。"""
        self._playing = False
        self._timer.stop()
        self._current_frame = 0
        self._show_current_frame()

    def set_fps(self, fps: float) -> None:
        """运行时修改帧率。"""
        self._fps = max(0.1, float(fps))
        if self._playing:
            self._timer.setInterval(max(1, int(1000.0 / self._fps)))

    def set_frame(self, index: int) -> None:
        """跳转到指定帧。"""
        self._current_frame = max(0, min(index, self._frame_count - 1))
        self._show_current_frame()

    def set_flipped(self, flipped: bool) -> None:
        """设置水平翻转（左右镜像）。"""
        if self._flipped_h == flipped:
            return
        self._flipped_h = flipped
        self._flipped_cache.clear()
        self._show_current_frame()

    def set_scale(self, factor: float) -> None:
        """设置缩放比例。"""
        self._scale_factor = max(0.1, float(factor))
        self._update_size()
        self._show_current_frame()

    @property
    def current_frame(self) -> int:
        return self._current_frame

    @property
    def frame_count(self) -> int:
        return self._frame_count

    @property
    def is_playing(self) -> bool:
        return self._playing

    @property
    def fps(self) -> float:
        return self._fps

    # ─────────────────────── 内部 ───────────────────────

    def _advance_frame(self) -> None:
        """定时器回调：推进到下一帧。"""
        next_frame = self._current_frame + 1

        if next_frame >= self._frame_count:
            if self._loop:
                next_frame = 0
                self.loop_completed.emit()
            else:
                self._timer.stop()
                self._playing = False
                self.animation_finished.emit()
                return

        self._current_frame = next_frame
        self._show_current_frame()
        self.frame_changed.emit(self._current_frame)

    def _show_current_frame(self) -> None:
        """显示当前帧到 QLabel。"""
        frame = self._get_display_frame(self._current_frame)
        if frame is not None:
            if abs(self._scale_factor - 1.0) > 0.01:
                w = int(frame.width() * self._scale_factor)
                h = int(frame.height() * self._scale_factor)
                frame = frame.scaled(
                    w, h,
                    Qt.AspectRatioMode.KeepAspectRatio,
                    Qt.TransformationMode.SmoothTransformation,
                )
            self._label.setPixmap(frame)

    def _get_display_frame(self, index: int) -> QPixmap | None:
        """获取要显示的帧（考虑翻转）。"""
        if not self._flipped_h:
            return self._frame_cache.get(index)

        cached = self._flipped_cache.get(index)
        if cached is not None:
            return cached

        original = self._frame_cache.get(index)
        if original is None:
            return None

        flipped = original.transformed(QTransform().scale(-1, 1))
        self._flipped_cache[index] = flipped
        return flipped

    def _update_size(self) -> None:
        """更新 widget 大小。"""
        w = int(self._frame_width * self._scale_factor)
        h = int(self._frame_height * self._scale_factor)
        self.setFixedSize(w, h)
        self._label.setFixedSize(w, h)
```

### 使用方法

```python
from ui.sprite_animator import SpriteAnimator

# 创建播放器
animator = SpriteAnimator()

# 加载 sprite sheet
animator.load_from_files("characters/sprites/state1_sheet.png")

# 播放
animator.play()

# 运行时修改帧率（例如角色走快时加速动画）
animator.set_fps(20)

# 角色向左移动时翻转
animator.set_flipped(True)

# 暂停
animator.pause()

# 跳到第 5 帧
animator.set_frame(5)
```

---

## 9. 第六步：替换现有的 QMovie 播放

### 原理

当前 `TrajectoryPlayer`（`gif_particle.py`）和 `EntityWindow` 使用 `QMovie` 播放 GIF。
替换策略是：QMovie 和 SpriteAnimator 提供类似接口，可以渐进式替换。

### 在 TrajectoryPlayer 中替换

```python
# 原来的代码（gif_particle.py TrajectoryPlayer._preload_movies）:
movie = QMovie(source, parent=self)
movie.setCacheMode(QMovie.CacheMode.CacheAll)
movie.jumpToFrame(0)
self._movie_cache[state_id] = movie

# 替换为:
# 1. 先检查是否有 sprite sheet 版本
sheet_path = Path(source).with_name(
    Path(source).stem + "_sheet.png"
)
if sheet_path.exists():
    animator = SpriteAnimator(self)
    animator.load_from_files(str(sheet_path))
    self._sprite_cache[state_id] = animator
else:
    # fallback: 继续使用 QMovie
    movie = QMovie(source, parent=self)
    movie.setCacheMode(QMovie.CacheMode.CacheAll)
    self._movie_cache[state_id] = movie
```

---

## 10. 第七步：方向翻转（左右镜像）

当角色向左移动时，动画应该面向左边。Sprite Sheet 方案中只需一行代码：

```python
# 根据移动方向决定翻转
dx = new_x - old_x
if dx < 0:
    animator.set_flipped(True)   # 向左走，翻转
elif dx > 0:
    animator.set_flipped(False)  # 向右走，正常
# dx == 0 时保持原样
```

GIF 方案中做翻转非常麻烦（需要准备两套 GIF 或运行时逐帧翻转 QImage）。

---

## 11. 第八步：与轨迹系统集成

### TrajectoryPlayer 集成 SpriteAnimator

在 `TrajectoryPlayer._on_timeline_value_changed` 中，追加帧率同步逻辑：

```python
def _on_timeline_value_changed(self, progress: float) -> None:
    # ... 现有的位置计算代码 ...

    # 计算移动速度（像素/秒）
    if hasattr(self, '_last_pos') and hasattr(self, '_last_time'):
        dt = elapsed - self._last_time
        if dt > 0:
            dx = new_x - self._last_pos[0]
            dy = new_y - self._last_pos[1]
            speed = ((dx*dx + dy*dy) ** 0.5) / dt

            # 根据速度调整动画帧率
            if self._current_sprite is not None:
                # 速度越快，帧率越高
                # 基准: 100 像素/秒 = 10fps
                target_fps = max(5, min(30, speed / 10.0))
                self._current_sprite.set_fps(target_fps)

            # 根据方向翻转
            if self._current_sprite is not None:
                self._current_sprite.set_flipped(dx < 0)

    self._last_pos = (new_x, new_y)
    self._last_time = elapsed
```

---

## 12. 第九步：动态速度同步

这是 Sprite Sheet 方案最大的优势之一：**让动画帧率匹配移动速度**。

### 效果对比

| 场景 | GIF 方案 | Sprite Sheet 方案 |
|------|---------|------------------|
| 角色快速移动 | 脚步动画 10fps，感觉在"滑行" | 自动加速到 25fps，脚步与移动同步 |
| 角色缓慢移动 | 脚步动画 10fps，感觉在"原地跑" | 自动减速到 5fps，自然的慢走 |
| 角色停下来 | 继续播放走路动画 | 自动切换到站立帧（frame 0） |

### 速度-帧率映射函数

```python
def speed_to_fps(speed_px_per_sec: float, base_fps: float = 10.0) -> float:
    """
    将移动速度映射到动画帧率。

    参数:
        speed_px_per_sec: 角色移动速度（像素/秒）
        base_fps: 基准帧率（中等速度时的帧率）

    返回:
        目标帧率
    """
    if speed_px_per_sec < 5.0:
        return 0.0  # 几乎静止，暂停动画或显示站立帧

    # 对数映射：速度翻倍 → 帧率增加 50%
    import math
    ratio = speed_px_per_sec / 100.0  # 100px/s 为基准速度
    fps = base_fps * (1.0 + math.log2(max(0.1, ratio)))
    return max(3.0, min(30.0, fps))
```

---

## 13. 完整工具脚本：一键 GIF → Sprite Sheet

### 脚本：`src/tools/gif_to_spritesheet.py`

这个脚本将前面两步合为一步，直接从 GIF 生成 Sprite Sheet + 元数据。

```python
"""
一键将 GIF 转换为 Sprite Sheet（PNG + JSON 元数据）。

用法:
    python gif_to_spritesheet.py characters/state1.gif
    python gif_to_spritesheet.py characters/*.gif --output-dir characters/sprites/
    python gif_to_spritesheet.py characters/state2.gif --columns 4 --scale 2.0

这个脚本会：
1. 从 GIF 提取所有帧（自动处理增量帧/透明度）
2. 合并为一张 Sprite Sheet PNG
3. 生成配套的 JSON 元数据文件
"""

import argparse
import json
import math
import tempfile
from pathlib import Path
from PIL import Image


def gif_to_spritesheet(
    gif_path: Path,
    output_dir: Path,
    *,
    columns: int = 0,
    padding: int = 0,
    scale: float = 1.0,
    state_id: int | None = None,
    label: str = "",
) -> dict:
    """
    从 GIF 生成 Sprite Sheet。

    返回元数据字典。
    """
    img = Image.open(gif_path)
    frames: list[Image.Image] = []
    durations: list[int] = []
    previous = None

    try:
        while True:
            dur = img.info.get("duration", 100) or 100
            durations.append(max(1, int(dur)))

            current = img.convert("RGBA")
            if previous is not None and img.info.get("disposal", 0) != 2:
                comp = previous.copy()
                comp.paste(current, (0, 0), current)
                current = comp
            previous = current.copy()

            # 可选缩放
            if abs(scale - 1.0) > 0.01:
                w = max(1, int(current.width * scale))
                h = max(1, int(current.height * scale))
                current = current.resize((w, h), Image.LANCZOS)

            frames.append(current)

            try:
                img.seek(img.tell() + 1)
            except EOFError:
                break
    finally:
        img.close()

    if not frames:
        raise ValueError(f"GIF {gif_path} 没有有效帧")

    fw = frames[0].width
    fh = frames[0].height
    fc = len(frames)

    # 布局
    if columns <= 0:
        cols = fc if fc <= 16 else math.ceil(math.sqrt(fc))
    else:
        cols = columns
    rows = math.ceil(fc / cols)

    sw = cols * (fw + padding) - padding
    sh = rows * (fh + padding) - padding

    sheet = Image.new("RGBA", (sw, sh), (0, 0, 0, 0))
    for idx, frame in enumerate(frames):
        c = idx % cols
        r = idx // cols
        sheet.paste(frame, (c * (fw + padding), r * (fh + padding)))

    output_dir.mkdir(parents=True, exist_ok=True)
    stem = gif_path.stem
    sheet_path = output_dir / f"{stem}_sheet.png"
    meta_path = output_dir / f"{stem}_sheet.json"

    sheet.save(sheet_path, "PNG", optimize=True)

    # 计算平均帧率
    avg_dur = sum(durations) / len(durations) if durations else 100
    avg_fps = round(1000.0 / max(1, avg_dur), 1)

    metadata = {
        "image": sheet_path.name,
        "source_gif": gif_path.name,
        "frame_width": fw,
        "frame_height": fh,
        "frame_count": fc,
        "columns": cols,
        "rows": rows,
        "padding": padding,
        "sheet_width": sw,
        "sheet_height": sh,
        "default_fps": avg_fps,
        "frame_durations_ms": durations,
        "loop": True,
    }
    if state_id is not None:
        metadata["state_id"] = state_id
    if label:
        metadata["label"] = label

    meta_path.write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    for f in frames:
        f.close()
    sheet.close()

    print(f"✅ {gif_path.name} → {sheet_path.name}")
    print(f"   {fc} 帧, {fw}x{fh}, {avg_fps}fps, 布局 {cols}x{rows}")

    return metadata


def main():
    parser = argparse.ArgumentParser(
        description="一键将 GIF 转换为 Sprite Sheet",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python gif_to_spritesheet.py characters/state1.gif
  python gif_to_spritesheet.py characters/state*.gif --output-dir characters/sprites/
  python gif_to_spritesheet.py characters/state2.gif --scale 1.5 --columns 8
""",
    )
    parser.add_argument("gif_files", nargs="+", help="GIF 文件路径")
    parser.add_argument("--output-dir", "-o", default="", help="输出目录")
    parser.add_argument("--columns", "-c", type=int, default=0, help="每行帧数")
    parser.add_argument("--padding", type=int, default=0, help="帧间距")
    parser.add_argument("--scale", type=float, default=1.0, help="缩放倍数")
    args = parser.parse_args()

    for gif_file in args.gif_files:
        gp = Path(gif_file).resolve()
        if not gp.exists():
            print(f"❌ 文件不存在: {gp}")
            continue
        od = Path(args.output_dir).resolve() if args.output_dir else gp.parent / "sprites"
        gif_to_spritesheet(gp, od, columns=args.columns,
                           padding=args.padding, scale=args.scale)


if __name__ == "__main__":
    main()
```

### 使用方法

```bash
# 转换所有角色 GIF（最常用的命令，一行搞定）
python src/tools/gif_to_spritesheet.py characters/state*.gif characters/aemeath.gif

# 指定输出目录
python src/tools/gif_to_spritesheet.py characters/state*.gif -o characters/sprites/

# 放大 1.5 倍（如果原素材太小）
python src/tools/gif_to_spritesheet.py characters/state1.gif --scale 1.5
```

---

## 14. 文件结构规划

完成转换后，推荐的文件结构：

```
characters/
├── state1.gif                     # 保留原始 GIF（向后兼容）
├── state2.gif
├── ...
├── aemeath.gif
└── sprites/                       # 新增 Sprite Sheet 目录
    ├── state1_sheet.png           # Sprite Sheet 图片
    ├── state1_sheet.json          # 元数据
    ├── state2_sheet.png
    ├── state2_sheet.json
    ├── ...
    ├── aemeath_sheet.png
    └── aemeath_sheet.json
```

---

## 15. 迁移策略：渐进式替换

**不要一次性全部替换！** 推荐分步骤渐进迁移：

### 阶段 1：准备素材（不改代码）

1. 运行 `gif_to_spritesheet.py` 生成所有 Sprite Sheet
2. 检查生成的 PNG 和 JSON 是否正确
3. 用图片查看器打开验证帧是否完整

### 阶段 2：新建 SpriteAnimator 类

1. 将上面的 `SpriteAnimator` 代码保存到 `src/ui/sprite_animator.py`
2. 写一个简单的测试脚本验证播放效果
3. 确认帧切换、翻转、缩放都正常

### 阶段 3：在 TrajectoryPlayer 中支持双模式

1. 修改 `TrajectoryPlayer._preload_movies()`
2. 优先尝试加载 Sprite Sheet，找不到就 fallback 到 QMovie
3. 这样新旧代码可以并存，不会破坏现有功能

### 阶段 4：在 EntityWindow 中支持双模式

1. 修改 `EntityWindow` 的 GIF 显示逻辑
2. 同样采用优先 Sprite Sheet + fallback QMovie 的策略

### 阶段 5：增加速度同步（可选）

1. 在 TrajectoryPlayer 中增加速度→帧率映射
2. 根据移动方向自动翻转

---

## 16. 常见问题 FAQ

### Q1: 我需要重新画素材吗？

**不需要。** 整个方案基于将你**现有的 GIF** 转换为 Sprite Sheet。
你的 state1.gif ~ state7.gif 和 aemeath.gif 都可以直接转换。

### Q2: 转换后 GIF 还能用吗？

**能。** 建议保留原始 GIF 文件。代码中采用"优先 Sprite Sheet，fallback GIF"
的策略，所以即使 Sprite Sheet 缺失，系统也能正常运行。

### Q3: Pillow 是唯一需要安装的东西吗？

**是的。** `pip install Pillow` 就够了。不需要 Spine、Live2D、TexturePacker
等任何专业工具。所有转换逻辑都在 Python 脚本中。

### Q4: Sprite Sheet 的 PNG 文件会不会很大？

通常比 GIF 还小。因为 PNG 的无损压缩比 GIF 的 LZW 压缩效率更高。
一个 12 帧、64x64 的 sprite sheet 通常只有 10-30KB。

### Q5: 我可以在不同角色包中使用不同的方案吗？

**可以。** 有些角色包可以用 Sprite Sheet（如果素材质量好），
有些可以继续用 GIF（如果懒得转换）。代码会自动检测并选用合适的方案。

### Q6: SpriteAnimator 支持 APNG 吗？

当前实现基于 Sprite Sheet PNG（静态大图）。
如果你想直接播放 APNG（动画 PNG），Qt 6 的 QMovie 本身就支持 APNG 格式，
直接把 `.apng` 文件当普通 GIF 一样用 QMovie 加载即可（无需 SpriteAnimator）。

### Q7: 生成 Sprite Sheet 时帧的透明度不对怎么办？

有些 GIF 使用"增量帧"（只存储变化像素）。`gif_to_spritesheet.py` 已经处理了
这种情况（通过合成前一帧 + 当前帧）。如果仍有问题，
检查 GIF 的 `disposal` 方法设置。

### Q8: 这个方案和现在的轨迹修复方案冲突吗？

**不冲突。** 轨迹修复方案（gif动画.md）解决的是"路径平滑"和"触发链路"问题，
Sprite Sheet 方案解决的是"动画播放质量"问题。
两个方案可以独立实施，也可以组合使用达到最佳效果。

### Q9: 实施这个方案的优先级如何？

**建议在 gif动画.md 的三个修复之后再实施。**
先修好轨迹和触发链路（最低成本、最高性价比），
再考虑 Sprite Sheet 升级（锦上添花）。

---

## 附录：快速参考卡片

```
┌──────────────────────────────────────────────────┐
│        GIF → Sprite Sheet 快速转换流程            │
├──────────────────────────────────────────────────┤
│                                                  │
│  1. pip install Pillow                           │
│                                                  │
│  2. python src/tools/gif_to_spritesheet.py \     │
│       characters/state*.gif \                    │
│       characters/aemeath.gif \                   │
│       -o characters/sprites/                     │
│                                                  │
│  3. 检查 characters/sprites/ 下的 PNG 和 JSON    │
│                                                  │
│  4. 将 SpriteAnimator 类保存到                    │
│     src/ui/sprite_animator.py                    │
│                                                  │
│  5. 修改 TrajectoryPlayer._preload_movies()      │
│     支持双模式加载                                │
│                                                  │
│  完成！GIF 保留作为 fallback，新旧并存。          │
└──────────────────────────────────────────────────┘
```
完全理解你的意思了！你要的是：

在一个可视化编辑器里画好动画路径/时间轴 → 导出 JSON 数据 → 你的程序直接加载播放

就像 Figma → 前端代码 那样。答案是：有的！ 而且有好几个选择。

适合你的"动画 Figma"
🥇 最推荐：Rive（原名 Flare）
项目	说明
是什么	在线动画编辑器，有完整的时间轴、关键帧、缓动曲线
免费吗	✅ 个人版免费
导出格式	.riv 二进制文件，有 Python/C++ runtime 可以直接播放
适配难度	🟡 中等——需要用 rive-python 或把 runtime 集成到 Qt
类比	Figma 之于 UI = Rive 之于动画
你可以在 Rive 里：画角色 → 拖时间轴 → 设关键帧 → 加缓动 → 导出 → 你的程序加载运行

但问题是：Rive 适合矢量动画（自己在里面画角色）。你现有的是 GIF 位图素材，不太适合直接导入。

🥈 最适合你的情况：Lottie + After Effects
项目	说明
流程	After Effects 做动画 → Bodymovin 插件导出 → Lottie JSON
导出格式	

.json
（Lottie 格式），包含完整的位置、缩放、旋转关键帧
Qt 支持	✅ 有 QQuickWidget + Lottie QML 插件，或用 lottie-python 解析
免费吗	AE 要付费，但有替代方案（见下面）
你的素材	可以把 GIF 帧作为序列帧导入 AE，然后在时间轴上编排路径
AE 的免费替代：

Cavalry — 免费版可用，类似 AE 但专为动效设计
Haiku Animator — 开源，导出 Lottie JSON
LottieFiles 在线编辑器 — 浏览器内编辑 Lottie 动画
🥉 最简单但够用：Piskel
项目	说明
是什么	免费在线像素动画编辑器
导出格式	Sprite Sheet PNG，直接就是你需要的格式！
适配难度	🟢 零——导出的 PNG 直接给你的 SpriteAnimator 用
局限	只能做帧动画，不能编辑移动路径/轨迹
总结：哪个对你来说最实际？
你的核心需求是两件事：

需求	最佳工具
编辑角色帧动画（走路、表情、站立）	Piskel（免费在线，导出 Sprite Sheet）
编辑移动轨迹/路径（从哪来、到哪去、什么速度）	目前没有完美匹配的工具
坦率说，"轨迹编辑"这件事比较特殊——你需要的是在你的屏幕坐标系里画路径，市面上的动画编辑器都是在自己的画布坐标系里工作的。所以轨迹这块，你目前的 

trajectory_recorder.py
 + Catmull-Rom 平滑 其实已经是最实际的方案了。

帧动画那块倒是可以考虑用 Piskel 来做——比如你觉得 

state2.gif
 的走路动画帧数太少、不够流畅，可以在 Piskel 里重新画/补帧，然后导出 Sprite Sheet 直接用。