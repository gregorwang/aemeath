# 📝 Project Cyber-Companion 开发文档 (Phase 1)

## —— 核心架构与 MVP 设计

---

## 1.0 产品愿景 (Vision)

打造一个**非侵入式、有生命感的 Windows 桌面伴侣**。

### 核心定义

| 属性 | 描述 |
|------|------|
| **存在方式** | 生活在屏幕边缘的数字生命，不是传统"程序窗口" |
| **触发机制** | 用户离席（Idle）时自动出现，用户回归时自动隐藏 |
| **表现形式** | 视觉（彩色 ASCII 动画）+ 听觉（TTS 语音合成） |
| **平台** | Windows 10/11，支持高 DPI 显示 |
| **潜行能力** | 平时完全不可见，不在任务栏显示，不干扰用户操作 |

### 用户体验场景

1. 用户离开电脑 3 分钟 → 一个彩色 ASCII 角色从屏幕边缘探出头
2. 角色说一句符合当前时段的台词（如深夜："还不睡吗？"）
3. 用户回来触摸鼠标/键盘 → 角色做出惊吓表情并迅速缩回屏幕外
4. 角色消失，系统恢复完全隐形状态

---

## 1.1 技术栈选型 (Tech Stack)

### 精确依赖清单

```txt
# requirements.txt (Phase 1 MVP)
PySide6>=6.6.0,<7.0.0          # Qt for Python - GUI 框架
Pillow>=10.0.0,<11.0.0         # 图像处理
numpy>=1.24.0,<2.0.0           # 矩阵计算（图片转 ASCII）
edge-tts>=6.1.0                # 微软 Edge 语音合成
pywin32>=306                    # Windows API 调用
```

### 选型理由

| 组件 | 选型 | 理由 |
|------|------|------|
| **GUI 框架** | PySide6 | 比 PyQt5 高分屏支持更好；Qt 官方维护；LGPL 许可证对个人开发友好；内置 `QPropertyAnimation` 动画框架 |
| **系统交互** | `ctypes` + `pywin32` | 直接调用 `User32.dll` 获取系统空闲时间，零延迟，无需额外依赖 |
| **图像处理** | `Pillow` + `NumPy` | 成熟稳定，高效处理图片 → ASCII 矩阵算法 |
| **语音合成** | `edge-tts` | 免费、效果极佳（支持中文自然语音）、异步接口 |
| **进程管理** | `threading` / `asyncio` | 保证 GUI 主线程不卡顿，后台监控逻辑独立运行 |

### Python 版本要求

```
Python >= 3.10, < 3.13
```

> **注意**：Python 3.10+ 是因为需要 `match-case` 语法和 PySide6 的兼容性。避免 3.13 是因为 PyInstaller 对最新版支持可能不稳定。

---

## 1.2 核心模块架构 (Architecture)

### 三层架构总览

```
┌─────────────────────────────────────────────────────────────┐
│                    层级 C: 表现层 (The Body)                  │
│   ┌─────────────────┐    ┌──────────────────────────┐       │
│   │  AsciiRenderer  │    │    EntityWindow (QWidget) │       │
│   │  (视觉引擎)      │    │    (透明无边框窗口)         │       │
│   └────────┬────────┘    └────────────┬─────────────┘       │
│            │                         │                      │
├────────────┼─────────────────────────┼──────────────────────┤
│            │    层级 B: 决策层 (The Brain)                    │
│            │    ┌────────────────────┐│                      │
│            └────┤ Director (导演)    │┘                      │
│                 │  - 调度资源         │                       │
│                 │  - 选择台词/动画    │                       │
│                 │  - 指挥渲染层       │                       │
│                 └────────┬───────────┘                       │
├──────────────────────────┼──────────────────────────────────┤
│                          │   层级 A: 感知层 (The Senses)      │
│                 ┌────────┴───────────┐                       │
│                 │ IdleMonitor        │                       │
│                 │ (Daemon Thread)    │                       │
│                 │  - GetLastInputInfo│                       │
│                 │  - 状态机管理       │                       │
│                 └────────────────────┘                       │
└─────────────────────────────────────────────────────────────┘
```

### 信号流 (Signal Flow)

