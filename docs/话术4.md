# 📝 Project Cyber-Companion 开发文档 (Phase 4)

## —— 高级交互与智能：视觉感知与认知核心

---

## 4.0 阶段概述 (Phase Overview)

如果说 Phase 1-3 造出了一个**"精致的机械人偶"**，那么 Phase 4 就是给它注入**"灵魂"**。

角色将从一个被动的反应程序，进化为一个**能看懂你、理解你的主动智能体**。

### 新增能力矩阵

| 能力 | 技术 | 效果 |
|------|------|------|
| **视觉感知** | MediaPipe Face Mesh | 知道你是否在看屏幕，眼神跟随你移动 |
| **存在检测** | 摄像头 + 键鼠融合判定 | 区分"用户真的离开"和"用户在看视频" |
| **屏幕理解** | OCR + 截屏 | 理解你当前屏幕上显示的内容 |
| **生成式对话** | LLM (本地/云端) | 根据上下文实时生成吐槽或安慰 |
| **动态人格** | 心情系统 + Prompt 调节 | 角色的语气随互动频率变化 |

### 前置依赖

- Phase 1-3 全部验收通过
- 摄像头硬件可用（USB 摄像头或笔记本内置）
- GPU 可选但不必需（MediaPipe 可在 CPU 上运行）

### 新增依赖

```txt
# requirements.txt (Phase 4 追加)
mediapipe>=0.10.0,<1.0.0       # Google 面部特征检测
mss>=9.0.0                      # 极速屏幕截图
paddleocr>=2.7.0                # OCR 文字识别（可选方案 A）
# 或使用 Windows 原生 OCR API (无需额外安装)

# LLM 方案 A (本地):
# ollama 需要单独安装: https://ollama.com
# 方案 B (云端):
openai>=1.0.0                   # OpenAI API 客户端
httpx>=0.24.0                   # HTTP 客户端（DeepSeek API）
```

---

## 4.1 视觉感知模块 (The "Eye" Module)

### 目标

利用摄像头实现非侵入式的**视线追踪**和**存在检测**，增强沉浸感。

### ⚠️ 隐私红线 (MANDATORY)

```
┌─────────────────────────────────────────────────────────┐
│                    隐私红线规则                           │
│                                                          │
│  1. 摄像头数据必须在本地内存中实时处理并立即丢弃            │
│  2. 严禁保存任何视频帧到磁盘                              │
│  3. 严禁上传任何摄像头数据到网络                           │
│  4. 仅在角色处于 PEEKING 或 ENGAGED 状态时激活摄像头       │
│  5. 用户必须能通过设置完全禁用摄像头功能                    │
│  6. 程序启动时必须获取摄像头授权提示                       │
│                                                          │
│  违反以上任何一条将导致严重的隐私和法律问题！               │
└─────────────────────────────────────────────────────────┘
```

### 4.1.1 技术选型

| 选项 | 库 | 优势 | 劣势 |
|------|-----|------|------|
| ✅ **推荐** | MediaPipe (Google) | 比 OpenCV Haar Cascade 更快更准；预训练的轻量级 Face Mesh 模型 (468 关键点)；CPU 实时运行 | 需要联网首次下载模型 |
| ❌ 不推荐 | OpenCV Haar Cascade | 传统方案 | 精度低、仅矩形检测 |
| ❌ 不推荐 | dlib | 精度高 | 需要编译 C++ 扩展 |

### 4.1.2 功能一：拟人化视线跟随 (Gaze Following)

**效果**: 当角色出现在屏幕上时，她的眼睛应该看着你。如果你移动头部，她的眼神应跟随移动。

#### 实现逻辑

