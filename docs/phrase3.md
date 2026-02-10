# 📝 Project Cyber-Companion 开发文档 (Phase 3)

## —— 表达与行为逻辑：声音与灵魂

---

## 3.0 阶段概述

前两部分构建了**躯体（GUI 与渲染）**和**神经（输入检测）**，本阶段构建角色的**"表达与行为逻辑"**。

| 子系统 | 核心问题 | 实现目标 |
|--------|---------|---------|
| **听觉系统** | 如何说话？ | 低延迟、非阻塞的 TTS 语音合成与播放 |
| **行为系统** | 如何思考？ | 有限状态机 (FSM) 驱动的行为逻辑 |
| **个性化系统** | 如何有"性格"？ | 基于规则的上下文感知脚本引擎 |

### 前置依赖

- Phase 1-2 中的 `IdleMonitor`、`EntityWindow`、`AsciiRenderer` 均已通过验收
- `edge-tts` 库已安装并可正常联网调用

---

## 3.1 听觉系统设计 (The Auditory System)

### 目标

实现**低延迟、非阻塞**的语音交互，并赋予角色"上下文感知"的说话能力。

### 3.1.1 语音合成架构 (TTS Pipeline)

> ⚠️ **GUI 主线程不能有任何阻塞操作**。音频生成和播放必须在独立线程中完成。

#### 缓存优先策略 (Cache-First)

```
┌──────────────────────────────────────────────────────────┐
│                    TTS Pipeline                          │
│                                                          │
│  ┌────────────┐     ┌──────────────┐     ┌──────────┐   │
│  │ Director   │────►│ AudioManager │────►│ 播放器   │   │
│  │ (触发台词)  │     │ (单例)        │     │          │   │
│  └────────────┘     └──────┬───────┘     └──────────┘   │
│                            │                             │
│                     ┌──────▼───────┐                     │
│                     │ 检查本地缓存  │                     │
│                     │ cache/audio/ │                     │
│                     └──────┬───────┘                     │
│                            │                             │
│               ┌────────────┼────────────┐                │
│               ▼                         ▼                │
│        ┌──────────┐              ┌──────────┐            │
│        │ Cache Hit│              │Cache Miss│            │
│        │ 直接播放  │              │          │            │
│        └──────────┘              └────┬─────┘            │
│                                      ▼                   │
│                               ┌──────────────┐           │
│                               │ Worker Thread│           │
│                               │ - edge-tts   │           │
│                               │ - async 生成  │           │
│                               │ - 写入缓存    │           │
│                               └──────┬───────┘           │
│                                      ▼                   │
│                               ┌──────────────┐           │
│                               │   播放音频    │           │
│                               └──────────────┘           │
└──────────────────────────────────────────────────────────┘
```

### 3.1.2 AudioManager 完整实现

