# 📝 Project Cyber-Companion 开发文档 (Phase 5)

## —— 工程化实施：打包、优化与发布

---

## 5.0 阶段概述 (Phase Overview)

这是构建软件产品中**极其重要但经常被忽视**的一环：如何让代码走出 IDE，运行在别人的电脑上。

对于 Python GUI 应用（尤其涉及 AI 库），**打包体积大、启动慢、依赖冲突**是三大恶梦。

### 本阶段目标

| 目标 | 指标 | 实现方案 |
|------|------|---------|
| **独立运行** | 在无 Python 环境的电脑上运行 | PyInstaller 打包 |
| **体积控制** | 核心包 < 200MB | 依赖精简 + 排除策略 |
| **快速启动** | 冷启动 < 5 秒 | 懒加载 + 启动画面 |
| **持续交付** | 一键发布 | GitHub Actions CI/CD |
| **自动更新** | 用户无感更新 | Launcher + OTA |

### 前置依赖

- Phase 1-4 全部代码开发完成
- 所有模块通过单元测试
- GitHub 仓库已创建

---

## 5.1 打包策略 (Packaging Strategy)

### 核心工具: PyInstaller

> ⚠️ **不要直接用命令行** `pyinstaller main.py`！必须编写 `.spec` 文件进行精细控制。

### 5.1.1 完整 `.spec` 文件

```python
# build.spec
# PyInstaller 打包配置文件
# 使用方法: pyinstaller build.spec

import sys
from pathlib import Path

# 项目根目录
PROJECT_ROOT = Path(SPECPATH)

a = Analysis(
    # ── 入口文件 ──
    [str(PROJECT_ROOT / 'src' / 'main.py')],
    
    # ── 搜索路径 ──
    pathex=[str(PROJECT_ROOT / 'src')],
    
    # ── 二进制文件 ──
    binaries=[],
    
    # ── 资源文件映射 ──
    # 格式: (源路径, 目标路径)
    # 注意: 目标路径是相对于打包后 _internal 目录的路径
    datas=[
        # 角色资源包
        (str(PROJECT_ROOT / 'characters'), 'characters'),
        # 静态资源
        (str(PROJECT_ROOT / 'assets'), 'assets'),
        # 配置文件
        (str(PROJECT_ROOT / 'config.json'), '.'),
        (str(PROJECT_ROOT / 'version.json'), '.'),
    ],
    
    # ── 隐式导入 ──
    # PyInstaller 无法通过静态分析发现的动态导入
    hiddenimports=[
        # PySide6 多媒体
        'PySide6.QtMultimedia',
        'PySide6.QtMultimediaWidgets',
        
        # edge-tts 依赖
        'edge_tts',
        'aiohttp',
        
        # MediaPipe (如果打包 Phase 4)
        # 'mediapipe',
        # 'mediapipe.python',
        # 'mediapipe.python.solutions',
        
        # OpenCV (如果需要)
        # 'cv2',
        
        # 其他可能的动态导入
        'json',
        'yaml',
        'hashlib',
    ],
    
    # ── 排除列表 ──
    # 不需要的重型库（大幅减小体积）
    excludes=[
        # GUI 相关（我们用 PySide6，不需要 tkinter）
        'tkinter',
        '_tkinter',
        'tk',
        
        # 数据科学（如果 numpy 拉入了这些）
        'matplotlib',
        'scipy',
        'pandas',
        'sklearn',
        'scikit-learn',
        
        # Jupyter / IPython
        'IPython',
        'jupyter',
        'notebook',
        'ipykernel',
        'ipywidgets',
        
        # 测试框架
        'pytest',
        'unittest',
        'doctest',
        
        # 不需要的标准库
        'lib2to3',
        'pydoc_data',
        'ensurepip',
        'venv',
    ],
    
    # ── 运行时钩子 ──
    runtime_hooks=[
        str(PROJECT_ROOT / 'tools' / 'runtime_hook.py'),
    ],
    
    hookspath=[],
    hooksconfig={},
    
    # ── 编码 ──
    noarchive=False,
)

# ── PYZ 压缩 ──
pyz = PYZ(a.pure, a.zipped_data)

# ── 可执行文件配置 ──
exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,    # True = 生成目录模式（推荐）
    name='CyberCompanion',    # 可执行文件名
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,              # Windows 上不要 strip
    upx=True,                 # 使用 UPX 压缩 DLL（需安装 UPX）
    console=False,            # False = 无控制台窗口（GUI 模式）
    disable_windowed_traceback=False,
    argv_emulation=False,
    
    # ── 图标 ──
    icon=str(PROJECT_ROOT / 'assets' / 'icon.ico'),
    
    # ── Windows 元数据 ──
    version='file_version_info.txt',  # 可选：Windows 文件版本信息
)

# ── 目录模式收集 ──
coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[
        # 这些 DLL 不能被 UPX 压缩（会导致崩溃）
        'vcruntime140.dll',
        'python3*.dll',
        'Qt6*.dll',           # Qt DLL 压缩后可能不稳定
    ],
    name='CyberCompanion',    # 输出目录名
)
```