```python
# src/ai/gaze_tracker.py

import cv2
import mediapipe as mp
import numpy as np
from PySide6.QtCore import QThread, Signal
from typing import Optional


class GazeData:
    """
    视线数据
    
    属性:
        face_detected: 是否检测到人脸
        face_x: 人脸中心的标准化 X 坐标 [-1.0 (左), 1.0 (右)]
        face_y: 人脸中心的标准化 Y 坐标 [-1.0 (上), 1.0 (下)]
        confidence: 检测置信度 [0.0, 1.0]
    """
    def __init__(
        self,
        face_detected: bool = False,
        face_x: float = 0.0,
        face_y: float = 0.0,
        confidence: float = 0.0
    ):
        self.face_detected = face_detected
        self.face_x = face_x
        self.face_y = face_y
        self.confidence = confidence


class GazeTracker(QThread):
    """
    视线追踪器 — 后台进程
    
    职责:
    - 以 15-30 FPS 读取摄像头帧
    - 使用 MediaPipe Face Mesh 检测人脸 468 个关键点
    - 计算人脸中心相对于画面的标准化坐标
    - 通过信号将 GazeData 发送给主线程
    
    资源管理:
    - 仅在 PEEKING 或 ENGAGED 状态时启动
    - 角色隐藏后立即释放摄像头
    - 检测帧率限制在 15 FPS（降低 CPU 占用）
    
    信号:
    - gaze_updated(GazeData): 每帧更新视线数据
    - camera_error(str): 摄像头错误
    """
    
    gaze_updated = Signal(object)   # GazeData
    camera_error = Signal(str)
    
    # 配置
    TARGET_FPS = 15
    MIN_DETECTION_CONFIDENCE = 0.5
    
    # MediaPipe Face Mesh 关键点索引
    # 完整列表: https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_geometry/data/canonical_face_model_uv_visualization.png
    LEFT_EYE_CENTER = 468    # 左瞳孔中心
    RIGHT_EYE_CENTER = 473   # 右瞳孔中心
    NOSE_TIP = 1             # 鼻尖
    
    def __init__(self, camera_index: int = 0, parent=None):
        """
        参数:
            camera_index: 摄像头索引 (0 = 默认摄像头)
        """
        super().__init__(parent)
        self._camera_index = camera_index
        self._running = False
    
    def run(self) -> None:
        """主循环 — 在独立线程中运行"""
        self._running = True
        
        cap = cv2.VideoCapture(self._camera_index)
        if not cap.isOpened():
            self.camera_error.emit("无法打开摄像头")
            return
        
        # 降低分辨率以提升性能
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
        
        face_mesh = mp.solutions.face_mesh.FaceMesh(
            max_num_faces=1,
            refine_landmarks=True,  # 启用瞳孔检测
            min_detection_confidence=self.MIN_DETECTION_CONFIDENCE,
            min_tracking_confidence=0.5,
        )
        
        frame_interval = 1.0 / self.TARGET_FPS
        
        try:
            while self._running:
                ret, frame = cap.read()
                if not ret:
                    continue
                
                # ⚠️ 内存安全：frame 在每次循环迭代后被覆盖，不会累积
                
                # 转换为 RGB（MediaPipe 要求）
                rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                results = face_mesh.process(rgb_frame)
                
                if results.multi_face_landmarks:
                    landmarks = results.multi_face_landmarks[0]
                    gaze_data = self._calculate_gaze(landmarks, frame.shape)
                else:
                    gaze_data = GazeData(face_detected=False)
                
                self.gaze_updated.emit(gaze_data)
                
                # 帧率控制
                import time
                time.sleep(frame_interval)
        
        finally:
            # ⚠️ 关键：确保摄像头被释放
            cap.release()
            face_mesh.close()
    
    def stop(self) -> None:
        """安全停止追踪"""
        self._running = False
        self.wait(3000)  # 等待最多 3 秒确保摄像头释放
    
    def _calculate_gaze(self, landmarks, frame_shape) -> GazeData:
        """
        计算视线方向
        
        算法:
        1. 提取鼻尖坐标作为人脸中心
        2. 将坐标从 [0,1] 归一化到 [-1,1]
           - x=0 → face_x=-1 (画面左边)
           - x=1 → face_x=+1 (画面右边)
        3. 由于摄像头画面是镜像的，需要翻转 X 坐标
        
        参数:
            landmarks: MediaPipe 人脸关键点
            frame_shape: 视频帧形状 (height, width, channels)
        
        返回:
            GazeData 对象
        """
        nose = landmarks.landmark[self.NOSE_TIP]
        
        # 归一化到 [-1, 1]
        # MediaPipe 输出的坐标已经是 [0, 1] 范围
        face_x = -(nose.x * 2.0 - 1.0)  # 翻转 X（摄像头镜像）
        face_y = nose.y * 2.0 - 1.0
        
        return GazeData(
            face_detected=True,
            face_x=np.clip(face_x, -1.0, 1.0),
            face_y=np.clip(face_y, -1.0, 1.0),
            confidence=nose.visibility if hasattr(nose, 'visibility') else 1.0
        )
```