```
IdleMonitor ──[Signal: user_idle_confirmed]──► Director
IdleMonitor ──[Signal: user_active_detected]──► Director
Director ──[调用: show_entity(pos, script)]──► EntityWindow
Director ──[调用: hide_entity(anim_type)]──► EntityWindow
Director ──[调用: play_audio(file_path)]──► AudioManager
```

---

## 1.3 层级 A: 感知层 (The Senses) — 详细设计

### IdleMonitor 类定义

```python
# src/core/idle_monitor.py

import ctypes
import ctypes.wintypes
import time
from PySide6.QtCore import QThread, Signal


class LASTINPUTINFO(ctypes.Structure):
    """
    Windows API 结构体：LASTINPUTINFO
    文档: https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-lastinputinfo
    """
    _fields_ = [
        ("cbSize", ctypes.wintypes.UINT),  # 结构体大小，必须初始化为 8
        ("dwTime", ctypes.wintypes.DWORD), # 最后一次输入事件的系统 Tick Count
    ]


class IdleState:
    """空闲状态枚举"""
    STANDBY = "STANDBY"           # 待命：用户活跃，等待空闲
    PRE_IDLE = "PRE_IDLE"         # 预空闲：接近阈值，可做预加载
    IDLE_TRIGGERED = "IDLE_TRIGGERED"  # 已触发：超过阈值
    ACTIVE = "ACTIVE"             # 活跃中：角色正在显示时用户回来了


class IdleMonitor(QThread):
    """
    空闲监控器 — 后台守护线程
    
    职责：
    - 每 100ms 轮询 Windows API GetLastInputInfo()
    - 计算用户空闲时间
    - 根据状态机发出对应信号
    
    信号：
    - user_idle_confirmed: 用户空闲超过阈值
    - user_active_detected: 用户从空闲状态恢复操作
    - idle_time_updated(int): 实时空闲时间（毫秒），用于调试
    """
    
    user_idle_confirmed = Signal()
    user_active_detected = Signal()
    idle_time_updated = Signal(int)  # 调试用：实时发送空闲毫秒数
    
    # ── 配置常量 ──
    POLL_INTERVAL_MS: int = 100        # 轮询间隔（毫秒）
    DEFAULT_THRESHOLD_MS: int = 180000 # 默认空闲阈值（3分钟 = 180,000ms）
    PRE_IDLE_RATIO: float = 0.8        # 预空闲比例（80% 阈值时进入预空闲）
    ACTIVE_RESET_MS: int = 1000        # 活跃重置阈值（1秒内有输入视为活跃）
    
    def __init__(self, threshold_ms: int = DEFAULT_THRESHOLD_MS, parent=None):
        super().__init__(parent)
        self._threshold_ms = threshold_ms
        self._state = IdleState.STANDBY
        self._running = False
        self._user32 = ctypes.windll.user32
        self._kernel32 = ctypes.windll.kernel32
    
    def run(self) -> None:
        """主轮询循环 — 在独立线程中运行"""
        self._running = True
        while self._running:
            idle_ms = self._get_idle_time_ms()
            self.idle_time_updated.emit(idle_ms)
            self._update_state(idle_ms)
            self.msleep(self.POLL_INTERVAL_MS)
    
    def stop(self) -> None:
        """安全停止线程"""
        self._running = False
        self.wait(2000)  # 等待最多 2 秒
    
    def _get_idle_time_ms(self) -> int:
        """
        调用 Windows API 获取空闲时间
        
        返回值: 空闲毫秒数
        
        注意: GetTickCount() 在系统运行 49.7 天后会溢出（DWORD 上限）。
        生产环境应使用 GetTickCount64() 替代。
        """
        lii = LASTINPUTINFO()
        lii.cbSize = ctypes.sizeof(LASTINPUTINFO)
        
        if not self._user32.GetLastInputInfo(ctypes.byref(lii)):
            return 0  # API 调用失败，安全退出
        
        current_tick = self._kernel32.GetTickCount()
        idle_time = current_tick - lii.dwTime
        
        # 处理 GetTickCount 溢出（约 49.7 天后）
        if idle_time < 0:
            idle_time += 0xFFFFFFFF
        
        return idle_time
    
    def _update_state(self, idle_ms: int) -> None:
        """
        有限状态机 (FSM) — 状态流转逻辑
        
        状态转换图:
        STANDBY ──(idle > threshold)──► IDLE_TRIGGERED ──(input detected)──► ACTIVE ──(动画完成)──► STANDBY
                ──(idle > 80% threshold)──► PRE_IDLE ──(idle > threshold)──► IDLE_TRIGGERED
                                                     ──(input detected)──► STANDBY
        """
        match self._state:
            case IdleState.STANDBY:
                if idle_ms >= self._threshold_ms:
                    self._state = IdleState.IDLE_TRIGGERED
                    self.user_idle_confirmed.emit()
                elif idle_ms >= self._threshold_ms * self.PRE_IDLE_RATIO:
                    self._state = IdleState.PRE_IDLE
                    
            case IdleState.PRE_IDLE:
                if idle_ms >= self._threshold_ms:
                    self._state = IdleState.IDLE_TRIGGERED
                    self.user_idle_confirmed.emit()
                elif idle_ms < self.ACTIVE_RESET_MS:
                    self._state = IdleState.STANDBY
                    
            case IdleState.IDLE_TRIGGERED:
                if idle_ms < self.ACTIVE_RESET_MS:
                    self._state = IdleState.ACTIVE
                    self.user_active_detected.emit()
                    
            case IdleState.ACTIVE:
                # ACTIVE 状态等待外部（Director）通知动画完成后回到 STANDBY
                if idle_ms >= self._threshold_ms:
                    self._state = IdleState.IDLE_TRIGGERED
                    self.user_idle_confirmed.emit()
    
    def reset_to_standby(self) -> None:
        """外部调用：通知动画完成，回到待命状态"""
        self._state = IdleState.STANDBY
```