```python
# src/core/audio_manager.py

import asyncio
import hashlib
from pathlib import Path
from enum import IntEnum
from typing import Optional
from PySide6.QtCore import QObject, QThread, Signal
from PySide6.QtMultimedia import QMediaPlayer, QAudioOutput


class AudioPriority(IntEnum):
    """
    音频优先级枚举
    
    数值越小优先级越高
    """
    CRITICAL = 0   # 惊吓、逃跑 → 立即停止当前播放并播放此音频
    HIGH = 1       # 角色台词 → 如果当前有播放则排队
    NORMAL = 2     # 环境音效 → 排队播放
    LOW = 3        # 呼吸声等 → 可被任何更高优先级打断


class TTSWorker(QThread):
    """
    TTS 生成工作线程
    
    职责:
    - 在后台线程中调用 edge-tts API (基于 asyncio)
    - 生成完成后将音频文件路径通过信号返回
    - 自动将生成的音频缓存到本地
    
    信号:
    - audio_ready(str): 音频文件路径
    - generation_failed(str): 错误信息
    """
    
    audio_ready = Signal(str)         # 参数: 音频文件路径
    generation_failed = Signal(str)   # 参数: 错误信息
    
    def __init__(
        self,
        text: str,
        voice: str,
        cache_dir: Path,
        rate: str = "+0%",
        parent=None
    ):
        """
        参数:
            text: 要合成的文本
            voice: 语音包名称 (如 "zh-CN-XiaoxiaoNeural")
            cache_dir: 音频缓存目录
            rate: 语速调节 (如 "+20%", "-10%")
        """
        super().__init__(parent)
        self._text = text
        self._voice = voice
        self._cache_dir = cache_dir
        self._rate = rate
    
    def run(self) -> None:
        """在工作线程中执行 TTS 生成"""
        try:
            # 生成缓存文件名（基于文本内容的 MD5 哈希）
            cache_key = hashlib.md5(
                f"{self._text}_{self._voice}_{self._rate}".encode()
            ).hexdigest()
            cache_path = self._cache_dir / f"{cache_key}.mp3"
            
            # 检查缓存
            if cache_path.exists():
                self.audio_ready.emit(str(cache_path))
                return
            
            # 确保缓存目录存在
            self._cache_dir.mkdir(parents=True, exist_ok=True)
            
            # 调用 edge-tts（需要在新的事件循环中运行）
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                loop.run_until_complete(
                    self._generate_audio(cache_path)
                )
                self.audio_ready.emit(str(cache_path))
            finally:
                loop.close()
        
        except Exception as e:
            self.generation_failed.emit(str(e))
    
    async def _generate_audio(self, output_path: Path) -> None:
        """
        调用 edge-tts API 生成音频
        
        edge-tts 使用方法:
            import edge_tts
            communicate = edge_tts.Communicate(text, voice, rate=rate)
            await communicate.save(str(output_path))
        """
        import edge_tts
        
        communicate = edge_tts.Communicate(
            text=self._text,
            voice=self._voice,
            rate=self._rate
        )
        await communicate.save(str(output_path))


class AudioManager(QObject):
    """
    音频管理器 — 单例模式
    
    职责:
    1. 管理 TTS 生成工作线程
    2. 维护音频播放队列
    3. 处理优先级和打断逻辑
    4. 管理本地音频缓存
    
    使用示例:
        audio_mgr = AudioManager(cache_dir=Path("cache/audio"))
        audio_mgr.speak("你好世界", priority=AudioPriority.NORMAL)
        audio_mgr.interrupt()  # 立即停止播放
    """
    
    playback_started = Signal(str)   # 参数: 正在播放的文本
    playback_finished = Signal()     # 播放完成
    
    # TTS 配置默认值
    DEFAULT_VOICE = "zh-CN-XiaoxiaoNeural"
    DEFAULT_RATE = "+0%"
    
    def __init__(self, cache_dir: Path, parent=None):
        """
        参数:
            cache_dir: 音频缓存目录 (如 Path("cache/audio"))
        """
        super().__init__(parent)
        self._cache_dir = cache_dir
        self._voice = self.DEFAULT_VOICE
        self._rate = self.DEFAULT_RATE
        
        # 播放器
        self._player = QMediaPlayer()
        self._audio_output = QAudioOutput()
        self._player.setAudioOutput(self._audio_output)
        self._audio_output.setVolume(0.8)
        
        # 播放队列
        self._queue: list[tuple[str, AudioPriority]] = []
        self._current_worker: Optional[TTSWorker] = None
        self._is_playing = False
        
        # 连接播放完成信号
        self._player.mediaStatusChanged.connect(self._on_media_status_changed)
    
    def speak(
        self,
        text: str,
        priority: AudioPriority = AudioPriority.NORMAL,
        cached_path: Optional[str] = None
    ) -> None:
        """
        播放语音
        
        参数:
            text: 要说的台词
            priority: 优先级
            cached_path: 预缓存的音频文件路径 (可选)
        
        行为:
        - CRITICAL 优先级: 立即停止当前播放，插队播放
        - HIGH/NORMAL 优先级: 加入队列排队
        - LOW 优先级: 如果队列为空则播放，否则丢弃
        """
        if priority == AudioPriority.CRITICAL:
            self._player.stop()
            self._queue.clear()
            self._play_text(text, cached_path)
        elif priority == AudioPriority.LOW and self._is_playing:
            return  # 低优先级时有更重要的音频在播放，丢弃
        else:
            self._queue.append((text, priority))
            if not self._is_playing:
                self._play_next()
    
    def interrupt(self) -> None:
        """
        立即停止所有音频播放
        
        场景: 用户移动鼠标触发逃跑，需要立即静音
        """
        self._player.stop()
        self._queue.clear()
        self._is_playing = False
        if self._current_worker and self._current_worker.isRunning():
            self._current_worker.terminate()
            self._current_worker = None
    
    def set_voice(self, voice: str) -> None:
        """
        切换语音包
        
        可用语音包列表:
        - "zh-CN-XiaoxiaoNeural"   : 温暖女声 (推荐)
        - "zh-CN-XiaoyiNeural"     : 活泼女声
        - "zh-CN-YunxiNeural"      : 清亮男声
        - "zh-CN-YunjianNeural"    : 沉稳男声
        - "zh-TW-HsiaoChenNeural"  : 台湾腔女声
        - "ja-JP-NanamiNeural"     : 日语女声
        
        完整列表: https://learn.microsoft.com/en-us/azure/ai-services/speech-service/language-support
        """
        self._voice = voice
    
    def set_volume(self, volume: float) -> None:
        """
        设置音量
        
        参数:
            volume: 0.0 (静音) ~ 1.0 (最大)
        """
        self._audio_output.setVolume(max(0.0, min(1.0, volume)))
    
    def _play_text(self, text: str, cached_path: Optional[str] = None) -> None:
        """生成/加载音频并播放"""
        if cached_path and Path(cached_path).exists():
            self._start_playback(cached_path)
        else:
            worker = TTSWorker(
                text=text,
                voice=self._voice,
                cache_dir=self._cache_dir,
                rate=self._rate
            )
            worker.audio_ready.connect(self._start_playback)
            worker.generation_failed.connect(self._on_generation_failed)
            self._current_worker = worker
            worker.start()
    
    def _start_playback(self, file_path: str) -> None:
        """开始播放音频文件"""
        from PySide6.QtCore import QUrl
        self._player.setSource(QUrl.fromLocalFile(file_path))
        self._player.play()
        self._is_playing = True
        self.playback_started.emit(file_path)
    
    def _play_next(self) -> None:
        """播放队列中的下一个"""
        if self._queue:
            text, priority = self._queue.pop(0)
            self._play_text(text)
        else:
            self._is_playing = False
            self.playback_finished.emit()
    
    def _on_media_status_changed(self, status) -> None:
        """媒体状态变化回调"""
        if status == QMediaPlayer.MediaStatus.EndOfMedia:
            self._play_next()
    
    def _on_generation_failed(self, error: str) -> None:
        """
        TTS 生成失败处理
        
        不崩溃，静默跳过，播放下一条
        """
        print(f"[AudioManager] TTS 生成失败: {error}")
        self._play_next()
```