#### 动态眼睛渲染

> ⚠️ **GIF 模式适用性说明**: 视线追踪通过 ASCII 字符画中的占位符 `{EYE_L}` / `{EYE_R}` 实现眼球移动。当角色使用 GIF Sprite 显示时（即 `Script.sprite_path` 有效），视线追踪**不生效**，因为 GIF 不支持字符级别的动态替换。此时 `Director._current_ascii_template` 为空字符串，`_on_gaze_updated` 方法会跳过渲染更新。这是**预期的设计行为**，不是 bug。

```python
# src/ui/ascii_renderer.py 新增方法

class AsciiRenderer:
    # ... (Phase 2 的代码) ...
    
    def apply_eye_tracking(
        self,
        ascii_html: str,
        face_x: float,
        eye_positions: list[tuple[int, int]],
        eye_width: int = 5
    ) -> str:
        """
        在 ASCII 字符画中实现动态眼球移动
        
        原理:
        - ASCII 画中眼睛区域是固定的字符位置
        - 根据 face_x 偏移量，移动眼球字符 "O" 的位置
        
        效果示例:
        - 你看向左边时:  | O   |
        - 你看向正前方:  |  O  |
        - 你看向右边时:  |   O |
        
        参数:
            ascii_html: 原始 HTML ASCII 字符画
            face_x: 人脸标准化 X 坐标 [-1.0, 1.0]
            eye_positions: 眼睛区域在 ASCII 矩阵中的 (row, col) 坐标列表
            eye_width: 眼睛可移动的字符宽度
        
        返回:
            修改后的 HTML 字符串
        
        实现方式:
        在 AsciiRenderer 中增加一个"动态层"，
        生成 ASCII 时预留眼睛占位符 {EYE_L} {EYE_R}，
        然后根据实时的 face_x 值替换占位符为偏移后的字符。
        """
        # 计算眼球偏移量
        offset = int(face_x * (eye_width // 2))
        
        # 生成偏移后的眼睛字符串
        eye_char = "O"
        padding = " " * eye_width
        eye_pos = max(0, min(eye_width - 1, (eye_width // 2) + offset))
        eye_str = padding[:eye_pos] + eye_char + padding[eye_pos + 1:]
        
        # 替换占位符
        result = ascii_html.replace("{EYE_L}", eye_str)
        result = result.replace("{EYE_R}", eye_str)
        
        return result
```

### 4.1.3 功能二："真·无人值守"检测 (True Presence Detection)

**问题**: Phase 2 中单纯依赖鼠标键盘有缺陷 — 用户在看电影时鼠标不动，但人其实在。

#### 融合检测逻辑