### IdleMonitor 验收标准 (Done Criteria)

| # | 验收项 | 测试方法 |
|---|--------|---------|
| 1 | 控制台能实时打印空闲时间 | 运行脚本，观察输出 `"用户已空闲: 10s... 20s..."` |
| 2 | 3 分钟不操作后触发 `user_idle_confirmed` 信号 | 连接信号到 print，等待触发 |
| 3 | 触发后移动鼠标，`user_active_detected` 信号触发 | 连接信号到 print，手动操作验证 |
| 4 | 不阻塞主线程 | 在 GUI 应用中集成，确认界面不卡顿 |
| 5 | CPU 占用率 < 1% | 使用任务管理器观察 |

---

## 1.4 层级 B: 决策层 (The Brain) — 详细设计

### Director 类定义

```python
# src/core/director.py

import random
import json
from datetime import datetime
from pathlib import Path
from PySide6.QtCore import QObject, Slot
from PySide6.QtWidgets import QApplication


class ScreenEdge:
    """屏幕边缘枚举"""
    RIGHT = "right"
    LEFT = "left"
    TOP = "top"
    BOTTOM = "bottom"


class Director(QObject):
    """
    导演控制器 — 接收感知层信号，指挥表现层
    
    职责：
    1. 接收 IdleMonitor 的信号
    2. 随机决定角色出场方向和位置
    3. 根据时段选择台词
    4. 调用 TTS 生成音频
    5. 命令 EntityWindow 执行动画
    
    依赖：
    - IdleMonitor: 信号源
    - EntityWindow: 渲染目标
    - AudioManager: 音频播放
    - AssetManager: 资源管理
    """
    
    def __init__(self, entity_window, audio_manager, asset_manager, parent=None):
        super().__init__(parent)
        self._entity_window = entity_window
        self._audio_manager = audio_manager
        self._asset_manager = asset_manager
    
    @Slot()
    def on_user_idle(self) -> None:
        """
        用户空闲时的处理逻辑
        
        执行流程：
        1. 获取屏幕尺寸
        2. 随机选择出场边缘和坐标
        3. 从 AssetManager 获取当前时段台词
        4. 生成/获取 TTS 音频
        5. 命令 EntityWindow 执行滑入动画
        """
        screen = QApplication.primaryScreen().geometry()
        edge = random.choice([ScreenEdge.RIGHT, ScreenEdge.LEFT])
        
        # 计算出场坐标（在屏幕边缘高度的 20%-80% 范围内随机）
        y_pos = random.randint(
            int(screen.height() * 0.2),
            int(screen.height() * 0.8)
        )
        
        # 获取当前时段的台词
        script = self._asset_manager.get_script_for_time(datetime.now())
        
        # 命令 EntityWindow 开始出场动画序列
        self._entity_window.summon(edge=edge, y_position=y_pos, script=script)
    
    @Slot()
    def on_user_active(self) -> None:
        """
        用户回归时的处理逻辑
        
        执行流程：
        1. 立即中断当前动画和音频
        2. 命令 EntityWindow 执行惊吓/逃跑动画
        3. 动画完成后隐藏窗口
        """
        self._audio_manager.interrupt()
        self._entity_window.flee()
```

