# 📝 Project Cyber-Companion 开发文档 (Phase 2)

## —— 核心实现规范：神经与肌肉

---

## 2.0 阶段概述

本阶段深入实现两个最硬核的技术子系统：

| 子系统 | 比喻 | 核心问题 |
|--------|------|---------|
| **感知层** | 神经系统 | 如何精准地知道用户"消失"了多久？ |
| **动画层** | 肌肉系统 | 如何让角色"滑"出来而不是"跳"出来？ |

### 前置依赖

- Phase 1 中的 `IdleMonitor` 类已通过所有验收标准
- Phase 1 中的 `EntityWindow` 类已通过所有验收标准

---

## 2.1 感知层详细实现 (The Nervous System)

### 目标

构建一个**零延迟、低资源**的全局输入监听器。

### 2.1.1 核心 Windows API 规范

我们需要绕过 Python 的高层封装，直接与 Windows 内核对话。

**技术锚点**: `User32.dll` 中的 `GetLastInputInfo` 函数

**官方文档**: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getlastinputinfo

#### C 结构体定义 → Python 映射

```c
// Windows C API 原始定义
typedef struct tagLASTINPUTINFO {
    UINT  cbSize;   // 结构体大小，必须初始化为 sizeof(LASTINPUTINFO)
    DWORD dwTime;   // 最后输入事件的系统 Tick Count（毫秒）
} LASTINPUTINFO, *PLASTINPUTINFO;
```

```python
# Python ctypes 映射实现
import ctypes
import ctypes.wintypes

class LASTINPUTINFO(ctypes.Structure):
    """
    与 Windows C 结构体 tagLASTINPUTINFO 一一对应。
    
    字段说明:
    - cbSize: UINT, 结构体大小（字节）。
              必须在调用 GetLastInputInfo() 前设置为 ctypes.sizeof(LASTINPUTINFO)，
              否则 Windows 会拒绝请求并返回 False。
    - dwTime: DWORD, 系统最后一次接收到输入事件（鼠标移动/点击/键盘按键）时的
              Tick Count。Tick Count 是系统自启动以来经过的毫秒数。
    
    注意事项:
    - DWORD 是 32 位无符号整数 (0 ~ 4,294,967,295)
    - Tick Count 约在系统连续运行 49.7 天后溢出归零
    - 溢出后 dwTime 可能大于 GetTickCount()，需要特殊处理
    """
    _fields_ = [
        ("cbSize", ctypes.wintypes.UINT),
        ("dwTime", ctypes.wintypes.DWORD),
    ]
```

#### 辅助 API 函数签名

```python
# 加载 DLL
user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32

# GetLastInputInfo: 获取最后输入信息
# 参数: PLASTINPUTINFO - 指向 LASTINPUTINFO 结构体的指针
# 返回: BOOL - 成功返回 True (非零)
user32.GetLastInputInfo.argtypes = [ctypes.POINTER(LASTINPUTINFO)]
user32.GetLastInputInfo.restype = ctypes.wintypes.BOOL

# GetTickCount: 获取系统自启动以来的毫秒数 (32位, 会溢出)
# 返回: DWORD
kernel32.GetTickCount.restype = ctypes.wintypes.DWORD

# GetTickCount64: 获取系统自启动以来的毫秒数 (64位, 不会溢出)
# 推荐在生产环境使用此函数替代 GetTickCount
# 返回: ULONGLONG
kernel32.GetTickCount64.restype = ctypes.c_uint64
```

### 2.1.2 完整轮询逻辑 (Logic Flow)

> ⚠️ **感知层必须运行在独立的 `QThread` 或 `Daemon Thread` 中**，否则会阻塞 GUI 主线程导致界面卡顿。

#### 流程图

```
┌─────────────────────────────────────────────────┐
│              IdleMonitor 轮询循环                 │
│                                                  │
│  ┌──────────────┐                                │
│  │    初始化     │                                │
│  │ 加载 User32  │                                │
│  └──────┬───────┘                                │
│         ▼                                        │
│  ┌──────────────┐                                │
│  │  Sleep 100ms │ ◄──────────────────────────┐   │
│  └──────┬───────┘                            │   │
│         ▼                                    │   │
│  ┌──────────────────────┐                    │   │
│  │ T_now = GetTickCount │                    │   │
│  │ T_last = GetLast...  │                    │   │
│  │ ΔT = T_now - T_last  │                    │   │
│  └──────┬───────────────┘                    │   │
│         ▼                                    │   │
│  ┌──────────────────────┐                    │   │
│  │   状态机判定 (FSM)    │                    │   │
│  │                      │                    │   │
│  │ ΔT > Threshold?      │                    │   │
│  │  ├─ Yes → emit idle  │                    │   │
│  │  └─ No               │                    │   │
│  │ ΔT < 1000ms?         │                    │   │
│  │  ├─ Yes → emit active│                    │   │
│  │  └─ No               │                    │   │
│  └──────┬───────────────┘                    │   │
│         └────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
```

