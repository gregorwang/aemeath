# Cyber Companion (Aemeath)

一个 Windows 桌面赛博伴侣。  
它会在屏幕边缘出场、根据你的空闲/活动状态变化行为，并支持语音、屏幕解读、摄像头感知等可选 AI 能力。

![state6 demo](characters/state6.gif)
![aemeath demo](characters/aemeath.gif)

## ✨ 项目亮点

- 桌面角色系统：边缘出场、退场、拖拽、双击显示/隐藏、右键菜单交互
- 状态机驱动：`HIDDEN / PEEKING / ENGAGED / FLEEING` 全流程行为控制
- 空闲入侵（Idle Invasion）：长时间空闲时多 GIF 小人逐步入侵屏幕
- 屏幕解读：截图后交给多模态 LLM 生成中文短评并语音播报
- 语音能力：支持唤醒词或 `Ctrl+B` 单次按键转写
- 视觉能力（可选）：摄像头人脸/表情检测、在场状态推断
- 工程化完整：原生 `CTest`、Inno Setup 安装包、Release 更新检查、GitHub Actions

## 🎮 快速上手

面向第一次使用的用户（不是开发环境）。

1. 下载并运行 Release 安装包 `CyberCompanionCppSetup.exe`，或直接使用本地 staged 包 `out/package/windows-ninja-release/`。
2. 启动 `CyberCompanionCpp.exe`。
3. 看系统托盘图标，右键可用常见功能：
   - `立即召唤`
   - `你在看什么？`（手动触发一次屏幕解读）
   - `设置`
   - `使用指南`
4. 首次建议先在 `设置` 里确认：
   - AI：`provider/base_url/model/api_key`
   - 语音：麦克风模式（持续唤醒或按键转写）
   - 视觉：是否启用摄像头
   - 显示：是否启用`角色常驻模式（全屏时自动隐藏）`

常用交互：

- 双击角色：显示/隐藏切换
- 右键角色：打开快捷菜单
- `Ctrl+Shift+S`：立即召唤角色
- `Ctrl+B`：按键单次语音转写（push-to-talk 模式）

## 🧩 功能说明

### 屏幕解读
- 通过截图理解你当前窗口内容，再生成短文本并 TTS 播放。
- 截图链路使用本地抓屏，不依赖摄像头。

### 视觉感知（可选）
- 角色出场/互动时按需启用摄像头进行本地检测。
- 摄像头帧仅在内存中处理，不保存、不上传。

### 调试入口
- 托盘和角色右键提供：空闲入侵、悲伤安慰、无人脸提醒等调试按钮。
- 便于验证语音链路、状态链路和视觉反馈链路。

### 常驻模式（新）
- 在`设置 -> 基础`中开启`角色常驻模式（全屏时自动隐藏）`后，角色会尽量保持可见。
- 若检测到全屏应用（如游戏/演示），角色会自动隐藏；退出全屏后会自动恢复可见。
- 若开启`请勿打扰`，常驻策略会被临时抑制。

## 🔒 隐私与安全

- 摄像头默认关闭，且需要用户授权。
- 摄像头处理仅本地内存实时计算，不落盘。
- 远程请求仅在你配置 API Key 且启用相关功能后发生。
- 不要把真实密钥提交到 `config.json` / `version.json`。

## 🚀 开发运行（默认原生 C++）

```powershell
cmake --preset windows-ninja-debug
cmake --build --preset build-windows-ninja-debug
ctest --test-dir out/build/windows-ninja-debug --output-on-failure

.\out\build\windows-ninja-debug\CyberCompanionCpp.exe
```

## 🐍 Legacy Python 开发运行（仅兼容维护）

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python src/main.py
```

## 🧱 原生 C++ 运行时

仓库现已将 `Qt 6 + CMake` 原生运行时作为默认桌面入口，位于 `src_cpp/`。Python 入口保留为 legacy 兼容路径。

当前状态：

- 原生 `Qt 6 + CMake` 应用壳、单实例、日志、配置、自启动和更新检查
- 原生 tray、角色窗口、脚本登场、idle invasion、右键菜单和角色包切换
- 原生音频播放、`edge-tts`、语音输入、语音命令、屏幕评论与运行时状态摘要
- 原生摄像头 presence / gaze / periodic scan、全屏暂停、常驻模式、DND 与 idle 行为编排
- 原生设置页、CI 构建、`CTest`、`windeployqt`、smoke-check 与 Inno Setup 打包
- legacy `google/google_webspeech` ASR 配置会自动迁移到 `zhipu_asr`
- legacy 非 `edge` 的 TTS 配置会自动迁移到 `edge-tts`

当前推荐命令：

```powershell
cmake --preset windows-ninja-debug
cmake --build --preset build-windows-ninja-debug
ctest --test-dir out/build/windows-ninja-debug --output-on-failure