### AssetManager 接口定义

```python
# src/core/asset_manager.py

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Optional


@dataclass
class Script:
    """台词数据模型"""
    id: str                    # 台词唯一标识，如 "midnight_greet_01"
    text: str                  # 台词文本
    audio_path: Optional[str]  # 预生成的音频文件路径（可选）
    anim_speed: str            # 动画速度: "slow", "normal", "fast"
    priority: int              # 优先级: 1=高, 2=中, 3=低


class AssetManager:
    """
    资源管理器
    
    职责：
    - 加载角色资源包
    - 根据时间段查询台词
    - 管理静态资源（图片、音效）路径
    """
    
    def __init__(self, character_dir: Path):
        """
        参数:
            character_dir: 角色资源包根目录，如 characters/rem_maid/
        """
        self._character_dir = character_dir
        self._scripts: list[Script] = []
        self._load_scripts()
    
    def _load_scripts(self) -> None:
        """从 scripts.json 加载台词库"""
        # 实现: 读取 JSON 并解析为 Script 对象列表
        ...
    
    def get_script_for_time(self, now: datetime) -> Script:
        """
        根据当前时间获取合适的台词
        
        匹配规则（按优先级）：
        1. 精确匹配当前时间范围的台词
        2. 匹配 "default" 时间范围的台词
        3. 如果有多个匹配，根据 probability 字段随机选择
        
        参数:
            now: 当前时间
        
        返回:
            匹配的 Script 对象
        """
        ...
```

---

## 1.5 层级 C: 表现层 (The Body) — 详细设计

### AsciiRenderer 类定义

```python
# src/ui/ascii_renderer.py

import numpy as np
from PIL import Image
from pathlib import Path


class AsciiRenderer:
    """
    ASCII 渲染引擎
    
    职责：
    - 将图片转换为彩色 HTML ASCII 字符串
    - 处理透明度遮罩（自动扣除背景色）
    - 输出可直接放入 QLabel 的 HTML 富文本
    
    渲染流程：
    1. 加载图片 → PIL.Image
    2. 降采样 → resize 到目标字符数
    3. 遍历像素 → 灰度映射到 ASCII 字符
    4. 颜色映射 → 每个字符附带 HTML <span> 颜色
    5. 透明遮罩 → 背景色像素替换为 &nbsp;
    """
    
    # ASCII 字符集：从密到疏（暗 → 亮）
    CHARSET: str = "@%#*+=-:. "
    
    # 等宽字体的高宽比修正系数（防止 ASCII 画被纵向拉伸）
    ASPECT_RATIO_CORRECTION: float = 0.55
    
    # 背景色判定阈值（R+G+B 之和低于此值视为背景）
    BG_THRESHOLD: int = 30
    
    def __init__(self, width: int = 80):
        """
        参数:
            width: 输出 ASCII 画的字符宽度（列数）
        """
        self._width = width
    
    def render_image(self, image_path: Path) -> str:
        """
        将图片文件渲染为 HTML ASCII 字符串
        
        参数:
            image_path: 图片文件路径（支持 PNG/JPG/GIF 单帧）
        
        返回:
            HTML 富文本字符串，格式如:
            <pre style="font-family: Consolas, monospace; font-size: 8px; line-height: 1.0;">
            <span style="color: rgb(255,200,180);">@</span>
            <span style="color: rgb(100,50,20);">#</span>
            ...
            </pre>
        
        异常:
            FileNotFoundError: 图片文件不存在
            ValueError: 图片格式不支持
        """
        ...
    
    def render_gif_frames(self, gif_path: Path) -> list[str]:
        """
        将 GIF 动画的每一帧渲染为 HTML ASCII 字符串列表
        
        参数:
            gif_path: GIF 文件路径
            
        返回:
            HTML 字符串列表，每个元素对应一帧
        """
        ...
    
    def _pixel_to_char(self, r: int, g: int, b: int) -> tuple[str, bool]:
        """
        单像素 → ASCII 字符转换
        
        参数:
            r, g, b: 像素 RGB 值 (0-255)
        
        返回:
            (字符, 是否透明) 元组
            - 字符: 映射后的 ASCII 字符
            - 是否透明: True 表示该像素应被视为背景
        
        映射算法:
            1. 判断透明: R+G+B < BG_THRESHOLD → 返回 ("&nbsp;", True)
            2. 计算灰度: Gray = 0.299*R + 0.587*G + 0.114*B
            3. 映射字符: index = int(Gray / 255 * (len(CHARSET) - 1))
            4. 返回: (CHARSET[index], False)
        """
        ...
```