#### 详细状态机转换表

| 当前状态 | 条件 | 动作 | 新状态 |
|---------|------|------|--------|
| STANDBY | `ΔT >= threshold * 0.8` | 无信号（可用于预加载资源） | PRE_IDLE |
| STANDBY | `ΔT >= threshold` | `emit user_idle_confirmed` | IDLE_TRIGGERED |
| PRE_IDLE | `ΔT >= threshold` | `emit user_idle_confirmed` | IDLE_TRIGGERED |
| PRE_IDLE | `ΔT < 1000ms` | 无信号 | STANDBY |
| IDLE_TRIGGERED | `ΔT < 1000ms` | `emit user_active_detected` | ACTIVE |
| ACTIVE | 外部调用 `reset_to_standby()` | 无信号 | STANDBY |

#### 关键代码：Tick 溢出处理

```python
def _get_idle_time_ms(self) -> int:
    """
    获取用户空闲时间（毫秒）
    
    ⚠️ 溢出处理说明:
    
    GetTickCount() 返回 DWORD (32位无符号整数)，
    约在系统连续运行 49.7 天后溢出归零。
    
    风险场景: 
    - T_last = 4,294,967,200 (溢出前)
    - T_now  = 100 (溢出后)
    - T_now - T_last = -4,294,967,100 (错误的负值!)
    
    解决方案:
    - 方案 A: 使用 GetTickCount64() (推荐，但需 Vista+)
    - 方案 B: 对负值取模 0xFFFFFFFF
    - 方案 C: 安全降级，返回 0（假设用户活跃）
    """
    lii = LASTINPUTINFO()
    lii.cbSize = ctypes.sizeof(LASTINPUTINFO)
    
    if not self._user32.GetLastInputInfo(ctypes.byref(lii)):
        # API 调用失败 — 安全降级，视为用户活跃
        return 0
    
    # 推荐使用 GetTickCount64 避免溢出
    try:
        current_tick = self._kernel32.GetTickCount64()
        idle_time = current_tick - lii.dwTime
    except AttributeError:
        # 降级到 32 位版本
        current_tick = self._kernel32.GetTickCount()
        idle_time = current_tick - lii.dwTime
        if idle_time < 0:
            idle_time = (idle_time + 0x100000000) & 0xFFFFFFFF
    
    return max(0, idle_time)
```

### 2.1.3 验收标准

| # | 验收项 | 测试方法 | 预期结果 |
|---|--------|---------|---------|
| 1 | API 调用正确性 | 运行测试脚本 5 分钟 | 空闲时间单调递增，操作后归零 |
| 2 | 状态转换正确性 | 模拟各种输入模式 | 状态按照转换表正确流转 |
| 3 | 线程安全性 | 在 GUI 应用中集成 | GUI 无任何卡顿 |
| 4 | 资源占用 | 任务管理器观察 | CPU < 1%, 内存 < 5MB |
| 5 | 长时间稳定性 | 后台运行 8 小时 | 无崩溃、无内存泄漏 |

---

## 2.2 视觉层详细实现 (The Visual Cortex)

### 目标

实现"透明背景"与"高保真字符渲染"。

### 2.2.1 动态着色算法 (Dynamic Coloring Algorithm)

单纯的黑白 ASCII 没有灵魂。我们需要通过 HTML 实现**全彩渲染**。

#### 输入处理流程

```python
def _preprocess_image(self, image: Image.Image) -> Image.Image:
    """
    图片预处理
    
    步骤:
    1. 调整尺寸:
       - 目标宽度: self._width (字符数)
       - 目标高度: width × 原图宽高比 × 0.55
       - 0.55 是等宽字体高宽比修正系数
       - 含义: 一个字符的显示高度约为宽度的 55%
       - 如果不乘以 0.55, ASCII 画会被纵向拉伸约 1.8 倍
    
    2. 转换为 RGBA 模式（保留 PNG 的 alpha 通道）
    """
    # 计算目标高度
    aspect_ratio = image.height / image.width
    target_height = int(self._width * aspect_ratio * self.ASPECT_RATIO_CORRECTION)
    
    # 确保至少 1 行
    target_height = max(1, target_height)
    
    # 使用 LANCZOS 重采样（保留细节）
    image = image.resize(
        (self._width, target_height),
        Image.Resampling.LANCZOS
    )
    
    # 转为 RGBA 以支持透明通道
    return image.convert("RGBA")
```