### 5.1.2 运行时钩子

```python
# tools/runtime_hook.py
# 在程序启动的最早期执行，早于任何业务代码

import os
import sys


def _setup_environment():
    """
    设置运行时环境
    
    解决的问题:
    1. 打包后资源文件路径变化
    2. DLL 搜索路径
    3. 编码问题
    """
    # 判断是否是打包后的环境
    if getattr(sys, 'frozen', False):
        # 打包后的根目录
        base_dir = sys._MEIPASS
    else:
        # 开发环境
        base_dir = os.path.dirname(os.path.abspath(__file__))
    
    # 设置环境变量
    os.environ['APP_BASE_DIR'] = base_dir
    
    # 确保 UTF-8 编码
    os.environ['PYTHONIOENCODING'] = 'utf-8'


_setup_environment()
```

### 5.1.3 资源路径工具

```python
# src/core/paths.py

import os
import sys
from pathlib import Path


def get_base_dir() -> Path:
    """
    获取应用程序根目录
    
    开发环境: 项目根目录
    打包环境: PyInstaller 的 _MEIPASS 临时目录
    
    使用:
        base = get_base_dir()
        config_path = base / "config.json"
        sprites_dir = base / "characters" / "rem_maid" / "assets" / "sprites"
    """
    if getattr(sys, 'frozen', False):
        return Path(sys._MEIPASS)
    else:
        return Path(__file__).parent.parent.parent  # src/core/paths.py → 项目根目录


def get_user_data_dir() -> Path:
    """
    获取用户数据目录 (可写入)
    
    打包后的 _MEIPASS 是只读的！
    用户数据（缓存、设置、日志）必须写到用户目录
    
    路径: C:/Users/<username>/AppData/Local/CyberCompanion/
    """
    if sys.platform == 'win32':
        base = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData' / 'Local'))
    else:
        base = Path.home() / '.local' / 'share'
    
    user_dir = base / 'CyberCompanion'
    user_dir.mkdir(parents=True, exist_ok=True)
    return user_dir


def get_cache_dir() -> Path:
    """音频缓存目录"""
    cache_dir = get_user_data_dir() / 'cache' / 'audio'
    cache_dir.mkdir(parents=True, exist_ok=True)
    return cache_dir


def get_log_dir() -> Path:
    """日志目录"""
    log_dir = get_user_data_dir() / 'logs'
    log_dir.mkdir(parents=True, exist_ok=True)
    return log_dir
```

### 5.1.4 依赖精简策略 (Dependency Pruning)

> ⚠️ **核心原则**: 使用干净的虚拟环境打包，绝不使用全局 Python 环境！

#### 打包环境搭建

```powershell
# 1. 创建干净的打包虚拟环境（与开发环境分开）
python -m venv build_env

# 2. 激活
.\build_env\Scripts\Activate.ps1

# 3. 只安装最小必需依赖
pip install -r requirements-build.txt

# 4. 安装 PyInstaller
pip install pyinstaller>=6.0.0

# 5. 打包
pyinstaller build.spec

# 6. 输出在 dist/CyberCompanion/ 目录
```

#### `requirements-build.txt` — 打包专用依赖清单