### 3.1.3 TTS 技术选型详情

| 属性 | 值 |
|------|-----|
| **引擎** | `edge-tts` (Python 库) |
| **原理** | 调用微软 Edge 浏览器内置的在线 TTS 接口 |
| **费用** | 完全免费，无需 API Key |
| **限制** | 需要联网；微软可能随时变更或限制接口 |
| **推荐语音** | `zh-CN-XiaoxiaoNeural` (温暖女声) |
| **备选语音** | `zh-CN-YunxiNeural` (清亮男声) |
| **输出格式** | MP3 |
| **典型延迟** | 1-3 秒（首次生成），0ms（缓存命中） |

### 3.1.4 验收标准

| # | 验收项 | 测试方法 | 预期结果 |
|---|--------|---------|---------|
| 1 | TTS 生成 | 调用 `speak("测试语音")` | 1-3 秒后听到中文语音 |
| 2 | 缓存命中 | 再次调用相同文本 | 立即播放，无网络请求 |
| 3 | 非阻塞 | TTS 生成期间操作 GUI | GUI 无卡顿 |
| 4 | 中断 | 播放中调用 `interrupt()` | 语音立即停止 |
| 5 | 优先级 | CRITICAL 优先级播放 | 打断当前播放，立即切换 |
| 6 | 队列 | 连续调用 3 次 `speak()` | 依次播放，不重叠 |
| 7 | 离线降级 | 断开网络后触发 TTS | 不崩溃，静默跳过 |
| 8 | 音频格式 | 检查缓存文件 | MP3 格式，可被系统播放器打开 |

---

## 3.2 行为逻辑核心：有限状态机 (FSM)

### 目标

让角色的行为**可预测但又充满变数**，避免"机械感"。

### 3.2.1 状态定义 (State Definitions)