#### 透明度遮罩 (Alpha Masking)

对每个像素进行透明判定：

```python
def _is_transparent(self, r: int, g: int, b: int, a: int) -> bool:
    """
    判定像素是否应被视为透明（背景色）
    
    透明判定规则（满足任一即透明）：
    1. PNG alpha 通道 < 30 (原图本身标记为透明)
    2. R + G + B < 30 (接近纯黑，可能是需要扣除的背景色)
    3. 像素在指定的绿幕色范围内（可扩展）
    
    参数:
        r, g, b: RGB 值 (0-255)
        a: Alpha 通道值 (0=完全透明, 255=完全不透明)
    
    返回: 
        True → 输出 "&nbsp;" (HTML 空格), 小人轮廓"镂空"
        False → 正常输出彩色 ASCII 字符
    """
    if a < 30:
        return True  # PNG 原生透明
    if r + g + b < self.BG_THRESHOLD:
        return True  # 接近纯黑
    return False
```

#### 灰度到字符映射

```python
def _gray_to_char(self, gray: float) -> str:
    """
    灰度值映射到 ASCII 字符
    
    映射表 (从暗到亮):
    - "@" : 灰度 0-28    (最暗/最密)
    - "%" : 灰度 29-56
    - "#" : 灰度 57-85
    - "*" : 灰度 86-113
    - "+" : 灰度 114-141
    - "=" : 灰度 142-170
    - "-" : 灰度 171-198
    - ":" : 灰度 199-226
    - "." : 灰度 227-254
    - " " : 灰度 255      (最亮/最疏)
    
    参数:
        gray: 灰度值 (0.0 - 255.0)
              计算公式: 0.299*R + 0.587*G + 0.114*B
              (ITU-R BT.601 标准亮度权重)
    
    返回:
        单个 ASCII 字符
    """
    index = int(gray / 255.0 * (len(self.CHARSET) - 1))
    index = max(0, min(index, len(self.CHARSET) - 1))
    return self.CHARSET[index]
```

#### 完整渲染输出格式

```python
def render_image(self, image_path: Path) -> str:
    """
    最终输出的 HTML 格式示例:
    
    <pre style="
        font-family: Consolas, 'Courier New', monospace;
        font-size: 8px;
        line-height: 1.0;
        letter-spacing: 0px;
        margin: 0;
        padding: 0;
        white-space: pre;
    ">
    <span style="color:rgb(255,200,180);">@</span><span style="color:rgb(230,180,160);">%</span>&nbsp;&nbsp;
    <span style="color:rgb(100,50,20);">#</span><span style="color:rgb(80,40,15);">*</span>&nbsp;&nbsp;
    </pre>
    
    关键 CSS 要求:
    - font-family: 必须是等宽字体，否则字符无法对齐
    - font-size: 8px 适合 80 列宽度在 1920px 屏幕
    - line-height: 1.0 消除行间距
    - letter-spacing: 0px 消除字间距
    - white-space: pre 保留空格和换行
    """
    image = Image.open(image_path)
    image = self._preprocess_image(image)
    pixels = np.array(image)
    
    lines = []
    for row in pixels:
        line_chars = []
        for pixel in row:
            r, g, b, a = pixel[0], pixel[1], pixel[2], pixel[3]
            
            if self._is_transparent(r, g, b, a):
                line_chars.append("&nbsp;")
            else:
                gray = 0.299 * r + 0.587 * g + 0.114 * b
                char = self._gray_to_char(gray)
                line_chars.append(
                    f'<span style="color:rgb({r},{g},{b});">{char}</span>'
                )
        
        lines.append("".join(line_chars))
    
    html_body = "<br>".join(lines)
    
    return (
        '<pre style="'
        'font-family: Consolas, \'Courier New\', monospace; '
        'font-size: 8px; '
        'line-height: 1.0; '
        'letter-spacing: 0px; '
        'margin: 0; padding: 0; '
        'white-space: pre;'
        f'">{html_body}</pre>'
    )
```

### 2.2.2 渲染容器 (The Container)