### EntityWindow 类定义

```python
# src/ui/entity_window.py

from PySide6.QtWidgets import QWidget, QLabel, QVBoxLayout
from PySide6.QtCore import (
    Qt, QPropertyAnimation, QEasingCurve, 
    QPoint, QSequentialAnimationGroup, QTimer
)
from PySide6.QtGui import QFont


class EntityWindow(QWidget):
    """
    实体窗口 — 角色的物理载体
    
    窗口属性：
    - 无边框 (FramelessWindowHint)
    - 总在最前 (WindowStaysOnTopHint)
    - 背景透明 (WA_TranslucentBackground)
    - 不显示任务栏图标 (Tool)
    
    职责：
    - 显示 ASCII 字符画（通过 QLabel + HTML）
    - 执行滑入/滑出动画
    - 管理动画队列
    """
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._setup_window_flags()
        self._setup_ui()
        self._setup_animations()
    
    def _setup_window_flags(self) -> None:
        """
        设置窗口标志位
        
        关键标志:
        - Qt.FramelessWindowHint: 去除标题栏和边框
        - Qt.WindowStaysOnTopHint: 置顶显示
        - Qt.Tool: 不在任务栏显示
        - Qt.WA_TranslucentBackground: 背景透明
        - Qt.WA_ShowWithoutActivating: 显示时不抢夺焦点
        """
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint
            | Qt.WindowType.WindowStaysOnTopHint
            | Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground, True)
        self.setAttribute(Qt.WidgetAttribute.WA_ShowWithoutActivating, True)
    
    def _setup_ui(self) -> None:
        """
        初始化 UI 组件
        
        核心组件:
        - QLabel: 显示 HTML 格式的 ASCII 字符画
        - 字体: 必须使用等宽字体 (Consolas/Courier New)
        """
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        self._label = QLabel()
        self._label.setTextFormat(Qt.TextFormat.RichText)
        self._label.setFont(QFont("Consolas", 8))
        self._label.setStyleSheet("background: transparent;")
        layout.addWidget(self._label)
    
    def _setup_animations(self) -> None:
        """
        初始化动画系统
        
        动画类型:
        - slide_in: 滑入动画 (OutBack 曲线, 1.5s)
        - slide_out: 滑出动画 (InExpo 曲线, 0.3s)
        - peek: 探头动画 (OutBack 曲线, 0.8s)
        """
        self._slide_anim = QPropertyAnimation(self, b"pos")
    
    def summon(self, edge: str, y_position: int, script) -> None:
        """
        召唤角色出场
        
        动画序列:
        1. 设置初始位置（屏幕外）
        2. 显示窗口
        3. 播放滑入动画
        
        参数:
            edge: 出场边缘 ("left" / "right")
            y_position: 垂直位置（像素）
            script: 台词对象
        """
        ...
    
    def flee(self) -> None:
        """
        角色逃跑
        
        动画序列:
        1. 停止当前动画
        2. 播放快速滑出动画 (InExpo, 0.3s)
        3. 动画完成后隐藏窗口
        """
        ...
    
    def set_ascii_content(self, html: str) -> None:
        """
        更新显示内容
        
        参数:
            html: AsciiRenderer 生成的 HTML 字符串
        """
        self._label.setText(html)
        self.adjustSize()
```