```python
# src/core/state_machine.py

from enum import Enum, auto
from typing import Callable, Optional
from PySide6.QtCore import QObject, Signal


class EntityState(Enum):
    """
    角色状态枚举
    
    状态流转图:
    
    ┌────────┐  idle > threshold  ┌────────┐  无操作 5s  ┌──────────┐
    │ HIDDEN │ ──────────────────►│PEEKING │ ──────────►│ ENGAGED  │
    │  (S0)  │                    │  (S1)  │             │   (S2)   │
    └────┬───┘                    └───┬────┘             └────┬─────┘
         │                            │                       │
         │  ◄──── 动画完成 ────────── │  ◄── 鼠标移动 ──────┘
         │                            │                       │
         │                       ┌────▼────┐                  │
         │  ◄── 动画完成 ────── │ FLEEING │ ◄── 鼠标移动 ────┘
         │                       │  (S3)   │
         │                       └─────────┘
         │
         │  ◄──── 超时 (30s) ───────────────── (S2)
    """
    
    HIDDEN = auto()     # S0: 潜行态 — 完全不可见
    PEEKING = auto()    # S1: 窥视态 — 从边缘探出半个身体
    ENGAGED = auto()    # S2: 交互态 — 完全出现在屏幕上
    FLEEING = auto()    # S3: 逃逸态 — 受惊缩回
```

### 3.2.2 每个状态的详细行为规范

#### S0: HIDDEN (潜行态)

```python
class HiddenState:
    """
    S0: 潜行态
    
    描述: 
        窗口完全移出屏幕外 (x > ScreenWidth)，透明度 0。
        用户完全无法感知程序的存在。
    
    运行中的系统:
        ✅ IdleMonitor (后台 Daemon)
        ❌ AsciiRenderer (不需渲染)
        ❌ AudioManager (静默)
        ❌ CV/MediaPipe (未启用)
    
    资源占用:
        - CPU: < 0.5%
        - 内存: < 30MB
    
    退出条件:
        ├── IdleMonitor.idle_time > threshold
        │   └── 转入 → S1 (PEEKING)
        └── 用户右键托盘图标选择"召唤"
            └── 转入 → S2 (ENGAGED) [直接跳过窥视]
    """
    pass
```

#### S1: PEEKING (窥视态)

```python
class PeekingState:
    """
    S1: 窥视态
    
    描述:
        窗口从屏幕边缘探出约 1/3 身体。
        角色做出试探性的动作，观察用户是否真的离开。
    
    入场动画:
        - 曲线: OutBack
        - 时长: 1500ms
        - X: screen_width → screen_width - 100
        
    运行中的行为:
        ✅ 播放"探头"ASCII 动画 (peek.gif 帧循环)
        ✅ 可选: 播放轻微音效 (衣服摩擦声)
        ✅ 启动 5 秒倒计时
    
    退出条件:
        ├── 用户鼠标/键盘活动
        │   └── 立即转入 → S3 (FLEEING)
        ├── 5 秒无操作
        │   └── 转入 → S2 (ENGAGED)
        └── 手动取消
            └── 转入 → S0 (HIDDEN)
    
    ⚠️ 注意:
        此状态下不播放语音（太早说话会显得突兀）
    """
    
    TIMEOUT_SECONDS = 5  # 窥视持续时间
    pass
```

#### S2: ENGAGED (交互态)

```python
class EngagedState:
    """
    S2: 交互态
    
    描述:
        窗口完全滑入屏幕，角色停留在屏幕一角。
        这是角色的"主场时间"，执行所有交互行为。
    
    入场动画:
        - 曲线: OutBounce
        - 时长: 800ms
        - X: screen_width - 100 → screen_width - 350
    
    运行中的行为:
        ✅ 循环播放 Idle_Animation (呼吸/眨眼 ASCII 帧循环)
        ✅ 执行 Script_Engine:
           - 根据当前时间段选择台词
           - 调用 TTS 引擎生成并播放语音
        ✅ 监听用户交互 (鼠标点击角色)
    
    退出条件:
        ├── 用户鼠标/键盘活动 (且不是点击角色本身)
        │   └── 转入 → S3 (FLEEING)
        ├── 自然超时 (auto_dismiss_seconds 秒后自动离开)
        │   └── 转入 → S0 (HIDDEN) [播放告别动画]
        └── 用户点击角色
            └── 留在 S2，触发交互菜单
    
    超时机制:
        - 默认 30 秒后自动离开
        - 每次与角色互动重置计时器
    """
    
    AUTO_DISMISS_SECONDS = 30
    pass
```