**关键原则**: 不要直接在 `paintEvent` 里逐字符绘制（太慢）。使用 QLabel + RichText (HTML)。

```python
# EntityWindow 中的 QLabel 配置
self._label = QLabel()
self._label.setTextFormat(Qt.TextFormat.RichText)     # 启用 HTML 渲染
self._label.setFont(QFont("Consolas", 8))              # 等宽字体
self._label.setStyleSheet("""
    QLabel {
        background: transparent;
        padding: 0px;
        margin: 0px;
    }
""")
```

### 2.2.3 验收标准

| # | 验收项 | 测试方法 | 预期结果 |
|---|--------|---------|---------|
| 1 | 彩色渲染 | 渲染一张彩色 PNG | 颜色与原图匹配 |
| 2 | 透明扣除 | 使用黑色背景图片 | 黑色部分可透过看到桌面 |
| 3 | PNG 透明通道 | 使用带 alpha 的 PNG | 透明区域正确镂空 |
| 4 | 比例正确 | 与原图对比 | ASCII 画不变形（无拉伸） |
| 5 | 渲染性能 | 计时 60 列宽渲染 | 单帧 < 50ms |

---

## 2.3 肌肉系统：动画与运动 (Motor Control)

### 目标

让角色具备物理惯性，表现出**"探头探脑"的生命感**。

### 2.3.1 坐标系设计

我们不把窗口放在屏幕中间，而是**藏在屏幕边缘之外**。

假设屏幕分辨率为 `1920 × 1080`，角色窗口宽 `300px`：

```
屏幕边界
│                                              │
│  ┌──────────────────────────────────────┐    │ 
│  │               屏幕可视区域            │    │
│  │                                      │    │
│  │                         ┌──────┐     │    │
│  │                         │ 角色 │     │    │ ← 完全态 X=1570
│  │                         └──────┘     │    │
│  │                                      │    │
│  └──────────────────────────────────────┘    │
│                                          ┌──┤│
│                                          │角││ ← 探头态 X=1820
│                                          └──┤│
│                                              │┌──────┐
│                                              ││ 角色 │ ← 隐藏态 X=1920
│                                              │└──────┘
```

#### 状态坐标定义

```python
class EntityPositions:
    """
    屏幕坐标常量
    
    使用方法: 在 EntityWindow 初始化时根据实际屏幕尺寸动态计算
    """
    
    @staticmethod
    def calculate(screen_width: int, window_width: int = 300, margin: int = 50):
        """
        动态计算各状态的 X 坐标
        
        参数:
            screen_width: 屏幕宽度 (像素)
            window_width: 角色窗口宽度 (像素)
            margin: 完全态时距离屏幕边缘的留白 (像素)
        
        返回: dict
            {
                "hidden": 1920,    # 完全在屏幕外
                "peeking": 1820,   # 只露出 100px (约 1/3)
                "full": 1570,      # 完全进入，留 50px 边距
            }
        """
        return {
            "hidden": screen_width,                              # 完全隐藏
            "peeking": screen_width - 100,                       # 探出头
            "full": screen_width - window_width - margin,        # 完全进入
        }
```

### 2.3.2 动画曲线 (Easing Curves) — 深入解析

> ⚠️ **不要使用线性移动（Linear）**，那像个机器人。

#### 推荐曲线对比

| 曲线 | Qt 枚举 | 视觉效果 | 使用场景 |
|------|---------|---------|---------|
| **OutBack** | `QEasingCurve.OutBack` | 冲过头后弹回 | 探头、登场 |
| **OutBounce** | `QEasingCurve.OutBounce` | 到达后反弹 2-3 次 | 开心跳出 |
| **InExpo** | `QEasingCurve.InExpo` | 先慢后极快 | 逃跑 |
| **OutElastic** | `QEasingCurve.OutElastic` | 弹簧效果 | 可选：从被拍飞后回弹 |
| ~~Linear~~ | ~~`QEasingCurve.Linear`~~ | ~~匀速~~ | ~~禁止使用~~ |

#### 曲线效果拟人化解释

- **OutBack（探头）**: 像一个人急匆匆跑出来刹车没刹住，稍微冲过头再退回来 → 笨拙可爱
- **OutBounce（登场）**: 像皮球落地弹跳 → 活泼开心
- **InExpo（逃跑）**: 开始慢慢后退，然后突然加速消失 → 被发现后慌张溜走

#### 动画代码实现