---

## 1.6 高 DPI 适配 (High DPI Support)

### 问题描述

Windows 缩放比例（如 125%、150%）会导致：
1. ASCII 字符画模糊
2. 窗口坐标偏移
3. 字体渲染异常

### 解决方案

在 `main.py` 入口文件的最开头（任何 Qt 导入之前）：

```python
# src/main.py — 必须在最顶部

import os
import sys

# ── 高 DPI 适配 ── 必须在 QApplication 创建之前设置 ──
os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "1"
os.environ["QT_SCALE_FACTOR_ROUNDING_POLICY"] = "PassThrough"

from PySide6.QtWidgets import QApplication
from PySide6.QtCore import Qt

def main():
    app = QApplication(sys.argv)
    
    # 确保使用逻辑像素而非物理像素进行坐标计算
    # 获取屏幕时使用 screen.availableGeometry() 而非硬编码分辨率
    
    # ... 初始化各模块 ...
    
    sys.exit(app.exec())

if __name__ == "__main__":
    main()
```

---

## 1.7 Phase 1 开发路线图 (MVP Roadmap)

### 严格执行顺序

> ⚠️ **每个 Step 必须完成所有验收标准后才能进入下一个 Step**

#### Step 1: 打造"幽灵窗口" (The Ghost Window)

**文件**: `src/ui/entity_window.py`, `src/ui/ascii_renderer.py`

**目标**: 实现一个能在屏幕上显示的、透明背景的静态彩色 ASCII 窗口。

**实现要点**:
1. 创建 `EntityWindow` 类，设置所有窗口标志位
2. 创建 `AsciiRenderer` 类，实现图片 → HTML ASCII 转换
3. 实现"黑色背景自动扣除"算法（`_pixel_to_char` 方法）
4. 处理高 DPI 缩放问题

**验收标准**:
- [ ] 窗口无边框、背景透明、置顶显示
- [ ] 不在任务栏显示图标
- [ ] 彩色 ASCII 字符画正常显示，无模糊
- [ ] 黑色背景被正确扣除（可透过字符看到桌面壁纸）
- [ ] 在 100%、125%、150% 缩放下均正常显示

**测试代码**:
```python
# tests/test_ghost_window.py
if __name__ == "__main__":
    app = QApplication(sys.argv)
    renderer = AsciiRenderer(width=60)
    html = renderer.render_image(Path("assets/sprites/test.png"))
    
    window = EntityWindow()
    window.set_ascii_content(html)
    window.move(500, 300)  # 放在屏幕中央
    window.show()
    
    sys.exit(app.exec())
```

---

#### Step 2: 接入"神经系统" (The Idle Detector)

**文件**: `src/core/idle_monitor.py`

**目标**: 实现 Windows API 空闲检测，能在控制台实时打印空闲时间。

**实现要点**:
1. 创建 `IdleMonitor` 类（完整代码见 1.3 节）
2. 正确调用 `User32.dll` 的 `GetLastInputInfo`
3. 处理 `GetTickCount` 时间戳溢出（49.7 天上限）
4. 实现状态机 (STANDBY → PRE_IDLE → IDLE_TRIGGERED → ACTIVE)

**验收标准**:
- [ ] 控制台每秒打印一次空闲时间（格式: `"空闲: 15.3s"`）
- [ ] 空闲超过设定阈值后触发信号
- [ ] 鼠标/键盘操作后立即重置计时
- [ ] 后台线程 CPU 占用 < 1%
- [ ] 不影响其他应用的输入接收

**测试代码**:
```python
# tests/test_idle_monitor.py
if __name__ == "__main__":
    app = QApplication(sys.argv)
    
    monitor = IdleMonitor(threshold_ms=10000)  # 10秒阈值（测试用）
    monitor.idle_time_updated.connect(
        lambda ms: print(f"空闲: {ms/1000:.1f}s")
    )
    monitor.user_idle_confirmed.connect(
        lambda: print(">>> 用户已离开！")
    )
    monitor.user_active_detected.connect(
        lambda: print(">>> 用户回来了！")
    )
    monitor.start()
    
    sys.exit(app.exec())
```