```python
# src/core/presence_detector.py

from enum import Enum, auto


class PresenceState(Enum):
    """用户存在状态"""
    PRESENT_ACTIVE = auto()     # 在场且活跃 (键鼠操作中)
    PRESENT_PASSIVE = auto()    # 在场但不操作 (看视频/思考)
    ABSENT = auto()             # 真的离开了
    UNKNOWN = auto()            # 无法判定 (摄像头异常)


class PresenceDetector:
    """
    多信号融合存在检测器
    
    职责:
    - 融合键鼠空闲时间和摄像头人脸检测
    - 输出更精准的"用户在不在"判断
    - 指导角色采取不同行为策略
    
    判定矩阵:
    ┌────────────────────┬────────────────┬──────────────────────┐
    │                    │ 人脸检测到     │ 人脸未检测到          │
    ├────────────────────┼────────────────┼──────────────────────┤
    │ 键鼠活跃 (<60s)    │ PRESENT_ACTIVE │ PRESENT_ACTIVE       │
    │                    │ (正常使用)     │ (低头操作/角度问题)    │
    ├────────────────────┼────────────────┼──────────────────────┤
    │ 键鼠空闲 (>5min)   │ PRESENT_PASSIVE│ ABSENT               │
    │                    │ (看视频/思考)   │ (真的离开了)          │
    └────────────────────┴────────────────┴──────────────────────┘
    
    行为映射:
    - PRESENT_ACTIVE   → 角色保持 HIDDEN
    - PRESENT_PASSIVE  → 角色静默陪伴 (不弹窗, 安静待在角落)
    - ABSENT           → 角色进入深度睡眠模式 (缩成一团睡觉)
    - UNKNOWN          → 退回到 Phase 2 的纯键鼠检测逻辑
    """
    
    IDLE_THRESHOLD_MS = 300000    # 5 分钟
    FACE_ABSENT_FRAMES = 30      # 连续 30 帧未检测到人脸 (约 2 秒)
    
    def __init__(self):
        self._face_absent_count = 0
    
    def determine_presence(
        self,
        idle_time_ms: int,
        gaze_data: "GazeData"
    ) -> PresenceState:
        """
        融合判定用户存在状态
        
        参数:
            idle_time_ms: 键鼠空闲时间 (毫秒)
            gaze_data: 摄像头视线数据
        
        返回:
            PresenceState 枚举值
        """
        if idle_time_ms < 60000:  # 1 分钟内有操作
            return PresenceState.PRESENT_ACTIVE
        
        if not gaze_data.face_detected:
            self._face_absent_count += 1
        else:
            self._face_absent_count = 0
        
        if idle_time_ms >= self.IDLE_THRESHOLD_MS:
            if self._face_absent_count >= self.FACE_ABSENT_FRAMES:
                return PresenceState.ABSENT
            elif gaze_data.face_detected:
                return PresenceState.PRESENT_PASSIVE
        
        return PresenceState.UNKNOWN
```

#### 行为响应定义

```python
class BehaviorResponse:
    """
    基于 PresenceState 的行为响应
    
    PRESENT_PASSIVE (用户在看视频/思考):
        ├── 角色悄悄滑入, 但不说话
        ├── 播放"安静陪伴"动画 (如坐在角落看书)
        ├── 不触发任何 TTS
        └── 30 秒后静默滑出
    
    ABSENT (用户真的离开了):
        ├── 等待额外 2 分钟确认
        ├── 角色进入"深度睡眠"模式:
        │   ├── 显示专用 sleep.gif ASCII 动画 (缩成一团)
        │   ├── 停留在屏幕角落
        │   └── 停止所有主动弹窗对话
        └── 用户回来时: 播放"打哈欠醒来"动画
    """
    pass
```

### 4.1.4 验收标准

| # | 验收项 | 测试方法 | 预期结果 |
|---|--------|---------|---------|
| 1 | 人脸检测 | 面对摄像头 | `face_detected = True` |
| 2 | 视线跟随 | 左右移动头部 | face_x 值在 [-1, 1] 范围变化 |
| 3 | 眼动渲染 | 观察 ASCII 角色 | 眼球字符跟随你的位置移动 |
| 4 | 摄像头释放 | 角色隐藏后 | 摄像头指示灯熄灭 |
| 5 | 存在检测 | 离开座位 5 分钟 | 判定为 ABSENT |
| 6 | 被动在场 | 看视频 5 分钟 | 判定为 PRESENT_PASSIVE |
| 7 | CPU 占用 | 任务管理器 | < 10% (15fps + MediaPipe) |

---

## 4.2 认知核心模块 (The "Brain" Module — LLM)

### 目标

让角色理解当前环境，并进行**开放式对话**。

### 4.2.1 架构设计：本地优先 vs 云端增强

考虑到隐私和响应速度，采用**混合架构**。

```
┌─────────────────────────────────────────────────┐
│               LLM 接口抽象层                      │
│                LLMProvider                       │
│       ┌───────────┴───────────┐                  │
│       ▼                       ▼                  │
│  ┌──────────┐           ┌──────────┐             │
│  │ 方案 A   │           │ 方案 B   │             │
│  │ 本地 LLM │           │ 云端 API │             │
│  │ (默认)   │           │ (可选)   │             │
│  ├──────────┤           ├──────────┤             │
│  │ Ollama   │           │ OpenAI   │             │
│  │ llama.cpp│           │ DeepSeek │             │
│  │ 7B 模型  │           │ Claude   │             │
│  ├──────────┤           ├──────────┤             │
│  │ ✅隐私安全│           │ ✅回复质量高│            │
│  │ ✅免费    │           │ ✅不占本地算力│           │
│  │ ❌占内存  │           │ ❌需联网    │            │
│  │ ❌速度慢  │           │ ❌有API费用 │            │
│  └──────────┘           └──────────┘             │
└─────────────────────────────────────────────────┘
```