```txt
# Phase 1-3 核心依赖
PySide6==6.6.1                # 锁定精确版本
Pillow==10.2.0
numpy==1.26.4
edge-tts==6.1.12
pywin32==306

# Phase 4 可选依赖（根据需要取消注释）
# mediapipe==0.10.9
# opencv-python-headless==4.9.0.80    # headless 版本！比完整版小 100MB+
# mss==9.0.1
# paddleocr==2.7.3
# httpx==0.27.0
# openai==1.12.0

# ⚠️ 注意:
# - 使用 opencv-python-headless 而不是 opencv-python
#   headless 版本不包含 GUI 功能（我们用 PySide6，不需要 cv2 GUI）
#   体积: headless ~30MB vs 完整版 ~130MB
# - PaddleOCR 体积大（~500MB），考虑使用 Windows 原生 OCR 替代
```

### 5.1.5 体积对照表

| 组件 | 完整包 | 精简后 | 节省 |
|------|--------|--------|------|
| PySide6 | ~150MB | ~80MB (排除 Designer/QML) | 70MB |
| NumPy | ~30MB | ~30MB | - |
| Pillow | ~10MB | ~10MB | - |
| OpenCV (完整) | ~130MB | - | 130MB |
| OpenCV (headless) | - | ~30MB | 100MB |
| MediaPipe | ~30MB | ~30MB | - |
| PaddleOCR | ~500MB | ~0 (用原生替代) | 500MB |
| tkinter | ~20MB | ~0 (排除) | 20MB |
| **总计** | ~870MB | ~180MB | **690MB** |

---

## 5.2 性能优化 (Performance Optimization)

### 5.2.1 启动速度优化：懒加载 (Lazy Loading)

Python 解释器启动很快（~50ms），但 `import` 巨重的库非常慢。

**问题**: 如果代码顶部写了 `import torch` 或 `import cv2`，程序启动时会卡 3-5 秒加载 DLL。

#### 设计模式：代理导入 (Proxy Import)

```python
# ❌ Bad Practice — 程序一启动就卡住加载所有 AI 库

import cv2                    # 加载时间: ~800ms
import mediapipe as mp        # 加载时间: ~1200ms  
import edge_tts               # 加载时间: ~200ms
from paddleocr import PaddleOCR  # 加载时间: ~3000ms

class GazeTracker:
    def detect_face(self, frame):
        ...
```

```python
# ✅ Good Practice — 只在功能首次使用时才加载对应库

class GazeTracker:
    """
    懒加载示例:
    - cv2 和 mediapipe 只在第一次调用 detect_face() 时加载
    - 用户如果从未使用视觉功能，这些库永远不会被加载
    - 首次调用会有 1-2 秒延迟，后续调用无延迟
    """
    
    _cv2 = None          # 类级别缓存
    _mediapipe = None
    
    @classmethod
    def _ensure_cv2(cls):
        """延迟加载 OpenCV"""
        if cls._cv2 is None:
            import cv2
            cls._cv2 = cv2
        return cls._cv2
    
    @classmethod
    def _ensure_mediapipe(cls):
        """延迟加载 MediaPipe"""
        if cls._mediapipe is None:
            import mediapipe
            cls._mediapipe = mediapipe
        return cls._mediapipe
    
    def detect_face(self, frame):
        cv2 = self._ensure_cv2()         # 首次调用: ~800ms, 后续: ~0ms
        mp = self._ensure_mediapipe()     # 首次调用: ~1200ms, 后续: ~0ms
        # ... 使用 cv2 和 mp ...
```

#### 启动流程优化

```python
# src/main.py — 优化后的启动流程

import os
import sys

# ── Step 1: 环境设置 (< 1ms) ──
os.environ["QT_ENABLE_HIGHDPI_SCALING"] = "1"

# ── Step 2: 最小 GUI 导入 (< 200ms) ──
from PySide6.QtWidgets import QApplication

# ── Step 3: 启动画面 (< 50ms) ──
app = QApplication(sys.argv)

# 如果使用 PyInstaller 的内置 splash
try:
    import pyi_splash
    pyi_splash.update_text("正在唤醒赛博伴侣...")
except ImportError:
    pass  # 非打包环境

# ── Step 4: 加载核心模块 (< 100ms) ──
# 只加载 GUI 和监控 — 这些都很轻量
from core.idle_monitor import IdleMonitor
from core.director import Director
from ui.entity_window import EntityWindow

# ── Step 5: 初始化 (< 50ms) ──
window = EntityWindow()
monitor = IdleMonitor()
director = Director(window, ...)

# ── Step 6: 关闭启动画面 ──
try:
    pyi_splash.close()
except NameError:
    pass

# ── Step 7: 启动事件循环 ──
monitor.start()
sys.exit(app.exec())

# ⏱ 总启动时间: < 500ms (不含 AI 模块)
# AI 模块 (cv2, mediapipe, llm) 在用户首次触发功能时才加载
```