```python
def _create_slide_animation(
    self,
    start_x: int,
    end_x: int,
    y: int,
    duration_ms: int,
    curve: QEasingCurve.Type
) -> QPropertyAnimation:
    """
    创建一个水平滑动动画
    
    参数:
        start_x: 起始 X 坐标
        end_x: 终点 X 坐标
        y: Y 坐标 (垂直位置不变)
        duration_ms: 持续时间 (毫秒)
        curve: 缓动曲线类型
    
    返回:
        QPropertyAnimation 实例
    
    使用示例:
        anim = self._create_slide_animation(
            start_x=1920, end_x=1820,
            y=400, duration_ms=1500,
            curve=QEasingCurve.Type.OutBack
        )
        anim.start()
    """
    anim = QPropertyAnimation(self, b"pos")
    anim.setDuration(duration_ms)
    anim.setStartValue(QPoint(start_x, y))
    anim.setEndValue(QPoint(end_x, y))
    anim.setEasingCurve(curve)
    return anim
```

### 2.3.3 动作编排 (Choreography)

这是从"程序"到"伴侣"的**关键跃迁**。角色的出场不是一个简单的移动，而是一段四幕戏剧。

#### 完整动画序列时间轴

```
时间 (秒)    0.0          1.5    3.5          4.3              4.6
            ├────────────┤├────┤├────────────┤├────────────────┤
Stage 1      探头 (Peek)    等待   登场 (Enter)   [用户活跃时]
            X: 1920→1820  停顿   X: 1820→1570   逃跑 X: →1920
            曲线:OutBack  2秒   曲线:OutBounce  曲线:InExpo
            速度:慢             速度:中         速度:极快
```

#### Stage 详细定义

```python
def summon(self, edge: str, y_position: int, script) -> None:
    """
    完整的召唤动画序列
    
    Stage 1: 探头 (Peek)
    ├── 时长: 1500ms
    ├── 动画: X 从 screen_width 移动到 screen_width-100
    ├── 曲线: OutBack (冲过头后弹回)
    └── 语义: 她小心翼翼地探出头，确认你是不是不在
    
    Stage 2: 确认 (Confirm)  
    ├── 时长: 2000ms 停顿
    ├── 动画: 无
    ├── 音频: 播放 "Master, are you there?" 或时段台词
    └── 语义: 观察环境，确认安全
    
    Stage 3: 登场 (Enter)
    ├── 时长: 800ms
    ├── 动画: X 从 screen_width-100 移动到 screen_width-350
    ├── 曲线: OutBounce (弹跳着进入)
    └── 语义: 确认安全，开心地跳出来
    """
    positions = EntityPositions.calculate(screen_width)
    
    # 构建动画序列
    sequence = QSequentialAnimationGroup(self)
    
    # Stage 1: 探头
    peek_anim = self._create_slide_animation(
        start_x=positions["hidden"],
        end_x=positions["peeking"],
        y=y_position,
        duration_ms=1500,
        curve=QEasingCurve.Type.OutBack
    )
    sequence.addAnimation(peek_anim)
    
    # Stage 2: 停顿 2 秒
    sequence.addPause(2000)
    
    # Stage 3: 登场
    enter_anim = self._create_slide_animation(
        start_x=positions["peeking"],
        end_x=positions["full"],
        y=y_position,
        duration_ms=800,
        curve=QEasingCurve.Type.OutBounce
    )
    sequence.addAnimation(enter_anim)
    
    # 初始化并启动
    self.move(positions["hidden"], y_position)
    self.show()
    sequence.start()


def flee(self) -> None:
    """
    Stage 4: 惊吓/逃跑 (Panic)
    ├── 时长: 300ms
    ├── 动画: X 从当前位置移动到 screen_width
    ├── 曲线: InExpo (先慢后极速)
    ├── 音频: 播放 "哇！被发现了！" (高优先级打断当前播放)
    └── 语义: 被发现了！赶紧溜！
    
    动画完成后:
    - 隐藏窗口 (hide())
    - 通知 IdleMonitor 重置为 STANDBY
    """
    flee_anim = self._create_slide_animation(
        start_x=self.x(),
        end_x=screen_width,
        y=self.y(),
        duration_ms=300,
        curve=QEasingCurve.Type.InExpo
    )
    flee_anim.finished.connect(self.hide)
    flee_anim.finished.connect(self._on_flee_complete)
    flee_anim.start()
```

### 2.3.4 验收标准