#### S3: FLEEING (逃逸态)

```python
class FleeingState:
    """
    S3: 逃逸态
    
    描述:
        角色受惊，快速缩回屏幕外。
        这是一个瞬时过渡状态，持续时间极短。
    
    行为序列:
        1. 中断所有正在播放的 TTS 音频
        2. 停止所有待机动画
        3. 播放 Panic_Animation (惊吓表情 ASCII 帧, 持续 200ms)
        4. 播放 Panic_Voice ("哇！被发现了！") [CRITICAL 优先级]
        5. 执行逃跑动画:
           - 曲线: InExpo
           - 时长: 300ms
           - X: 当前位置 → screen_width
    
    退出条件:
        └── 逃跑动画播放完毕
            └── 转入 → S0 (HIDDEN)
    
    ⚠️ 注意:
        - 此状态不可被打断（已经在逃跑了，不能更快）
        - 惊叫语音可能在窗口隐藏后仍在播放（有延迟效果更好）
    """
    pass
```

### 3.2.3 状态机实现

```python
class StateMachine(QObject):
    """
    有限状态机 (FSM) 控制器
    
    职责:
    - 管理状态流转
    - 执行状态进入/退出回调
    - 防止非法状态跳转
    
    信号:
    - state_changed(EntityState, EntityState): (旧状态, 新状态)
    """
    
    state_changed = Signal(EntityState, EntityState)
    
    # 合法的状态转换表
    VALID_TRANSITIONS: dict[EntityState, list[EntityState]] = {
        EntityState.HIDDEN:  [EntityState.PEEKING, EntityState.ENGAGED],
        EntityState.PEEKING: [EntityState.ENGAGED, EntityState.FLEEING, EntityState.HIDDEN],
        EntityState.ENGAGED: [EntityState.FLEEING, EntityState.HIDDEN],
        EntityState.FLEEING: [EntityState.HIDDEN],
    }
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._current_state = EntityState.HIDDEN
        self._callbacks: dict[EntityState, Callable] = {}
    
    @property
    def current_state(self) -> EntityState:
        """获取当前状态"""
        return self._current_state
    
    def register_state_handler(
        self,
        state: EntityState,
        on_enter: Optional[Callable] = None,
        on_exit: Optional[Callable] = None
    ) -> None:
        """
        注册状态进入/退出的回调函数
        
        参数:
            state: 目标状态
            on_enter: 进入该状态时调用
            on_exit: 离开该状态时调用
        """
        self._callbacks[state] = {
            "enter": on_enter,
            "exit": on_exit
        }
    
    def transition_to(self, new_state: EntityState) -> bool:
        """
        请求状态转换
        
        参数:
            new_state: 目标状态
        
        返回:
            True: 转换成功
            False: 非法转换，已被拒绝
        
        行为:
            1. 验证转换是否合法
            2. 调用旧状态的 on_exit
            3. 更新当前状态
            4. 调用新状态的 on_enter
            5. 发出 state_changed 信号
        """
        if new_state not in self.VALID_TRANSITIONS.get(self._current_state, []):
            print(
                f"[FSM] 非法状态转换: "
                f"{self._current_state.name} → {new_state.name}"
            )
            return False
        
        old_state = self._current_state
        
        # 退出旧状态
        if old_state in self._callbacks:
            exit_fn = self._callbacks[old_state].get("exit")
            if exit_fn:
                exit_fn()
        
        # 更新状态
        self._current_state = new_state
        
        # 进入新状态
        if new_state in self._callbacks:
            enter_fn = self._callbacks[new_state].get("enter")
            if enter_fn:
                enter_fn()
        
        self.state_changed.emit(old_state, new_state)
        print(f"[FSM] 状态转换: {old_state.name} → {new_state.name}")
        return True
```

### 3.2.4 FSM 验收标准

| # | 验收项 | 测试方法 | 预期结果 |
|---|--------|---------|---------|
| 1 | 正常流转 | 触发完整流程 | HIDDEN→PEEKING→ENGAGED→FLEEING→HIDDEN |
| 2 | 非法转换 | 尝试 HIDDEN→FLEEING | 被拒绝，打印警告 |
| 3 | 回调执行 | 注册 on_enter/on_exit | 状态变化时正确调用 |
| 4 | 窥视中断 | PEEKING 时移动鼠标 | 直接跳到 FLEEING |
| 5 | 超时退出 | ENGAGED 状态等待 30 秒 | 自动回到 HIDDEN |