#### 启动画面 (Splash Screen)

```python
# PyInstaller 打包时添加启动画面:
# 在 build.spec 的 EXE 块中添加:

exe = EXE(
    ...
    splash='assets/splash.png',          # 启动画面图片
    splash_timeout=0,                     # 0 = 手动关闭
)
```

### 5.2.2 内存占用优化

```python
class MemoryManager:
    """
    内存管理器
    
    职责:
    - 监控各子进程/线程的内存使用
    - 在空闲时主动释放资源
    - 实现"按需启动，用完即焚"的 AI 子进程策略
    
    策略:
    ┌────────────────────────────────────────────┐
    │ 组件          │ 内存占用  │ 生命周期        │
    ├────────────────────────────────────────────┤
    │ GUI 主进程    │ ~30MB    │ 永久运行        │
    │ IdleMonitor   │ ~2MB     │ 永久运行        │
    │ AudioManager  │ ~10MB    │ 永久运行        │
    │ GazeTracker   │ ~200MB   │ 角色可见时启动  │
    │ LLM 进程      │ ~4-8GB   │ 对话时启动      │
    │ OCR 引擎      │ ~300MB   │ 截屏分析时启动  │
    └────────────────────────────────────────────┘
    
    释放策略:
    - GazeTracker: 角色隐藏后立即释放摄像头和模型
    - LLM: 对话结束 5 分钟后 terminate() 进程
    - OCR: 一次性使用后释放
    """
    
    LLM_IDLE_TIMEOUT_SECONDS = 300  # 5 分钟
    
    def release_idle_resources(self):
        """
        释放空闲资源
        
        调用时机: 每分钟执行一次检查
        """
        # 检查 LLM 进程是否空闲超过 5 分钟
        if self._llm_process and self._llm_idle_seconds > self.LLM_IDLE_TIMEOUT_SECONDS:
            self._llm_process.terminate()
            self._llm_process = None
            # 释放 4-8GB 内存
            
        # 检查 GazeTracker 是否需要释放
        if self._gaze_tracker and self._entity_state == EntityState.HIDDEN:
            self._gaze_tracker.stop()
            self._gaze_tracker = None
            # 释放 ~200MB 内存
```

---

## 5.3 自动更新系统 (Update System)

### 架构: 引导器 + 核心包 (Launcher + Core)

```
用户桌面
├── CyberCompanion.exe         ← Launcher (入口, ~2MB, 很少更新)
│
└── /Core                      ← 业务包 (所有 Python 逻辑, ~180MB)
    ├── CyberCompanion-core.exe
    ├── _internal/
    │   ├── PySide6/
    │   ├── characters/
    │   └── ...
    └── version.json

远程服务器 (GitHub Releases / S3)
├── latest_version.json        ← 当前最新版本信息
└── CyberCompanion_v1.2.0.zip  ← 下载包
```

### 5.3.1 Launcher 逻辑