cmake --preset windows-ninja-release
cmake --build --preset build-windows-ninja-release
ctest --preset test-windows-ninja-release --output-on-failure

.\tools\cpp\deploy_cpp.ps1 -BuildDir out/build/windows-ninja-release -DeployMode Release -OutputDir out/package/windows-ninja-release
.\tools\cpp\smoke_check_cpp.ps1
```

参考：

- `CMakeLists.txt`
- `CMakePresets.json`
- `docs/cpp_phase0.md`
- `tools/cpp/build_cpp.ps1`
- `tools/cpp/deploy_cpp.ps1`
- `tools/cpp/smoke_check_cpp.ps1`

## ✅ 原生测试

```powershell
cmake --preset windows-ninja-release
cmake --build --preset build-windows-ninja-release
ctest --preset test-windows-ninja-release --output-on-failure
```

## ✅ Legacy Python 测试

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements-dev.txt
pytest tests/ -v --tb=short
```

## 📦 原生打包（默认发布链）

```powershell
.\tools\cpp\deploy_cpp.ps1 -BuildDir out/build/windows-ninja-release -DeployMode Release -OutputDir out/package/windows-ninja-release
.\tools\cpp\smoke_check_cpp.ps1
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "installer\cybercompanioncpp.iss"
```

GitHub Release 现在默认上传：

- `CyberCompanionCppSetup.exe`
- 原生 staged package zip
- `release_manifest.json`

## 📦 Legacy Python 打包（仅兼容维护）

```powershell
python -m venv build_env
.\build_env\Scripts\Activate.ps1
pip install -r requirements-build.txt
pip install "pyinstaller>=6.0.0"
pyinstaller --clean --noconfirm build.spec
```

打包注意事项：

- 打包依赖以 `requirements-build.txt` 为准（不要用 `requirements.txt`）。
- 统一使用 `build.spec`（不要用 `pyinstaller src/main.py`）。
- 发布前建议重建 `build_env`，避免旧依赖污染。

## 🔄 更新检查

- 默认更新源：`https://api.github.com/repos/gregorwang/aemeath/releases/latest`
- 可通过环境变量 `CYBERCOMPANION_UPDATE_URL` 覆盖
- 或在 `version.json` 设置 `update_url`

## 🔧 API 兼容说明

- 支持 OpenAI 兼容接口：`llm.base_url + llm.api_key`
- `base_url` 可写成 `https://host` 或 `https://host/v1`（程序会标准化）
- LLM Key 环境变量回退：`OPENAI_API_KEY -> POLOAI_API_KEY`
- TTS 使用 `edge-tts`

## 🗣 原生语音命令

- 纯 C++ 运行时现已覆盖主要 tray 用户入口：`设置`、`使用指南`、`编辑台词`、`复制最近日志`、`打开配置/数据/日志目录`、`反馈问题`、`关于`
- 纯 C++ 运行时现已支持 `检查更新`，会读取本地 `version.json/update_url` 并请求最新 release
- 角色切换支持 `characters/*/manifest.json` 的 `name` 与 `aliases`
- 角色窗口右键菜单已独立于 tray 总菜单，保留更贴近 Python 版的常用交互动作集合
- 原生设置页现已支持 `测试 API 连接`
- 原生配置现已覆盖 `behavior.auto_start_on_login` 和 `trigger.jitter_range_seconds`
- 原生配置现已按 Python 的 `audio / vision / wakeup / behavior` schema 落盘
- `appearance.ascii_width / font_size_px` 现已作用到原生角色窗口
- legacy `google` ASR 配置进入原生版时会自动迁移到 `zhipu_asr`

当前原生运行时说明见 [docs/cpp_runtime.md](docs/cpp_runtime.md)。
迁移历史保留在 [docs/cpp_phase0.md](docs/cpp_phase0.md)。
