# 🖥️ Project Cyber-Companion 用户指南

## —— 环境配置、系统需求与 API Key 完全指南

---

## 📋 目录

1. [系统需求与兼容性](#1-系统需求与兼容性)
2. [笔记本电脑能跑吗？— 性能评估](#2-笔记本电脑能跑吗性能评估)
3. [需要哪些 API Key？— 完整清单](#3-需要哪些-api-key完整清单)
4. [开发环境搭建（一步步来）](#4-开发环境搭建一步步来)
5. [各阶段硬件需求对照](#5-各阶段硬件需求对照)
6. [常见问题 FAQ](#6-常见问题-faq)

---

## 1. 系统需求与兼容性

### ✅ 你的系统: Windows 11 专业版 — 完全兼容

| 项目 | 要求 | Windows 11 专业版 | 状态 |
|------|------|-------------------|------|
| 操作系统 | Windows 10 / 11 | ✅ Windows 11 Pro | ✅ 完全兼容 |
| 系统架构 | x64 (64位) | ✅ 默认 64 位 | ✅ |
| User32.dll | Windows 原生 API | ✅ 系统自带 | ✅ |
| Windows OCR | 系统内置 OCR | ✅ Win11 内置 | ✅ 无需额外安装 |
| 高 DPI 支持 | 缩放适配 | ✅ Win11 原生支持 | ✅ |

> **结论**: Windows 11 专业版是本项目**最理想的运行环境**。所有 Windows API（`GetLastInputInfo`、`GetForegroundWindow`、Windows OCR）都完全可用。

---

## 2. 笔记本电脑能跑吗？— 性能评估

### 🔑 关键结论：能跑！但需要分阶段看

本项目分 5 个阶段，**并不是每个阶段都需要高配**。核心功能（Phase 1-3）对硬件要求极低。

### 各阶段资源消耗

```
阶段负载 (从轻到重):

Phase 1-3 (核心功能)      ████░░░░░░░░░░░░░░░░  极轻量
  - CPU: < 2%
  - 内存: < 100MB
  - GPU: 不需要
  - 网络: 仅 TTS 需要（可选）

Phase 4A (视觉感知)       ████████░░░░░░░░░░░░  中等
  - CPU: ~10% (MediaPipe)  
  - 内存: ~300MB
  - GPU: 不需要（CPU 推理）
  - 摄像头: 需要

Phase 4B (本地 LLM)       ██████████████████░░  较重
  - CPU: ~50-80% (推理时)
  - 内存: 4-8GB (模型常驻)
  - GPU: 强烈推荐
  - 硬盘: 5-15GB (模型文件)

Phase 4B (云端 LLM 替代)  ████░░░░░░░░░░░░░░░░  极轻量
  - CPU: < 1%
  - 内存: < 10MB
  - GPU: 不需要
  - 网络: 需要稳定联网
```

### 笔记本电脑配置分档

#### 🟢 入门级笔记本（4GB 内存 / 集成显卡）

```
可运行:
  ✅ Phase 1: 幽灵窗口 + ASCII 渲染
  ✅ Phase 2: 空闲检测 + 动画系统
  ✅ Phase 3: TTS 语音 + 状态机 + 角色系统
  ⚠️ Phase 4: 仅云端 LLM 模式（不能跑本地 LLM）
  ✅ Phase 5: 打包发布

不可运行:
  ❌ 本地 LLM (7B 模型需要至少 8GB 内存)
  ⚠️ MediaPipe 视觉可能会卡顿
```

#### 🟡 中端笔记本（8GB 内存 / 集成显卡或入门独显）

```
可运行:
  ✅ Phase 1-3: 所有核心功能
  ✅ Phase 4 视觉感知 (MediaPipe 可在 CPU 流畅运行)
  ⚠️ Phase 4 本地 LLM: 可以跑 3B 量化模型，但推理较慢 (5-15秒/回复)
  ✅ Phase 4 云端 LLM: 完全流畅
  ✅ Phase 5: 打包发布
```

#### 🟢 高端笔记本（16GB+ 内存 / 独立显卡 RTX 3060+）

```
可运行:
  ✅ 所有功能完全流畅
  ✅ 本地 LLM 7B 模型 (2-5秒/回复)
  ✅ MediaPipe 30FPS 流畅运行
  ✅ 多进程架构无压力
```

### 💡 推荐策略

> **如果你的笔记本内存 ≤ 8GB**：
> - Phase 1-3 完全没有问题，放心跑
> - Phase 4 的 LLM 功能请**使用云端 API 方案**（OpenAI / DeepSeek），不要跑本地模型
> - 这样整个项目总内存占用 < 300MB，任何笔记本都能轻松运行

---

## 3. 需要哪些 API Key？— 完整清单

### 🔑 核心答案

**不是所有功能都需要 API Key！** Phase 1-3 的核心功能**完全不需要**任何 API Key。

### 完整 API Key 需求表

| API Key | 用途 | 必需？ | 费用 | 何时需要 |
|---------|------|--------|------|---------|
| **无需 Key** | Phase 1-3 核心功能 | — | 免费 | 开发 MVP 时 |
| **edge-tts** | 语音合成 (TTS) | ❌ **不需要 Key** | **完全免费** | Phase 3 语音功能 |
| **OpenAI API Key** | GPT-4o 云端 LLM | ⚠️ 可选 | 按量付费 | Phase 4 屏幕吐槽 |
| **DeepSeek API Key** | DeepSeek 云端 LLM | ⚠️ 可选 | 按量付费（便宜） | Phase 4 屏幕吐槽 |
| **Ollama** | 本地 LLM | ❌ **不需要 Key** | **完全免费** | Phase 4 本地 LLM |
| **GitHub Token** | CI/CD 自动发布 | ⚠️ 仅发布需要 | 免费 | Phase 5 自动化 |

### 详细说明

---

#### 1. 🆓 edge-tts（语音合成）— 无需 Key，完全免费

```
API Key: ❌ 不需要
费用:    🆓 完全免费
原理:    调用微软 Edge 浏览器内置的在线 TTS 接口
要求:    需要联网（每次生成语音需请求微软服务器）
限制:    无官方限制，但可能被微软随时变更
离线:    不可用（但已缓存的语音可离线播放）
```

**使用示例**:
```python
# 无需任何 Key，直接使用
import edge_tts
communicate = edge_tts.Communicate("你好世界", "zh-CN-XiaoxiaoNeural")
await communicate.save("hello.mp3")
```

**可用中文语音包**:
| 语音 | 代码 | 性别 | 风格 |
|------|------|------|------|
| 晓晓 | `zh-CN-XiaoxiaoNeural` | 女 | 温暖自然（推荐） |
| 晓伊 | `zh-CN-XiaoyiNeural` | 女 | 活泼开朗 |
| 云希 | `zh-CN-YunxiNeural` | 男 | 清亮 |
| 云健 | `zh-CN-YunjianNeural` | 男 | 沉稳 |
| 晓辰 | `zh-CN-XiaochenNeural` | 女 | 知性 |

---

#### 2. ⚠️ OpenAI API Key（云端 LLM）— 可选

```
API Key:  ✅ 需要
费用:     💰 按量付费
获取地址: https://platform.openai.com/api-keys
注册要求: 需要海外手机号或虚拟号
支付方式: Visa / Mastercard 信用卡
```

**费用估算** (以 GPT-4o-mini 为例):
```
每次屏幕吐槽:
  - 输入 token: ~200 (屏幕 OCR 文本)
  - 输出 token: ~50 (吐槽文本)
  - 费用: 约 $0.00003/次

每日使用 50 次:
  - 日费用: ~$0.0015
  - 月费用: ~$0.05 (约 ¥0.35)

结论: 极其便宜，基本可忽略不计
```

**推荐模型**:
| 模型 | 价格 | 速度 | 质量 | 推荐度 |
|------|------|------|------|--------|
| `gpt-4o-mini` | 极低 | 快 | 够用 | ⭐⭐⭐⭐⭐ |
| `gpt-4o` | 中等 | 中 | 极好 | ⭐⭐⭐ |
| `gpt-3.5-turbo` | 极低 | 极快 | 一般 | ⭐⭐⭐ |

---

#### 3. ⚠️ DeepSeek API Key（云端 LLM 替代方案）— 可选

```
API Key:  ✅ 需要
费用:     💰 按量付费（比 OpenAI 便宜很多）
获取地址: https://platform.deepseek.com/api_keys
注册要求: 中国手机号即可
支付方式: 支持支付宝
```

**费用估算**:
```
DeepSeek-V3 / DeepSeek-Chat:
  - 输入: ¥0.001/千 token
  - 输出: ¥0.002/千 token
  - 每次吐槽约 ¥0.0005
  - 月费用 (50次/天): ¥0.75

结论: 极便宜 + 中文能力强 + 国内注册方便
```

**与 OpenAI 对比**:
| 对比项 | OpenAI | DeepSeek |
|--------|--------|----------|
| 注册难度 | 需海外手机/代理 | 中国手机号直接注册 |
| 支付方式 | Visa/MC 信用卡 | **支付宝** |
| 中文能力 | 好 | **非常好** |
| 费用 | 低 | **极低** |
| API 兼容 | 标准 | **兼容 OpenAI 格式** |
| 推荐度 | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ **首选推荐** |

> **💡 强烈推荐 DeepSeek**: 国内手机号注册、支付宝充值、中文能力更强、API 兼容 OpenAI 格式（代码几乎不用改），费用更低。

**使用方式**（与 OpenAI 完全兼容）:
```python
from openai import OpenAI

# 只需修改 base_url 和 api_key
client = OpenAI(
    api_key="sk-xxxxxxxxx",                    # DeepSeek API Key
    base_url="https://api.deepseek.com/v1"     # DeepSeek 端点
)

response = client.chat.completions.create(
    model="deepseek-chat",
    messages=[{"role": "user", "content": "你好"}],
)
```

---

#### 4. 🆓 Ollama（本地 LLM）— 无需 Key，完全免费

```
API Key: ❌ 不需要
费用:    🆓 完全免费（模型运行在你自己电脑上）
下载:    https://ollama.com
要求:    建议 8GB+ 内存 / 推荐 16GB+
```

**安装与使用**:
```powershell
# 1. 从 https://ollama.com 下载安装 (Windows 版)

# 2. 拉取中文模型 (选一个即可):

# 轻量级 (3B, 需要 ~2GB 内存, 笔记本友好):
ollama pull qwen2.5:3b

# 标准级 (7B, 需要 ~4-8GB 内存, 推荐):
ollama pull qwen2.5:7b

# 3. 测试是否能用:
ollama run qwen2.5:7b "用一句话吐槽一个正在加班的程序员"
```

**推荐模型对照**:
| 模型 | 大小 | 内存需求 | 中文质量 | 推理速度 | 适合 |
|------|------|---------|---------|---------|------|
| `qwen2.5:3b` | ~2GB | 4GB | 一般 | 快 | 4-8GB 内存笔记本 |
| `qwen2.5:7b` | ~4.5GB | 8GB | 好 | 中 | 8-16GB 内存 |
| `qwen2.5:14b` | ~9GB | 16GB | 很好 | 慢 | 16GB+ 内存 |
| `llama3.2:3b` | ~2GB | 4GB | 差 | 快 | 英文场景 |
| `mistral:7b` | ~4.1GB | 8GB | 一般 | 中 | 通用 |

> **推荐**: 中文场景首选 `qwen2.5` 系列（阿里通义千问），中文能力最强。

---

#### 5. 🆓 GitHub Token（CI/CD）— 仅发布时需要

```
API Key: ⚠️ 仅 Phase 5 发布时需要
费用:    🆓 免费 (GitHub 免费账户即可)
获取:    GitHub 仓库自动提供 GITHUB_TOKEN
```

**说明**: GitHub Actions 的 `GITHUB_TOKEN` 是**自动生成的**，不需要你手动创建。只要你的代码在 GitHub 仓库中，推送 tag 时自动触发构建。

---

### 🎯 API Key 推荐配置方案

#### 方案 A: 纯免费方案（推荐新手）

```
✅ edge-tts     → 免费，无需 Key
✅ Ollama       → 免费，无需 Key，本地运行
❌ OpenAI       → 不需要
❌ DeepSeek     → 不需要

总费用: ¥0
适合: 有 8GB+ 内存的笔记本
```

#### 方案 B: 低成本云端方案（推荐性价比）

```
✅ edge-tts     → 免费，无需 Key
✅ DeepSeek API → 约 ¥1/月，支付宝充值
❌ OpenAI       → 不需要
❌ Ollama       → 不需要

总费用: ~¥1/月
适合: 任何笔记本（包括 4GB 内存）
```

#### 方案 C: 最强配置方案

```
✅ edge-tts     → 免费
✅ DeepSeek API → 备用云端（低配时）
✅ Ollama       → 本地运行（高配时）
⚠️ OpenAI      → 可选备用

总费用: ~¥1-5/月
适合: 16GB+ 内存笔记本
```

---

## 4. 开发环境搭建（一步步来）

### 4.1 安装 Python

```powershell
# 检查是否已安装 Python
python --version
# 期望输出: Python 3.10.x ~ 3.12.x

# 如果没有安装:
# 1. 访问 https://www.python.org/downloads/
# 2. 下载 Python 3.11.x (推荐稳定版)
# 3. 安装时 ⚠️ 勾选 "Add Python to PATH"
```

### 4.2 创建项目

```powershell
# 1. 创建项目目录
mkdir C:\Projects\CyberCompanion
cd C:\Projects\CyberCompanion

# 2. 创建虚拟环境
python -m venv .venv

# 3. 激活虚拟环境
# ⚠️ 如果 PowerShell 报错 "无法加载文件...因为在此系统上禁止运行脚本"
# 先执行（管理员模式）:
# Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
.\.venv\Scripts\Activate.ps1

# 激活成功后命令行会显示 (.venv) 前缀
```

### 4.3 安装依赖

```powershell
# 确保虚拟环境已激活 (前缀显示 .venv)

# Phase 1-3 核心依赖 (必装, 约 150MB)
pip install PySide6>=6.6.0 Pillow>=10.0.0 numpy>=1.24.0 edge-tts>=6.1.0 pywin32>=306

# Phase 4 可选依赖 (按需安装):

# 视觉感知 (如果你的笔记本有摄像头):
pip install mediapipe>=0.10.0 opencv-python-headless>=4.9.0

# 屏幕截图 + OCR:
pip install mss>=9.0.0

# 云端 LLM (选一个):
pip install openai>=1.0.0    # OpenAI / DeepSeek 都用这个包
pip install httpx>=0.24.0    # HTTP 客户端

# 冻结依赖列表
pip freeze > requirements.txt
```

### 4.4 创建项目目录结构

```powershell
# 创建所有必要的目录和文件
mkdir src\core, src\ui, src\ai
mkdir assets\sprites, assets\sounds, assets\models
mkdir characters\rem_maid\assets\sprites
mkdir characters\rem_maid\assets\sounds
mkdir characters\rem_maid\scripts
mkdir tests, tools

# 创建 Python 包初始化文件
New-Item src\__init__.py -ItemType File
New-Item src\core\__init__.py -ItemType File
New-Item src\ui\__init__.py -ItemType File
New-Item src\ai\__init__.py -ItemType File

# 创建核心源文件
New-Item src\main.py -ItemType File
New-Item src\core\idle_monitor.py -ItemType File
New-Item src\core\director.py -ItemType File
New-Item src\core\asset_manager.py -ItemType File
New-Item src\core\audio_manager.py -ItemType File
New-Item src\core\state_machine.py -ItemType File
New-Item src\core\script_engine.py -ItemType File
New-Item src\core\paths.py -ItemType File
New-Item src\core\logger.py -ItemType File
New-Item src\ui\entity_window.py -ItemType File
New-Item src\ui\ascii_renderer.py -ItemType File
New-Item src\ui\tray_icon.py -ItemType File
New-Item src\ai\gaze_tracker.py -ItemType File
New-Item src\ai\llm_provider.py -ItemType File
New-Item src\ai\screen_commentator.py -ItemType File
New-Item src\ai\presence_detector.py -ItemType File
```

### 4.5 配置 API Key（如果使用云端 LLM）

#### 方法 A: 环境变量（推荐）

```powershell
# 设置 DeepSeek API Key (推荐)
[Environment]::SetEnvironmentVariable("DEEPSEEK_API_KEY", "sk-你的密钥", "User")

# 或设置 OpenAI API Key
[Environment]::SetEnvironmentVariable("OPENAI_API_KEY", "sk-你的密钥", "User")

# 设置完后需要重启终端才能生效
```

#### 方法 B: 项目配置文件

```json
// config.json (项目根目录)
// ⚠️ 把这个文件加入 .gitignore，不要提交到 Git！
{
    "llm": {
        "provider": "deepseek",
        "api_key": "sk-你的密钥",
        "base_url": "https://api.deepseek.com/v1",
        "model": "deepseek-chat"
    }
}
```

> ⚠️ **安全提醒**: API Key 绝不能写死在代码中或提交到 Git 仓库！

### 4.6 安装 Ollama（如果使用本地 LLM）

```powershell
# 1. 下载安装 Ollama
# 访问 https://ollama.com/download/windows 下载安装包
# 双击安装即可

# 2. 安装完成后，打开新的终端
# 验证安装
ollama --version

# 3. 拉取中文模型（根据内存选择）
# 8GB 内存笔记本:
ollama pull qwen2.5:3b

# 16GB 内存笔记本:
ollama pull qwen2.5:7b

# 4. 测试模型
ollama run qwen2.5:3b "用一句话吐槽一个半夜还在写代码的程序员"

# 5. Ollama 会在后台运行一个 API 服务
# 默认地址: http://localhost:11434
# 程序会自动连接这个地址，无需额外配置
```

### 4.7 验证环境

```powershell
# 运行验证脚本
python -c "
import sys
print(f'Python 版本: {sys.version}')

# 检查核心依赖
try:
    import PySide6; print(f'✅ PySide6: {PySide6.__version__}')
except: print('❌ PySide6 未安装')

try:
    import PIL; print(f'✅ Pillow: {PIL.__version__}')
except: print('❌ Pillow 未安装')

try:
    import numpy; print(f'✅ NumPy: {numpy.__version__}')
except: print('❌ NumPy 未安装')

try:
    import edge_tts; print(f'✅ edge-tts: 已安装')
except: print('❌ edge-tts 未安装')

try:
    import win32api; print(f'✅ pywin32: 已安装')
except: print('❌ pywin32 未安装')

# 检查可选依赖
try:
    import cv2; print(f'✅ OpenCV: {cv2.__version__}')
except: print('⚠️ OpenCV 未安装 (Phase 4 可选)')

try:
    import mediapipe; print(f'✅ MediaPipe: {mediapipe.__version__}')
except: print('⚠️ MediaPipe 未安装 (Phase 4 可选)')

try:
    import openai; print(f'✅ OpenAI SDK: {openai.__version__}')
except: print('⚠️ OpenAI SDK 未安装 (Phase 4 可选)')

# 检查 Windows API
import ctypes
try:
    user32 = ctypes.windll.user32
    print(f'✅ User32.dll: 可用')
except: print('❌ User32.dll: 不可用')

# 检查 Ollama
import urllib.request
try:
    r = urllib.request.urlopen('http://localhost:11434/api/tags', timeout=2)
    print(f'✅ Ollama: 运行中')
except: print('⚠️ Ollama: 未运行 (Phase 4 可选)')

print()
print('环境检查完成！')
"
```

---

## 5. 各阶段硬件需求对照

### 一张表说清楚

| | Phase 1-2 | Phase 3 | Phase 4 (视觉) | Phase 4 (本地LLM) | Phase 4 (云端LLM) | Phase 5 |
|---|---|---|---|---|---|---|
| **CPU** | 任意 | 任意 | i5+ | i5+ | 任意 | 任意 |
| **内存** | 2GB+ | 2GB+ | 4GB+ | 8GB+（推荐16GB） | 2GB+ | 4GB+ |
| **GPU** | ❌ | ❌ | ❌ | ⚠️ 推荐 | ❌ | ❌ |
| **硬盘** | 500MB | 500MB | 1GB | 5-15GB | 500MB | 2GB |
| **摄像头** | ❌ | ❌ | ✅ 需要 | ❌ | ❌ | ❌ |
| **网络** | ❌ | ⚠️ TTS 需要 | ❌ | ❌ | ✅ 需要 | ⚠️ 发布时 |
| **API Key** | ❌ | ❌ | ❌ | ❌ | ✅ 需要 | ❌ |
| **费用** | ¥0 | ¥0 | ¥0 | ¥0 | ~¥1/月 | ¥0 |
| **难度** | ⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐ |

### 📱 快速判断你的笔记本配置

按 `Win + Pause` 或 `设置 → 系统 → 关于` 查看：

```powershell
# 或者在 PowerShell 中运行:
systeminfo | findstr /B /C:"OS 名称" /C:"系统类型" /C:"处理器"
wmic memorychip get capacity
# 内存容量以字节为单位：
# 4294967296 = 4GB
# 8589934592 = 8GB
# 17179869184 = 16GB
```

---

## 6. 常见问题 FAQ

### Q: 我完全不需要任何 API Key 就能开始开发吗？

**A: 是的！** Phase 1-3 的核心功能（ASCII 角色 + 空闲检测 + 动画 + TTS 语音）**完全不需要** 任何 API Key。edge-tts 是免费的，不需要注册。

### Q: 没有摄像头可以做 Phase 4 吗？

**A:** Phase 4 的视觉感知功能需要摄像头。但 Phase 4 的其他功能（LLM 吐槽、心情系统）不需要摄像头，可以独立使用。你可以在 config.json 中禁用视觉功能：
```json
{
    "features": {
        "camera_enabled": false,
        "llm_enabled": true
    }
}
```

### Q: DeepSeek 和 OpenAI 只需要选一个对吗？

**A: 对！** 它们都是云端 LLM 提供者，选一个就行。**国内用户强烈推荐 DeepSeek**——中国手机号注册、支付宝付款、中文能力更强、价格更低。

### Q: Ollama 和 DeepSeek/OpenAI 有什么区别？

**A:**
| | Ollama (本地) | DeepSeek/OpenAI (云端) |
|---|---|---|
| **运行位置** | 你的电脑本地 | 远程服务器 |
| **费用** | 完全免费 | 按量付费 (~¥1/月) |
| **隐私** | ✅ 数据不离开电脑 | ⚠️ 数据发送到服务器 |
| **速度** | 取决于你的硬件 | 通常 1-3 秒 |
| **内存** | 需要 4-8GB | 不占本地内存 |
| **网络** | ❌ 不需要 | ✅ 需要 |

你可以两个都配置，让程序自动选择：有 Ollama 就用本地，没有就降级到云端。

### Q: edge-tts 断网了怎么办？

**A:** 已生成过的语音会自动缓存在本地 (`cache/audio/` 目录)。断网后：
- 之前说过的台词 → 从缓存播放，正常工作
- 新台词 → 静默跳过语音，不会崩溃

### Q: 打包后的 .exe 文件有多大？

**A:**
| 配置 | 预估体积 |
|------|---------|
| Phase 1-3 核心 | ~150-200MB |
| + Phase 4 视觉 (MediaPipe) | ~250MB |
| + Phase 4 本地 LLM | 不打包进去（Ollama 独立安装） |
| + OpenCV 完整版（❌ 不推荐） | +130MB |
| + OpenCV headless（✅ 推荐） | +30MB |

### Q: 可以同时用本地 LLM 和云端 LLM 吗？

**A: 可以！** 推荐策略：
```python
# 优先使用本地，超时或不可用时降级到云端
if ollama_provider.is_available():
    response = await ollama_provider.generate(request)
else:
    response = await deepseek_provider.generate(request)
```

### Q: 我如何获取 DeepSeek API Key？

**A:**
1. 访问 https://platform.deepseek.com
2. 使用中国手机号注册
3. 登录后进入「API Keys」页面
4. 点击「创建 API Key」
5. 复制密钥（`sk-` 开头）
6. 按照 4.5 节的方法配置到环境变量

### Q: 我如何知道自己的笔记本有多少内存？

**A:**
```powershell
# PowerShell 快速查看
(Get-CimInstance Win32_PhysicalMemory | Measure-Object -Property Capacity -Sum).Sum / 1GB
# 输出示例: 16 (代表 16GB)
```

---

## 📝 快速开始检查清单

```
□ Windows 11 专业版                    ✅ 你已满足
□ Python 3.10-3.12 已安装              □ 需检查
□ 虚拟环境已创建                        □ 需执行
□ Phase 1-3 核心依赖已安装              □ 需执行
□ 验证脚本全部通过                      □ 需执行
□ (可选) DeepSeek 账号已注册            □ 按需
□ (可选) Ollama 已安装                  □ 按需
□ (可选) 摄像头可用                     □ 按需
□ 开始编写 Phase 1 代码！               □ 🚀
```

---

> **最后的话**: 不要被文档的复杂度吓到！Phase 1 的核心代码只有 3 个文件，总共不到 300 行。从最简单的 `IdleMonitor` 类开始写起 —— 一个能在终端打印"你已经发呆 10 秒了"的 Python 脚本，就是你的第一步。