```python
# tools/launcher.py
# 编译为独立的 Launcher.exe（使用 PyInstaller 单文件模式打包）

import json
import os
import sys
import urllib.request
import zipfile
import shutil
from pathlib import Path


# ── 配置 ──
UPDATE_URL = "https://api.github.com/repos/YOUR_USERNAME/CyberCompanion/releases/latest"
CORE_DIR = Path(os.path.dirname(sys.executable)) / "Core"
TEMP_DIR = Path(os.environ.get('TEMP', '/tmp')) / "CyberCompanion_update"


def get_current_version() -> str:
    """读取本地版本号"""
    version_file = CORE_DIR / "version.json"
    if version_file.exists():
        with open(version_file, "r") as f:
            return json.load(f).get("version", "0.0.0")
    return "0.0.0"


def check_for_update() -> dict | None:
    """
    检查远程最新版本
    
    返回:
        更新信息 dict 或 None（无更新）
    
    GitHub Release API 返回格式:
    {
        "tag_name": "v1.2.0",
        "assets": [
            {
                "name": "CyberCompanion_v1.2.0.zip",
                "browser_download_url": "https://..."
            }
        ]
    }
    """
    try:
        req = urllib.request.Request(UPDATE_URL)
        req.add_header("Accept", "application/vnd.github.v3+json")
        req.add_header("User-Agent", "CyberCompanion-Launcher")
        
        with urllib.request.urlopen(req, timeout=10) as response:
            release = json.loads(response.read())
        
        remote_version = release["tag_name"].lstrip("v")
        local_version = get_current_version()
        
        if remote_version > local_version:
            # 寻找 zip 文件资产
            for asset in release.get("assets", []):
                if asset["name"].endswith(".zip"):
                    return {
                        "version": remote_version,
                        "download_url": asset["browser_download_url"],
                        "size": asset.get("size", 0),
                    }
    except Exception as e:
        print(f"[Launcher] 更新检查失败: {e}")
    
    return None


def download_and_apply_update(update_info: dict) -> bool:
    """
    下载并应用更新
    
    步骤:
    1. 下载 zip 到临时目录
    2. 验证完整性 (文件大小)
    3. 备份当前 Core 目录
    4. 解压覆盖 Core 目录
    5. 清理临时文件和旧备份
    
    返回:
        True: 更新成功
        False: 更新失败（保留旧版本）
    """
    try:
        # Step 1: 下载
        TEMP_DIR.mkdir(parents=True, exist_ok=True)
        zip_path = TEMP_DIR / "update.zip"
        
        print(f"[Launcher] 下载更新 v{update_info['version']}...")
        urllib.request.urlretrieve(update_info["download_url"], str(zip_path))
        
        # Step 2: 验证
        if update_info["size"] > 0:
            actual_size = zip_path.stat().st_size
            if actual_size != update_info["size"]:
                print(f"[Launcher] 文件大小不匹配: 期望 {update_info['size']}, 实际 {actual_size}")
                return False
        
        # Step 3: 备份
        backup_dir = CORE_DIR.parent / "Core_backup"
        if backup_dir.exists():
            shutil.rmtree(backup_dir)
        if CORE_DIR.exists():
            shutil.copytree(CORE_DIR, backup_dir)
        
        # Step 4: 解压覆盖
        with zipfile.ZipFile(zip_path, 'r') as z:
            z.extractall(CORE_DIR)
        
        # Step 5: 清理
        shutil.rmtree(TEMP_DIR, ignore_errors=True)
        shutil.rmtree(backup_dir, ignore_errors=True)
        
        print(f"[Launcher] 更新成功: v{update_info['version']}")
        return True
    
    except Exception as e:
        print(f"[Launcher] 更新失败: {e}")
        # 回滚
        if backup_dir.exists() and not CORE_DIR.exists():
            shutil.copytree(backup_dir, CORE_DIR)
        return False


def launch_core():
    """启动核心程序"""
    core_exe = CORE_DIR / "CyberCompanion-core.exe"
    if core_exe.exists():
        os.execv(str(core_exe), [str(core_exe)])
    else:
        print("[Launcher] 错误: 核心程序不存在")
        sys.exit(1)


def main():
    """
    Launcher 主流程:
    1. 检查更新 (超时 10 秒)
    2. 如果有更新 → 下载并应用
    3. 启动核心程序
    
    设计要点:
    - 更新检查失败不影响启动（静默跳过）
    - 更新下载失败不影响启动（使用旧版本）
    - 用户无感知（后台完成所有操作）
    """
    # 检查更新
    update = check_for_update()
    if update:
        download_and_apply_update(update)
    
    # 启动核心
    launch_core()


if __name__ == "__main__":
    main()
```

### 5.3.2 增量更新 (Delta Update) — 进阶

对于 200MB+ 的包，每次全量下载太慢。