#### LLM Provider 抽象接口

```python
# src/ai/llm_provider.py

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Optional


@dataclass
class LLMRequest:
    """LLM 请求数据模型"""
    system_prompt: str          # 系统级提示词（角色设定）
    user_message: str           # 用户侧消息（屏幕内容/触发语境）
    max_tokens: int = 100       # 最大生成 token 数
    temperature: float = 0.7    # 生成温度 (0.0=确定, 1.0=随机)


@dataclass
class LLMResponse:
    """LLM 响应数据模型"""
    text: str                   # 生成的文本
    tokens_used: int            # 消耗的 token 数
    latency_ms: int             # 延迟（毫秒）
    provider: str               # 提供者标识


class LLMProvider(ABC):
    """
    LLM 提供者抽象基类
    
    所有 LLM 实现（本地/云端）都必须继承此接口。
    """
    
    @abstractmethod
    async def generate(self, request: LLMRequest) -> LLMResponse:
        """生成回复"""
        ...
    
    @abstractmethod
    def is_available(self) -> bool:
        """检查服务是否可用"""
        ...
    
    @abstractmethod
    def get_resource_usage(self) -> dict:
        """获取资源占用信息 (内存/CPU)"""
        ...


class OllamaProvider(LLMProvider):
    """
    Ollama 本地 LLM 提供者
    
    前置条件:
    - 安装 Ollama: https://ollama.com
    - 拉取模型: ollama pull qwen2.5:7b
    
    API 端点: http://localhost:11434/api/generate
    """
    
    def __init__(self, model: str = "qwen2.5:7b", base_url: str = "http://localhost:11434"):
        self._model = model
        self._base_url = base_url
    
    async def generate(self, request: LLMRequest) -> LLMResponse:
        """
        调用 Ollama API
        
        HTTP POST http://localhost:11434/api/generate
        Body:
        {
            "model": "qwen2.5:7b",
            "prompt": "<system_prompt>\n\n<user_message>",
            "stream": false,
            "options": {
                "num_predict": 100,
                "temperature": 0.7
            }
        }
        """
        import httpx
        import time
        
        start = time.time()
        
        async with httpx.AsyncClient(timeout=30.0) as client:
            response = await client.post(
                f"{self._base_url}/api/generate",
                json={
                    "model": self._model,
                    "prompt": f"{request.system_prompt}\n\n{request.user_message}",
                    "stream": False,
                    "options": {
                        "num_predict": request.max_tokens,
                        "temperature": request.temperature,
                    }
                }
            )
            result = response.json()
        
        latency = int((time.time() - start) * 1000)
        
        return LLMResponse(
            text=result.get("response", ""),
            tokens_used=result.get("eval_count", 0),
            latency_ms=latency,
            provider="ollama"
        )
    
    def is_available(self) -> bool:
        """检查 Ollama 是否在运行"""
        try:
            import httpx
            r = httpx.get(f"{self._base_url}/api/tags", timeout=2.0)
            return r.status_code == 200
        except Exception:
            return False
    
    def get_resource_usage(self) -> dict:
        return {"memory_gb": 4.0, "provider": "ollama"}


class OpenAIProvider(LLMProvider):
    """
    OpenAI 兼容 API 提供者
    
    支持: OpenAI / DeepSeek / 任何 OpenAI 兼容 API
    
    配置:
        api_key: API 密钥（从环境变量 OPENAI_API_KEY 读取）
        base_url: API 端点（可自定义为 DeepSeek 等）
    """
    
    def __init__(
        self,
        model: str = "gpt-4o-mini",
        api_key: Optional[str] = None,
        base_url: str = "https://api.openai.com/v1"
    ):
        self._model = model
        self._api_key = api_key or self._load_api_key()
        self._base_url = base_url
    
    def _load_api_key(self) -> str:
        """从环境变量加载 API Key"""
        import os
        key = os.environ.get("OPENAI_API_KEY", "")
        if not key:
            print("[LLM] 警告: OPENAI_API_KEY 未设置")
        return key
    
    async def generate(self, request: LLMRequest) -> LLMResponse:
        """
        调用 OpenAI 兼容 API
        
        使用 openai Python SDK:
            from openai import AsyncOpenAI
            client = AsyncOpenAI(api_key=..., base_url=...)
            response = await client.chat.completions.create(...)
        """
        from openai import AsyncOpenAI
        import time
        
        start = time.time()
        client = AsyncOpenAI(api_key=self._api_key, base_url=self._base_url)
        
        response = await client.chat.completions.create(
            model=self._model,
            messages=[
                {"role": "system", "content": request.system_prompt},
                {"role": "user", "content": request.user_message},
            ],
            max_tokens=request.max_tokens,
            temperature=request.temperature,
        )
        
        latency = int((time.time() - start) * 1000)
        text = response.choices[0].message.content or ""
        
        return LLMResponse(
            text=text,
            tokens_used=response.usage.total_tokens if response.usage else 0,
            latency_ms=latency,
            provider="openai"
        )
    
    def is_available(self) -> bool:
        return bool(self._api_key)
    
    def get_resource_usage(self) -> dict:
        return {"memory_gb": 0, "provider": "openai_api"}
```