---

## 3.3 个性化模组 (Personality Module)

### 目标

让用户感觉不仅仅是个程序，而是一个有"性格"的伴侣。

### 3.3.1 上下文感知脚本引擎 (Context-Aware Scripting)

不使用硬编码字符串，构建一个**基于规则的查询引擎**。

#### 台词数据结构

```yaml
# characters/rem_maid/scripts/dialogue.yaml

# 台词库定义
# 每条台词可以有多个触发条件，且支持概率权重

scripts:
  # ── 深夜提醒 ──
  - id: "late_night_nagging"
    text: "两点了还不睡？这种由于缺乏睡眠导致的内分泌失调是不可逆的哦。"
    conditions:
      time_start: "02:00"
      time_end: "05:00"
      probability: 0.8           # 80% 概率触发
      cooldown_minutes: 30       # 30 分钟冷却
    tts:
      voice_override: null       # null 表示使用全局语音设置
      rate: "-10%"               # 说慢一点（深夜语气）
    animation:
      speed: "slow"
      sprite: "worried.gif"      # 使用担忧表情

  # ── 午餐提醒 ──
  - id: "lunch_break"
    text: "如果不去吃饭的话，下午的代码质量会下降 30%。"
    conditions:
      time_start: "12:00"
      time_end: "13:00"
      probability: 0.6
      cooldown_minutes: 60
    tts:
      rate: "+0%"
    animation:
      speed: "normal"
      sprite: "cheerful.gif"

  # ── 通用空闲 ──
  - id: "idle_generic_01"
    text: "三分钟没动了，是在发呆吗？"
    conditions:
      time_start: "default"      # 任意时间
      time_end: "default"
      probability: 1.0
      cooldown_minutes: 10
    animation:
      speed: "normal"
      sprite: "idle.gif"
```

#### 台词选择引擎

```python
# src/core/script_engine.py

from datetime import datetime
import random
from typing import Optional


class ScriptEngine:
    """
    台词选择引擎
    
    职责:
    - 根据当前时间匹配可用台词
    - 处理概率权重
    - 管理冷却时间
    - 避免连续重复同一台词
    
    查询优先级:
    1. 精确时间匹配 (如 02:00-05:00 的台词)
    2. 默认台词 (time_range == "default")
    3. 如果有多个匹配 → 根据 probability 加权随机选择
    4. 已冷却的台词被排除
    """
    
    def __init__(self, scripts: list[dict]):
        self._scripts = scripts
        self._last_played: dict[str, datetime] = {}  # {script_id: last_play_time}
        self._last_script_id: Optional[str] = None    # 上一次播放的台词 ID
    
    def select_script(self, now: Optional[datetime] = None) -> Optional[dict]:
        """
        选择一条合适的台词
        
        参数:
            now: 当前时间（可选，默认使用系统时间）
        
        返回:
            匹配的台词 dict，或 None（无可用台词）
        
        算法:
        1. 过滤出时间范围匹配的台词
        2. 过滤掉冷却中的台词
        3. 过滤掉上一次刚播放过的台词（避免连续重复）
        4. 根据 probability 加权随机选择一条
        """
        if now is None:
            now = datetime.now()
        
        candidates = []
        for script in self._scripts:
            # 检查时间范围
            if not self._is_time_match(script, now):
                continue
            # 检查冷却
            if self._is_cooling_down(script, now):
                continue
            # 避免连续重复
            if script["id"] == self._last_script_id and len(self._scripts) > 1:
                continue
            candidates.append(script)
        
        if not candidates:
            # 如果过滤太严格导致无候选，放宽条件（允许重复）
            candidates = [
                s for s in self._scripts
                if self._is_time_match(s, now) and not self._is_cooling_down(s, now)
            ]
        
        if not candidates:
            return None
        
        # 加权随机选择
        selected = self._weighted_random(candidates)
        
        # 记录
        self._last_played[selected["id"]] = now
        self._last_script_id = selected["id"]
        
        return selected
    
    def _is_time_match(self, script: dict, now: datetime) -> bool:
        """检查台词的时间范围是否匹配当前时间"""
        conditions = script.get("conditions", {})
        start = conditions.get("time_start", "default")
        end = conditions.get("time_end", "default")
        
        if start == "default" or end == "default":
            return True  # 默认台词始终匹配
        
        # 解析时间 "HH:MM"
        start_time = datetime.strptime(start, "%H:%M").time()
        end_time = datetime.strptime(end, "%H:%M").time()
        current_time = now.time()
        
        # 处理跨午夜的时间范围 (如 22:00-06:00)
        if start_time <= end_time:
            return start_time <= current_time <= end_time
        else:
            return current_time >= start_time or current_time <= end_time
    
    def _is_cooling_down(self, script: dict, now: datetime) -> bool:
        """检查台词是否在冷却中"""
        cooldown = script.get("conditions", {}).get("cooldown_minutes", 0)
        if cooldown == 0:
            return False
        
        last_time = self._last_played.get(script["id"])
        if last_time is None:
            return False
        
        elapsed = (now - last_time).total_seconds() / 60
        return elapsed < cooldown
    
    def _weighted_random(self, candidates: list[dict]) -> dict:
        """根据 probability 加权随机选择"""
        weights = [
            c.get("conditions", {}).get("probability", 1.0)
            for c in candidates
        ]
        return random.choices(candidates, weights=weights, k=1)[0]
```