```python
class DeltaUpdater:
    """
    增量更新器 (进阶功能)
    
    原理:
    - 使用 bsdiff/bspatch 进行二进制差分
    - 只下载变更的文件（通常只有几百 KB 的 .pyc 变化）
    
    远程存储结构:
    /releases
      /v1.0.0
        full.zip              ← 完整包
      /v1.1.0  
        full.zip              ← 完整包
        patch_from_v1.0.0.bsp ← 差分补丁 (通常 < 5MB)
    
    更新逻辑:
    1. 检查是否有从当前版本到最新版本的差分补丁
    2. 有 → 下载差分补丁并应用
    3. 没有 → 下载完整包（降级到全量更新）
    
    依赖: bsdiff4 (pip install bsdiff4)
    """
    pass
```

---

## 5.4 自动化构建流水线 (CI/CD Pipeline)

### GitHub Actions Workflow

```yaml
# .github/workflows/build.yml

name: Build & Release

on:
  push:
    tags:
      - 'v*'  # 当推送 tag (如 v1.0.0) 时触发

jobs:
  build-windows:
    runs-on: windows-latest
    
    steps:
      # ── Step 1: 检出代码 ──
      - name: Checkout
        uses: actions/checkout@v4
      
      # ── Step 2: 设置 Python ──
      - name: Setup Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'
          cache: 'pip'
      
      # ── Step 3: 创建干净的虚拟环境 ──
      - name: Create Virtual Environment
        run: |
          python -m venv build_env
          .\build_env\Scripts\Activate.ps1
          pip install --upgrade pip
      
      # ── Step 4: 安装依赖 ──
      - name: Install Dependencies
        run: |
          .\build_env\Scripts\Activate.ps1
          pip install -r requirements-build.txt
          pip install pyinstaller>=6.0.0
      
      # ── Step 5: 运行测试 ──
      - name: Run Tests
        run: |
          .\build_env\Scripts\Activate.ps1
          pip install pytest
          pytest tests/ -v --tb=short
      
      # ── Step 6: 打包 ──
      - name: Build with PyInstaller
        run: |
          .\build_env\Scripts\Activate.ps1
          pyinstaller build.spec
      
      # ── Step 7: 压缩 ──
      - name: Create Archive
        run: |
          $version = "${{ github.ref_name }}"
          Compress-Archive -Path "dist\CyberCompanion\*" -DestinationPath "CyberCompanion_${version}.zip"
      
      # ── Step 8: 创建 Release ──
      - name: Create GitHub Release
        uses: softprops/action-gh-release@v1
        with:
          files: CyberCompanion_*.zip
          draft: false
          prerelease: false
          generate_release_notes: true
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### 发布流程

```powershell
# 开发者执行：
git tag v1.0.0
git push origin v1.0.0
# → GitHub Actions 自动触发 → 打包 → 发布 → 用户自动更新
```

---

## 5.5 系统托盘集成 (System Tray)

```python
# src/ui/tray_icon.py

from PySide6.QtWidgets import QSystemTrayIcon, QMenu
from PySide6.QtGui import QIcon, QAction
from PySide6.QtCore import Signal


class SystemTrayManager:
    """
    系统托盘图标管理器
    
    功能:
    - 在 Windows 系统托盘显示图标
    - 右键菜单: 设置 / 切换角色 / 召唤 / 退出
    - 双击: 召唤/隐藏角色
    - 气泡通知
    """
    
    def __init__(self, app, icon_path: str):
        self._tray = QSystemTrayIcon(QIcon(icon_path), app)
        self._setup_menu()
        self._tray.show()
    
    def _setup_menu(self):
        """
        构建右键菜单:
        
        ┌────────────────┐
        │ 🎭 切换角色  ► │ → 子菜单: 角色列表
        │ ⚡ 立即召唤     │ → 无视空闲时间，立即出场
        │ ⚙️ 设置         │ → 打开设置窗口
        │ ─────────────  │
        │ 📊 状态         │ → 显示当前状态信息
        │ ─────────────  │
        │ ❌ 退出         │ → 完全退出程序
        └────────────────┘
        """
        menu = QMenu()
        
        # 切换角色
        char_menu = menu.addMenu("🎭 切换角色")
        # 动态填充角色列表...
        
        # 立即召唤
        summon_action = menu.addAction("⚡ 立即召唤")
        
        # 设置
        settings_action = menu.addAction("⚙️ 设置")
        
        menu.addSeparator()
        
        # 状态
        status_action = menu.addAction("📊 状态")
        
        menu.addSeparator()
        
        # 退出
        quit_action = menu.addAction("❌ 退出")
        quit_action.triggered.connect(QApplication.quit)
        
        self._tray.setContextMenu(menu)