### 4.2.2 核心功能：屏幕内容吐槽 (Screen Context Commentary)

当用户主动点击角色时，她能根据你当前屏幕上的内容进行吐槽。

#### 完整工作流

```python
# src/ai/screen_commentator.py

from pathlib import Path
import re
from datetime import datetime


class ScreenCommentator:
    """
    屏幕内容吐槽器
    
    完整工作流:
    1. 触发: 用户右键点击 ASCII 小人 → 选择"你在看什么？"
    2. 截屏: 使用 mss 截取当前活动窗口
    3. OCR:  使用 PaddleOCR 或 Windows OCR 提取文字
    4. 隐私: 模糊化邮箱/手机号等敏感信息
    5. Prompt: 构建角色化的 LLM 提示词
    6. LLM:   生成吐槽文本
    7. TTS:   语音播放
    """
    
    def __init__(self, llm_provider: "LLMProvider", audio_manager: "AudioManager"):
        self._llm = llm_provider
        self._audio = audio_manager
    
    async def comment_on_screen(self, mood_value: float = 0.5) -> str:
        """
        对当前屏幕内容进行吐槽
        
        参数:
            mood_value: 当前心情值 [0.0 生气, 1.0 开心]
        
        返回:
            LLM 生成的吐槽文本
        """
        # Step 1: 截屏
        screenshot = self._capture_active_window()
        
        # Step 2: OCR
        text = self._extract_text(screenshot)
        
        # Step 3: 隐私过滤
        text = self._filter_privacy(text)
        
        # Step 4: 如果文字太少则放弃
        if len(text.strip()) < 10:
            return "哼，屏幕上什么都没有，好无聊。"
        
        # Step 5: 构建 Prompt
        system_prompt = self._build_system_prompt(mood_value)
        user_prompt = f"用户当前屏幕上显示以下文字：\n{text[:500]}"  # 限制 500 字符
        
        # Step 6: 调用 LLM
        request = LLMRequest(
            system_prompt=system_prompt,
            user_message=user_prompt,
            max_tokens=80,
            temperature=0.8
        )
        response = await self._llm.generate(request)
        
        # Step 7: TTS 播放
        self._audio.speak(response.text, priority=AudioPriority.HIGH)
        
        return response.text
    
    def _capture_active_window(self) -> "np.ndarray":
        """
        截取当前活动窗口
        
        使用 mss 库而不是 PIL.ImageGrab:
        - mss 速度更快 (约 10-30ms)
        - 支持多显示器
        - 不会闪烁
        """
        import mss
        import numpy as np
        
        with mss.mss() as sct:
            # 截取主显示器
            monitor = sct.monitors[1]  # monitors[0] 是所有显示器的联合
            screenshot = sct.grab(monitor)
            return np.array(screenshot)
    
    def _extract_text(self, image: "np.ndarray") -> str:
        """
        OCR 文字识别
        
        方案 A — PaddleOCR (跨平台):
            from paddleocr import PaddleOCR
            ocr = PaddleOCR(use_angle_cls=True, lang='ch')
            result = ocr.ocr(image, cls=True)
        
        方案 B — Windows 原生 OCR (轻量):
            # 使用 Windows.Media.Ocr API (通过 winocr 库)
            # pip install winocr
            import winocr
            result = await winocr.recognize_pil(image, 'zh-Hans-CN')
        
        选择建议:
        - 如果已经安装了 PaddleOCR → 用方案 A
        - 如果想最小化依赖 → 用方案 B
        """
        # 这里使用方案 B 作为默认实现示例
        ...
    
    def _filter_privacy(self, text: str) -> str:
        """
        过滤隐私信息
        
        替换规则:
        - 邮箱: user@example.com → u***@e***
        - 手机号: 13812345678 → 138****5678
        - IP 地址: 192.168.1.1 → ***.***.***.***
        - 银行卡号: 连续 16 位数字 → ****
        """
        # 邮箱
        text = re.sub(
            r'[\w.-]+@[\w.-]+\.\w+',
            '[邮箱已隐藏]',
            text
        )
        # 手机号 (中国)
        text = re.sub(
            r'1[3-9]\d{9}',
            '[手机号已隐藏]',
            text
        )
        # IP 地址
        text = re.sub(
            r'\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}',
            '[IP已隐藏]',
            text
        )
        return text
    
    def _build_system_prompt(self, mood_value: float) -> str:
        """
        构建角色化的系统提示词
        
        心情值影响 Prompt:
        - mood < 0.3: 冷淡/生气模式
        - mood 0.3-0.7: 正常/吐槽模式
        - mood > 0.7: 开心/粘人模式
        
        参数:
            mood_value: [0.0, 1.0]
        
        返回:
            系统级提示词字符串
        """
        if mood_value < 0.3:
            personality = "你现在很生气，不太想理用户。回复简短冷淡。"
        elif mood_value > 0.7:
            personality = "你现在心情很好，有点粘人。回复活泼可爱，会用颜文字。"
        else:
            personality = "你是一个傲娇毒舌的风格。回复犀利但关心对方。"
        
        return (
            f"你是一个赛博朋克风格的桌面伴侣 AI 少女。{personality}\n"
            "规则：\n"
            "1. 根据屏幕文字内容，用简短吐槽的语气评论用户正在做的事\n"
            "2. 回复不超过 30 个字\n"
            "3. 如果是代码报错，可以给出简短建议\n"
            "4. 如果是聊天记录，装作没看到\n"
            "5. 使用中文回复"
        )
```