### 3.3.2 随机性引擎 (Entropy Engine)

为了避免用户通过"每 3 分钟动一次鼠标"来**卡 BUG**，引入随机性：

```python
class EntropyEngine:
    """
    随机性引擎
    
    职责:
    - 为触发时间添加抖动 (Jitter)
    - 随机化表现位置
    - 避免机械感
    """
    
    @staticmethod
    def jitter_threshold(base_threshold_ms: int) -> int:
        """
        为空闲阈值添加随机抖动
        
        参数:
            base_threshold_ms: 基础阈值 (如 180000ms = 3分钟)
        
        返回:
            抖动后的阈值
        
        公式:
            actual = base + random(-30s, +60s)
            即实际触发时间在 2.5 ~ 4 分钟之间随机
        
        目的:
            防止用户准确预判触发时间
        """
        jitter = random.randint(-30000, 60000)  # -30s ~ +60s (毫秒)
        return max(60000, base_threshold_ms + jitter)  # 下限 1 分钟
    
    @staticmethod
    def random_y_position(screen_height: int) -> int:
        """
        随机化垂直位置
        
        参数:
            screen_height: 屏幕高度 (像素)
        
        返回:
            Y 坐标 (在屏幕高度的 20%-80% 范围内)
        
        目的:
            每次出现在不同位置，增加趣味性
        """
        return random.randint(
            int(screen_height * 0.2),
            int(screen_height * 0.8)
        )
```

---

## 3.4 资源打包与扩展性 (Asset Management)

### 目标

允许用户**自定义角色（换皮）**，如使用不同的 ASCII 角色图、声线、台词。

### 3.4.1 角色包目录结构规范

```
/characters
  /rem_maid/                      # 角色 ID (目录名即 ID)
    manifest.json                 # 元数据
    config.json                   # 角色个性化配置
    /assets
      /sprites
        idle.gif                  # 待机动画 (GIF, 每帧一个 ASCII 画)
        idle.png                  # 待机静帧 (备用)
        peek.png                  # 探头静帧
        panic.gif                 # 惊吓动画
        sleep.gif                 # 睡眠动画 (Phase 4 用)
      /sounds
        cloth_rustle.mp3          # 衣服摩擦音效 (可选)
    /scripts
      dialogue.yaml               # 台词库
    /voice_cache                   # TTS 预生成的语音文件 (自动填充)
```

#### `manifest.json` 规范

```json
{
    "id": "rem_maid",
    "name": "蕾姆・女仆装",
    "version": "1.0.0",
    "author": "YourName",
    "description": "从异世界前来的忠实女仆",
    "ascii_width": 60,
    "default_voice": "zh-CN-XiaoxiaoNeural",
    "preview_image": "assets/sprites/idle.png",
    "tags": ["anime", "maid", "cute"],
    "min_app_version": "1.0.0"
}
```

### 3.4.2 动态加载机制