```

---

## 5.6 日志系统 (Logging)

```python
# src/core/logger.py

import logging
from pathlib import Path
from logging.handlers import RotatingFileHandler


def setup_logger(log_dir: Path, debug: bool = False) -> logging.Logger:
    """
    配置日志系统
    
    特性:
    - 日志文件轮转: 每个文件最大 5MB, 保留最近 3 个文件
    - 日志路径: AppData/Local/CyberCompanion/logs/
    - Debug 模式: 同时输出到控制台
    
    日志格式:
    [2026-01-15 14:30:22] [INFO] [idle_monitor] 用户空闲触发: 185.3s
    [2026-01-15 14:30:22] [DEBUG] [director] 选择台词: late_night_01
    """
    logger = logging.getLogger("CyberCompanion")
    logger.setLevel(logging.DEBUG if debug else logging.INFO)
    
    # 文件处理器
    log_file = log_dir / "app.log"
    file_handler = RotatingFileHandler(
        log_file,
        maxBytes=5 * 1024 * 1024,  # 5MB
        backupCount=3,
        encoding="utf-8"
    )
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(logging.Formatter(
        "[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
    ))
    logger.addHandler(file_handler)
    
    # 控制台处理器 (仅 debug 模式)
    if debug:
        console_handler = logging.StreamHandler()
        console_handler.setLevel(logging.DEBUG)
        console_handler.setFormatter(logging.Formatter(
            "[%(levelname)s] %(message)s"
        ))
        logger.addHandler(console_handler)
    
    return logger
```

---

## 5.7 最终项目结构 (Final Project Structure)

```
/CyberCompanion-Project
│
├── /src                        # 源代码
│   ├── __init__.py
│   ├── main.py                 # 入口文件 (高 DPI 设置 + 启动流程)
│   │
│   ├── /core                   # 核心逻辑
│   │   ├── __init__.py
│   │   ├── idle_monitor.py     # Phase 1: Windows API 空闲检测
│   │   ├── director.py         # Phase 1: 导演控制器
│   │   ├── asset_manager.py    # Phase 1: 资源管理
│   │   ├── audio_manager.py    # Phase 3: TTS 语音合成 + 播放
│   │   ├── state_machine.py    # Phase 3: 有限状态机
│   │   ├── script_engine.py    # Phase 3: 台词选择引擎
│   │   ├── mood_system.py      # Phase 4: 心情系统
│   │   ├── character_loader.py # Phase 3: 角色包加载
│   │   ├── paths.py            # Phase 5: 路径工具
│   │   └── logger.py           # Phase 5: 日志系统
│   │
│   ├── /ui                     # 界面层
│   │   ├── __init__.py
│   │   ├── entity_window.py    # Phase 1: 透明无边框窗口
│   │   ├── ascii_renderer.py   # Phase 2: ASCII 字符画渲染引擎
│   │   └── tray_icon.py        # Phase 5: 系统托盘
│   │
│   └── /ai                     # 智能层
│       ├── __init__.py
│       ├── gaze_tracker.py     # Phase 4: MediaPipe 视线追踪
│       ├── presence_detector.py # Phase 4: 存在检测
│       ├── llm_provider.py     # Phase 4: LLM 抽象接口
│       └── screen_commentator.py # Phase 4: 屏幕内容吐槽
│
├── /characters                 # 角色资源包
│   └── /rem_maid               # 默认角色
│       ├── manifest.json
│       ├── config.json
│       ├── /assets
│       │   ├── /sprites        # idle.gif, peek.png, panic.gif, sleep.gif
│       │   └── /sounds         # 音效文件
│       ├── /scripts
│       │   └── dialogue.yaml   # 台词库
│       └── /voice_cache        # TTS 预缓存（自动生成）
│
├── /assets                     # 全局静态资源
│   ├── icon.ico                # 应用图标
│   ├── splash.png              # 启动画面
│   └── /models                 # AI 模型文件 (onnx 等)
│
├── /tools                      # 工程工具
│   ├── launcher.py             # 启动器源码
│   ├── runtime_hook.py         # PyInstaller 运行时钩子
│   └── build_hooks.py          # PyInstaller 自定义钩子
│
├── /tests                      # 测试
│   ├── test_idle_monitor.py
│   ├── test_ascii_renderer.py
│   ├── test_state_machine.py
│   ├── test_script_engine.py
│   └── test_audio_manager.py
│
├── /.github
│   └── /workflows
│       └── build.yml           # CI/CD 构建流水线
│
├── build.spec                  # PyInstaller 打包配置
├── requirements.txt            # 开发依赖
├── requirements-build.txt      # 打包依赖（精简版）
├── version.json                # 版本信息
├── config.json                 # 全局配置
├── LICENSE                     # 开源协议 (MIT)
├── README.md                   # 项目说明
└── .gitignore                  # Git 忽略规则
```

---

## 5.8 `.gitignore` 推荐配置

```gitignore
# Python
__pycache__/
*.py[cod]
*.pyo
*.egg-info/
dist/
build/
*.spec.bak