### 4.2.3 动态角色设定 (Dynamic Persona) — 心情系统

```python
# src/core/mood_system.py

class MoodSystem:
    """
    心情系统
    
    心情值: 0.0 (极度生气) ~ 1.0 (极度开心)
    初始值: 0.5 (中性)
    
    影响因素:
    - 用户无视角色弹出 (dismiss)      → 心情 -0.05
    - 用户与角色互动 (interact)        → 心情 +0.1
    - 用户点击"你在看什么" (engage)     → 心情 +0.15
    - 自然衰减 (每小时)                → 趋向 0.5
    
    心情对行为的影响:
    - mood < 0.3: 角色说话冷淡，出现频率降低
    - mood 0.3-0.7: 正常行为
    - mood > 0.7: 角色更活泼，出现频率增加，使用颜文字
    """
    
    def __init__(self, initial_mood: float = 0.5):
        self._mood = initial_mood
        self._mood = max(0.0, min(1.0, self._mood))
    
    @property
    def mood(self) -> float:
        return self._mood
    
    @property
    def mood_label(self) -> str:
        """获取心情描述"""
        if self._mood < 0.2:
            return "愤怒"
        elif self._mood < 0.4:
            return "不满"
        elif self._mood < 0.6:
            return "平静"
        elif self._mood < 0.8:
            return "开心"
        else:
            return "兴奋"
    
    def on_dismissed(self) -> None:
        """用户无视了角色弹出"""
        self._mood = max(0.0, self._mood - 0.05)
    
    def on_interacted(self) -> None:
        """用户与角色互动"""
        self._mood = min(1.0, self._mood + 0.1)
    
    def on_engaged(self) -> None:
        """用户主动与角色深度互动"""
        self._mood = min(1.0, self._mood + 0.15)
    
    def natural_decay(self) -> None:
        """自然衰减（每小时调用一次），趋向 0.5"""
        if self._mood > 0.5:
            self._mood -= 0.02
        elif self._mood < 0.5:
            self._mood += 0.02
```