```python
# src/core/character_loader.py

class CharacterLoader:
    """
    角色包加载器
    
    职责:
    - 程序启动时扫描 /characters 目录
    - 验证角色包完整性 (manifest + 必要文件)
    - 提供角色切换接口
    - 热重载: 切换角色时无需重启主程序
    
    使用方式:
        loader = CharacterLoader(Path("characters"))
        available = loader.list_characters()
        # → [{"id": "rem_maid", "name": "蕾姆・女仆装"}, ...]
        
        loader.load_character("rem_maid")
        # → 更新 AssetManager, AsciiRenderer, ScriptEngine
    """
    
    REQUIRED_FILES = [
        "manifest.json",
        "config.json",
        "scripts/dialogue.yaml",
        "assets/sprites/idle.gif",  # 或 idle.png
        "assets/sprites/peek.png",
    ]
    
    def __init__(self, characters_dir: Path):
        self._characters_dir = characters_dir
        self._loaded_characters: dict[str, dict] = {}
    
    def scan_characters(self) -> list[dict]:
        """
        扫描可用角色包
        
        返回: manifest 信息列表
        """
        result = []
        for char_dir in self._characters_dir.iterdir():
            if not char_dir.is_dir():
                continue
            manifest_path = char_dir / "manifest.json"
            if manifest_path.exists():
                # 验证必要文件
                if self._validate_character(char_dir):
                    with open(manifest_path, "r", encoding="utf-8") as f:
                        manifest = json.load(f)
                    result.append(manifest)
        return result
    
    def _validate_character(self, char_dir: Path) -> bool:
        """验证角色包是否包含所有必要文件"""
        for required in self.REQUIRED_FILES:
            if not (char_dir / required).exists():
                print(f"[CharacterLoader] 缺少文件: {char_dir / required}")
                return False
        return True
    
    def load_character(self, character_id: str):
        """
        加载指定角色包
        
        行为:
        1. 读取 manifest.json 和 config.json
        2. 重新初始化 AsciiRenderer (宽度可能不同)
        3. 重新加载 ScriptEngine (台词不同)
        4. 更新 AudioManager 的语音设置
        5. 预缓存角色的 sprite 帧
        """
        ...
```

---

## 3.5 隐私与安全边界 (Privacy & Safety)

> ⚠️ **强制要求** — 作为一个监控用户输入的程序，必须**自证清白**。

### 安全规则

| 规则 | 详细说明 | 实现方式 |
|------|---------|---------|
| **零上传** | 所有鼠标/键盘监听数据仅在内存中用于计算 IdleTime | 不写入磁盘、不发送网络请求 |
| **零记录** | 不记录用户按了什么键、移动了多远 | 只使用 `GetLastInputInfo` 的时间戳 |
| **全屏禁用** | 全屏应用运行时自动暂停弹出 | 通过 `GetForegroundWindow` + `GetWindowRect` 检测全屏 |
| **可退出** | 系统托盘图标必须提供"退出"选项 | 系统托盘右键菜单 |

### 全屏检测实现

```python
def _is_fullscreen_app_running(self) -> bool:
    """
    检测是否有全屏应用在运行（如游戏、PPT 放映）
    
    实现:
    1. 获取前台窗口句柄 (GetForegroundWindow)
    2. 获取该窗口矩形 (GetWindowRect)
    3. 获取屏幕尺寸 (GetSystemMetrics)
    4. 如果窗口矩形覆盖了整个屏幕 → 全屏模式
    
    返回:
        True: 有全屏应用，应暂停角色弹出
        False: 无全屏应用
    """
    user32 = ctypes.windll.user32
    hwnd = user32.GetForegroundWindow()
    
    rect = ctypes.wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    
    screen_w = user32.GetSystemMetrics(0)  # SM_CXSCREEN
    screen_h = user32.GetSystemMetrics(1)  # SM_CYSCREEN
    
    return (
        rect.left <= 0
        and rect.top <= 0
        and rect.right >= screen_w
        and rect.bottom >= screen_h
    )
```

---

## 3.6 本阶段验收总结

完成 Phase 3 后，系统应具备以下**完整功能**:

```
✅ 语音合成: 角色能根据台词内容说话 (edge-tts)
✅ 音频缓存: 相同台词不重复网络请求
✅ 行为状态机: HIDDEN → PEEKING → ENGAGED → FLEEING → HIDDEN
✅ 上下文台词: 不同时段说不同的话
✅ 随机性: 触发时间和位置每次不同
✅ 角色换皮: 支持加载不同角色包
✅ 全屏保护: 游戏/PPT 时不弹出
✅ 安全合规: 零上传、零记录
```