# 虚拟环境
.venv/
build_env/
venv/

# IDE
.vscode/
.idea/
*.swp

# 操作系统
Thumbs.db
.DS_Store

# 项目特定
characters/*/voice_cache/    # TTS 缓存（每个用户本地生成）
*.log
```

---

## 5.9 `version.json` 格式

```json
{
    "version": "1.0.0",
    "build_date": "2026-02-10",
    "python_version": "3.11.7",
    "commit_hash": "abc1234",
    "phase": "Phase 3-MVP"
}
```

---

## 🏁 全文档结语 (Conclusion)

至此，**"Project Cyber-Companion"** 的完整开发文档已经全部完成。

### 文档覆盖总览

| Phase | 主题 | 核心交付物 |
|-------|------|-----------|
| **Phase 1** | 核心架构与 MVP | IdleMonitor, EntityWindow, AsciiRenderer |
| **Phase 2** | 神经与肌肉实现 | Windows API 规范, 动画曲线, 坐标系, 数据 Schema |
| **Phase 3** | 声音与灵魂 | AudioManager, FSM 状态机, 脚本引擎, 角色包规范 |
| **Phase 4** | 视觉感知与 AI | GazeTracker, PresenceDetector, LLM Provider, MoodSystem |
| **Phase 5** | 工程化与发布 | PyInstaller, CI/CD, 自动更新, 日志系统 |

### 技术栈总览

```
底层系统钩子        → User32.dll (GetLastInputInfo, GetForegroundWindow)
高性能透明 GUI     → PySide6 + QPropertyAnimation + HTML Rendering
情感化语音合成      → edge-tts + Cache-First 策略 + AudioManager
行为引擎           → FSM (有限状态机) + 概率脚本引擎 + 心情系统
视觉感知           → MediaPipe Face Mesh + 存在检测融合
认知核心           → Ollama / OpenAI API + 屏幕 OCR + Prompt Engineering
工程化分发         → PyInstaller + GitHub Actions CI/CD + OTA 自动更新
```

### AI 开发指导说明

本文档专为 **Claude Code / Copilot / Cursor** 等 AI 编程助手设计：

1. **每个类都有完整的 docstring** — 包含职责说明、参数类型、返回值、使用示例
2. **每个步骤都有验收标准** — AI 可以据此判断任务是否完成
3. **代码骨架可直接使用** — AI 只需填充 `...` 占位符即可实现完整功能
4. **依赖版本锁定** — 避免兼容性问题
5. **错误处理已内置** — 每个模块都有降级策略（如 TTS 离线时静默跳过）
6. **文件路径明确** — 每段代码都标注了对应的文件路径

**推荐开发顺序**: Phase 1 Step 1 → Step 2 → Step 3 → Step 4 → Phase 2 → Phase 3 → Phase 4 → Phase 5