---

#### Step 3: 赋予"行动能力" (The Animation)

**文件**: `src/ui/entity_window.py` (扩展 `summon` 和 `flee` 方法)

**目标**: 将 Step 1 和 Step 2 结合。窗口平时隐藏，空闲触发后从屏幕边缘滑入。

**实现要点**:
1. 使用 `QPropertyAnimation` 动画系统
2. 使用 `QEasingCurve.OutBack` 曲线（弹性探头效果）
3. 使用 `QSequentialAnimationGroup` 串联动画序列
4. 逃跑动画使用 `QEasingCurve.InExpo` 曲线（极速缩回）

**动画参数表**:

| 动画 | 持续时间 | 缓动曲线 | 起点 X | 终点 X | 语义 |
|------|---------|----------|--------|--------|------|
| 探头 (Peek) | 1500ms | OutBack | ScreenWidth | ScreenWidth - 100 | 小心翼翼探出头 |
| 登场 (Enter) | 800ms | OutBounce | ScreenWidth - 100 | ScreenWidth - 350 | 开心跳出来 |
| 逃跑 (Flee) | 300ms | InExpo | 当前位置 | ScreenWidth | 被发现了！赶紧溜 |

**验收标准**:
- [ ] 空闲触发后，窗口从屏幕右侧缓慢探出
- [ ] 探出后停顿 2 秒，然后完全滑入
- [ ] 移动鼠标后，窗口极速缩回并隐藏
- [ ] 动画流畅无卡顿（≥ 60fps）
- [ ] 动画过程中不影响其他应用操作

---

#### Step 4: 赋予"声音" (The Voice)

**文件**: `src/core/audio_manager.py`

**目标**: 接入 `edge-tts` 语音合成，角色出场时能说话。

**实现要点**:
1. 实现 `AudioManager` 类（Cache-First 策略）
2. 异步调用 `edge-tts` API（Worker Thread）
3. 使用 `QMediaPlayer` 或 `miniaudio` 播放本地文件
4. 实现音频队列和优先级中断机制

**验收标准**:
- [ ] 角色出场后能播放对应时段的语音
- [ ] 语音播放不阻塞 GUI（角色动画和语音同步进行）
- [ ] 同一台词的音频自动缓存，二次触发无网络请求
- [ ] 逃跑时语音立即停止
- [ ] 无网络时不崩溃，静默跳过语音功能

---

## 1.8 项目初始化命令 (Project Bootstrap)

```powershell
# 1. 创建项目目录结构
mkdir CyberCompanion-Project
cd CyberCompanion-Project
mkdir src\core, src\ui, src\ai, assets\sprites, assets\sounds, assets\models, characters, tests, tools

# 2. 创建 Python 虚拟环境
python -m venv .venv
.\.venv\Scripts\Activate.ps1

# 3. 安装依赖
pip install PySide6>=6.6.0 Pillow>=10.0.0 numpy>=1.24.0 edge-tts>=6.1.0 pywin32>=306

# 4. 冻结依赖
pip freeze > requirements.txt

# 5. 创建入口文件
New-Item src\main.py -ItemType File
New-Item src\core\__init__.py -ItemType File
New-Item src\ui\__init__.py -ItemType File
New-Item src\core\idle_monitor.py -ItemType File
New-Item src\core\director.py -ItemType File
New-Item src\core\asset_manager.py -ItemType File
New-Item src\core\audio_manager.py -ItemType File
New-Item src\ui\entity_window.py -ItemType File
New-Item src\ui\ascii_renderer.py -ItemType File
```

---

## 1.9 Phase 2 & 3 预告

下一阶段将深入实现以下高级功能（详见 Phase 2-3 文档）：
- **视觉暂留系统 (Ghosting)**: 利用 `QGraphicsEffect` 实现赛博朋克残影效果
- **视觉感知 (OpenCV/MediaPipe)**: 摄像头人脸检测，角色的眼神跟随你的位置移动
- **大语言模型 (LLM)**: OCR 识别屏幕文字 + LLM 生成吐槽评论