| # | 验收项 | 测试方法 | 预期结果 |
|---|--------|---------|---------|
| 1 | 探头动画 | 触发空闲 | 角色从右侧平滑探出 100px |
| 2 | 弹性效果 | 观察探头瞬间 | 有轻微"冲过头再回弹"的感觉 |
| 3 | 分阶段动画 | 全程观察 | 探头 → 停顿 → 登场，节奏清晰 |
| 4 | 逃跑响应 | 动画中移动鼠标 | 立即中断当前动画，极速缩回 |
| 5 | 逃跑速度 | 计时 | 0.3 秒内完全消失 |
| 6 | 动画帧率 | 肉眼观察 | 无明显卡顿 (≥ 60fps) |

---

## 2.4 数据与资源结构 (Data Schema)

> ⚠️ **不要把台词写死在代码里**。使用外部 JSON/YAML 文件，方便后续扩展和角色换皮。

### 2.4.1 配置文件规范

#### `config.json` — 全局配置

```json
{
    "$schema": "config_schema.json",
    "version": "1.0.0",
    "trigger": {
        "idle_threshold_seconds": 180,
        "jitter_range_seconds": [-30, 60],
        "auto_dismiss_seconds": 30
    },
    "appearance": {
        "theme": "rem_maid",
        "position": "right",
        "ascii_width": 60,
        "font_size_px": 8
    },
    "audio": {
        "tts_voice": "zh-CN-XiaoxiaoNeural",
        "tts_rate": "+0%",
        "volume": 0.8,
        "cache_enabled": true
    },
    "behavior": {
        "full_screen_pause": true,
        "auto_start_on_login": false,
        "debug_mode": false
    }
}
```

#### `scripts.json` — 台词库

```json
{
    "$schema": "scripts_schema.json",
    "version": "1.0.0",
    "idle_events": [
        {
            "id": "late_night_01",
            "time_range": "22:00-06:00",
            "text": "这么晚了还不睡，头发会掉光的哦。",
            "audio_cache": "assets/voice/late_night_01.mp3",
            "anim_speed": "slow",
            "probability": 0.8,
            "cooldown_minutes": 30,
            "tags": ["health", "night"]
        },
        {
            "id": "idle_normal_01",
            "time_range": "default",
            "text": "三分钟没动了，是在发呆吗？",
            "audio_cache": "assets/voice/idle_normal_01.mp3",
            "anim_speed": "normal",
            "probability": 1.0,
            "cooldown_minutes": 10,
            "tags": ["idle", "general"]
        },
        {
            "id": "lunch_break_01",
            "time_range": "12:00-13:00",
            "text": "不去吃饭的话，下午代码质量会下降 30% 哦。",
            "audio_cache": null,
            "anim_speed": "normal",
            "probability": 0.6,
            "cooldown_minutes": 60,
            "tags": ["health", "lunch"]
        }
    ],
    "panic_events": [
        {
            "id": "panic_default",
            "text": "哇！被发现了！",
            "audio_cache": "assets/voice/panic_01.mp3",
            "probability": 0.5
        },
        {
            "id": "panic_shy",
            "text": "才...才没有在偷看你...",
            "audio_cache": null,
            "probability": 0.5
        }
    ]
}
```

### 2.4.2 字段说明

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | ✅ | 台词唯一标识，用于缓存音频文件 |
| `time_range` | string | ✅ | 时间范围，格式 `"HH:MM-HH:MM"` 或 `"default"` |
| `text` | string | ✅ | 台词文本内容 |
| `audio_cache` | string\|null | ❌ | 预生成的音频文件路径，null 则运行时 TTS 生成 |
| `anim_speed` | string | ❌ | 动画速度: `"slow"` / `"normal"` / `"fast"` |
| `probability` | float | ❌ | 触发概率 (0.0-1.0)，默认 1.0 |
| `cooldown_minutes` | int | ❌ | 冷却时间(分钟)，避免同一台词重复播放 |
| `tags` | string[] | ❌ | 标签，用于后期过滤和扩展 |

---

## 2.5 本阶段完成后的系统能力

完成 Phase 2 后，系统应能展示以下完整流程：

```
[用户离开 3 分钟]
    → IdleMonitor 检测到空闲
    → Director 选择台词
    → EntityWindow 从右侧探头 (OutBack, 1.5s)
    → 停顿 2 秒
    → EntityWindow 弹跳入场 (OutBounce, 0.8s)
    → TTS 播放: "三分钟没动了，是在发呆吗？"
    
[用户移动鼠标]
    → IdleMonitor 检测到活跃
    → 中断 TTS
    → EntityWindow 极速缩回 (InExpo, 0.3s)
    → 窗口隐藏，系统回到待命状态
```