---

## 4.3 系统集成：多进程架构 (Integration)

### ⚠️ 核心原则：AI 推理绝对不能阻塞 GUI 线程

引入 CV 和 LLM 后，系统复杂度指数级上升。必须使用多进程架构。

### 4.3.1 进程架构设计

```
┌───────────────────────────────────────────────────────────────────┐
│                        操作系统                                    │
│                                                                    │
│  ┌─────────────────────┐                                          │
│  │   主进程 (GUI)       │ ◄──── 用户可见的进程                     │
│  │   - PySide6 事件循环 │                                          │
│  │   - EntityWindow     │                                          │
│  │   - 动画渲染          │                                         │
│  │   - 用户输入接收      │                                          │
│  │   内存: ~30MB        │                                          │
│  └───────┬─────────────┘                                          │
│          │ IPC (Queue/Pipe)                                        │
│          │                                                         │
│  ┌───────┴────────────┐    ┌───────────────────────────┐          │
│  │ 感知进程 (Daemon 1) │    │ 认知进程 (Daemon 2)       │          │
│  │ - MediaPipe CV      │    │ - LLM 模型加载            │          │
│  │ - 摄像头 30FPS      │    │ - 推理计算                │          │
│  │ - 发送眼球坐标       │    │ - 平时休眠               │          │
│  │ 内存: ~200MB        │    │ 内存: 4-8GB (模型)       │          │
│  └────────────────────┘    └───────────────────────────┘          │
│                                                                    │
│  ┌─────────────────────────────────────────────┐                  │
│  │ 通信: multiprocessing.Queue 或 ZeroMQ        │                  │
│  │                                               │                 │
│  │ 消息格式:                                      │                │
│  │ {"type": "gaze_update", "data": {...}}        │                 │
│  │ {"type": "llm_request", "data": {...}}        │                 │
│  │ {"type": "llm_response", "data": {...}}       │                 │
│  └─────────────────────────────────────────────┘                  │
└───────────────────────────────────────────────────────────────────┘
```

### 4.3.2 资源调度策略

```python
class ResourceScheduler:
    """
    资源调度器
    
    职责:
    - 检测前台应用类型
    - 动态调整 CV/LLM 进程状态
    - 释放 CPU/GPU 资源给高优先级应用
    
    策略:
    ┌─────────────────────┬──────────┬──────────┬──────────┐
    │ 场景                │ GUI 进程 │ CV 进程  │ LLM 进程 │
    ├─────────────────────┼──────────┼──────────┼──────────┤
    │ 正常桌面使用        │ ✅ 运行  │ ✅ 运行  │ 💤 休眠  │
    │ 全屏游戏运行        │ ✅ 运行  │ ❌ 挂起  │ ❌ 挂起  │
    │ 用户主动对话        │ ✅ 运行  │ ✅ 运行  │ ✅ 唤醒  │
    │ 对话结束 5 分钟后   │ ✅ 运行  │ ✅ 运行  │ ❌ 终止  │
    └─────────────────────┴──────────┴──────────┴──────────┘
    
    内存释放策略:
    - LLM 进程设计为"按需启动，用完即焚"
    - 5 分钟无对话 → terminate() LLM 进程
    - 释放 4-8GB 内存
    - 只保留 30MB 的 GUI 主进程
    """
    pass
```

### 4.3.3 验收标准

| # | 验收项 | 测试方法 | 预期结果 |
|---|--------|---------|---------|
| 1 | 多进程隔离 | LLM 推理时操作 GUI | GUI 无卡顿 |
| 2 | IPC 通信 | 发送视线数据 | 主进程正确接收并更新眼球 |
| 3 | 资源释放 | 终止 LLM 进程 | 内存立即释放 |
| 4 | 全屏检测 | 启动全屏游戏 | CV 和 LLM 自动挂起 |
| 5 | 进程恢复 | 退出全屏 | 进程自动恢复 |
| 6 | 屏幕吐槽 | 点击角色触发 | 3-10 秒内返回吐槽文本 |
| 7 | 心情系统 | 连续无视角色 5 次 | 角色语气变冷 |
| 8 | 隐私过滤 | 屏幕显示邮箱/手机号 | LLM 收到的文本已脱敏 |