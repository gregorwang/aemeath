# CyberCompanion 打包与 Release2 教学文档（新手深度版）

> 目标读者：你这种做过 Go / Remix / PG，但对桌面 Python 打包、事件系统、运行时一致性还在“表层理解”的开发者。  
> 文档目标：让你不仅会“照命令执行”，还知道“为什么必须这么做”，并能独立排障。

---

## 1. 先回答你最关心的两个问题

### 1.1 `release2` 到底怎么做

你可以把 “GitHub Release” 理解成三层：

1. Git 提交（commit）：代码快照。
2. Git 标签（tag）：给某个快照起一个稳定名字，比如 `release2`。
3. GitHub Release 页面：给这个标签补上说明、上传 `zip/exe` 等二进制产物。

最小发布流程：

1. 确保代码已推送：`git push origin main`
2. 打标签并推送：`git tag release2` + `git push origin release2`
3. 在 GitHub 仓库 `Releases -> Draft a new release` 里选择 tag=`release2`
4. 上传 `dist/CyberCompanion.zip`
5. 填写更新说明并发布

如果你本机安装了 GitHub CLI（`gh`），可以命令行一步到位；没有 `gh` 也完全能发，只是最后一步走网页。

### 1.2 为什么这个 exe 构建看起来这么麻烦

一句话：  
你不是在“编译一份 Python 代码”，而是在“把一个依赖大量动态组件的运行时系统，冻结成一套可移植的 Windows 应用目录”。

这件事天然复杂，原因包括：

1. Python 默认是解释执行，不是静态链接编译。
2. 你的项目依赖大量 C/C++ 扩展和系统 API（PySide6、OpenCV、Mediapipe、pycaw、comtypes）。
3. Qt/音频/图像/模型等资源不是纯 `.py`，还包括 DLL、插件、数据文件。
4. 动态导入、运行时反射、插件机制让“自动依赖分析”不总是准确。
5. 目标机器环境不可控（系统版本、音频设备、权限、杀软、VC 运行库等）。

所以复杂不是你“不会”，而是问题本身就属于“软件交付工程”。

---

## 2. 用你熟悉的概念做映射（Go/Remix/PG -> 本项目）

### 2.1 你在 Go 里熟悉的东西，在这里对应什么

1. Go `go build` -> Python `PyInstaller build.spec`
2. Go 静态链接（相对）-> Python 运行时打包（解释器 + 模块 + DLL + 资源）
3. Go goroutine/channel -> Qt 事件循环 + Signal/Slot + QTimer/线程
4. Go 服务部署 -> 桌面 app 分发（zip/exe + 本地配置 + 本地日志）

### 2.2 你在 Remix 里熟悉的东西，在这里对应什么

1. Loader/Action 数据流 -> `main.py` 装配 + `Director` 调度流
2. 组件通信（props/context）-> Qt Signal/Slot
3. Hydration 问题 -> 桌面端“资源初始化顺序 + 事件注册时机”
4. 客户端生命周期 -> `aboutToQuit` 资源清理、线程停止、日志落盘

### 2.3 你在 PG 里关心的一致性，在这里怎么落地

你这里没有数据库事务，但有“运行时一致性约束”：

1. 状态机必须单向收敛，不能乱跳。
2. 退出流程必须最终释放资源。
3. 音频检测失败要进入可退化模式，而不是崩溃。
4. 打包产物必须在新机器至少能启动并写日志。

这就是桌面系统里的“可用性一致性”。

---

## 3. 建立完整心智模型：从源码到 exe 到 release

### 3.1 八层模型（建议背下来）

1. 代码层：`src/**/*.py`
2. 依赖层：`requirements-build.txt`
3. 打包规则层：`build.spec`（hidden imports、datas、excludes、hook）
4. 冻结层：PyInstaller 解析依赖 -> 复制解释器/DLL/资源
5. 产物层：`dist/CyberCompanion/` + `CyberCompanion-core.exe`
6. 烟雾验证层：运行一次 + 查日志
7. 版本层：Git tag（例如 `release2`）
8. 分发层：GitHub Release 上传 zip/exe

任何一步没做好，用户拿到的“exe”都可能不可用。

### 3.2 为什么不能只跑 `pyinstaller src/main.py`

你的仓库已经明确规定：

1. 依赖真源是 `requirements-build.txt`，不是 `requirements.txt`
2. 必须走 `build.spec`，不能绕开规则

本质原因：

1. `build.spec` 承载了项目特有的隐式依赖与资源规则。
2. `src/main.py` 直接打只适合最简单脚本，不适合复杂桌面应用。

---

## 4. 针对这个仓库，为什么复杂度更高

### 4.1 技术栈复杂度叠加

你这里是“多系统耦合”：

1. GUI：PySide6（Qt 插件系统）
2. 音频：pycaw/comtypes/PyAudio（Windows COM + 设备状态）
3. 视觉：OpenCV + Mediapipe（大量原生依赖）
4. AI：HTTP/WebSocket/流式返回（网络不确定性）
5. 动画粒子与状态机：多模块联动

这些系统每个单独都不简单，叠加后打包复杂度指数级上升。

### 4.2 运行时的“动态性”让静态分析失效

PyInstaller 依赖静态扫描 `import`，但很多依赖在运行时决定：

1. 条件导入
2. 插件发现
3. 反射/字符串导入
4. Qt 后端选择

所以经常需要：

1. hidden imports
2. runtime hooks
3. 手动加 datas / binaries

这不是 Python 独有问题，Node 打包 Electron、Java 打 fat jar 也有类似痛点。

### 4.3 “在我机器可跑”不等于“可发布”

本机可跑只证明开发环境没问题。  
发布要证明：

1. 干净环境也能跑
2. 首次启动不崩
3. 日志可追踪
4. 关键能力能降级但不中断

这就是你看到“要 smoke-check、看日志”的原因。

---

## 5. 你现在最该掌握的 Python 基础语法（项目向）

### 5.1 导入与模块边界

核心语法：

```python
from src.core.config_manager import ConfigManager
from src.core.director import Director
```

你要理解的不是语法本身，而是“模块职责边界”：

1. `core` 放调度和状态
2. `ui` 放窗口与交互
3. `ai` 放模型能力适配

### 5.2 类与依赖注入

```python
class Director:
    def __init__(self, state_machine, window, audio_manager, config):
        self.state_machine = state_machine
        self.window = window
        self.audio_manager = audio_manager
        self.config = config
```

这就是最朴素的依赖注入。  
优点：

1. 测试时可替换 mock
2. 逻辑与实现解耦
3. 更容易定位 bug 来源

### 5.3 类型标注（你要把它当“文档+约束”）

```python
from typing import Optional

def resolve_config_path(path: Optional[str]) -> str:
    ...
```

类型不是为了炫技，是为了降低误用概率和阅读成本。

### 5.4 异常处理与降级策略

```python
try:
    start_audio_detector()
except Exception as exc:
    logger.warning("Audio detector degraded: %s", exc)
```

桌面应用非常需要“可退化运行”，否则用户设备差异会直接导致崩溃。

### 5.5 事件驱动（这个比语法更关键）

Qt 模型本质：

1. 事件循环驱动
2. 信号发射
3. 槽函数消费

你可以把它理解为“GUI 版消息队列系统”。

---

## 6. 代码业务逻辑主线（项目运行时）

### 6.1 启动主线

主入口 `src/main.py` 主要做四件事：

1. 读取配置并初始化日志
2. 创建 UI 与核心组件
3. 连接信号、热键、托盘与线程
4. 进入事件循环并处理退出清理

### 6.2 Director 的角色

`src/core/director.py` 是“业务编排中枢”：

1. 根据状态决定行为
2. 接收各模块事件并统一调度
3. 管控生命周期（启动/暂停/退出）

你可把它类比为后端系统里的 orchestrator。

### 6.3 配置系统为什么关键

`src/core/config_manager.py` 负责：

1. 默认值
2. 配置读取/写入
3. 兼容迁移

打包后用户环境变化大，配置系统决定了“能否稳定落地”。

---

## 7. 标准打包流程（你要形成肌肉记忆）

### 7.1 每次发布都重建打包环境

```powershell
python -m venv build_env --clear
.\build_env\Scripts\python -m pip install --upgrade pip
.\build_env\Scripts\pip install -r requirements-build.txt
.\build_env\Scripts\pip install "pyinstaller>=6.0.0"
```

理由：防止历史脏依赖污染本次构建。

### 7.2 打包前导入验证

```powershell
.\build_env\Scripts\python -c "import importlib.util as u; print('pycaw', bool(u.find_spec('pycaw'))); print('comtypes', bool(u.find_spec('comtypes'))); print('PySide6', bool(u.find_spec('PySide6')))"
```

如果这里失败，后面打包大概率也失败。

### 7.3 正式构建

```powershell
.\build_env\Scripts\pyinstaller --clean --noconfirm build.spec
```

关注：

1. 产物位置 `dist/CyberCompanion/`
2. 警告文件 `build/build/warn-build.txt`

### 7.4 烟雾验证（强制）

1. 启动 `dist/CyberCompanion/CyberCompanion-core.exe`
2. 检查 `%LOCALAPPDATA%/CyberCompanion/logs/app.log`
3. 确认没有启动即崩溃、没有关键致命错误

---

## 8. Release2 实战流程（命令版 + 网页版）

### 8.1 命令准备

假设你已经得到 `dist/CyberCompanion.zip`。

```powershell
git add -A
git commit -m "Update docs and release assets guidance"
git push origin main
```

### 8.2 创建 `release2` 标签

```powershell
git tag release2
git push origin release2
```

如果标签已存在，改用：

```powershell
git tag release2.1
git push origin release2.1
```

### 8.3 在 GitHub 网页创建 Release（无 `gh` 时推荐）

1. 打开仓库：`https://github.com/gregorwang/aemeath`
2. 进入 `Releases`
3. 点击 `Draft a new release`
4. 选择 tag：`release2`
5. Title：`release2`
6. 上传文件：`dist/CyberCompanion.zip`
7. 填写说明后点击 `Publish release`

### 8.4 如果你后续安装了 `gh`

```powershell
gh release create release2 dist/CyberCompanion.zip --title "release2" --notes "Build from build.spec; smoke-check passed."
```

---

## 9. 常见失败场景与排障策略

### 9.1 打包成功但运行时报 `ModuleNotFoundError`

原因：

1. 动态导入未被扫描
2. `build.spec` 少了 hidden import

处理：

1. 看 `warn-build.txt`
2. 在 `build.spec` 增加 hidden imports
3. 重新打包

### 9.2 启动正常但某功能失效（音频/摄像头/AI）

原因通常是环境差异：

1. 设备权限
2. 驱动/系统组件
3. 网络波动或第三方服务异常

处理：

1. 查 `app.log`
2. 确认是“可降级”而非“崩溃”
3. 优先补日志与错误提示，再考虑强依赖优化

### 9.3 打包过程超慢

正常现象，尤其是：

1. Qt + OpenCV + Mediapipe 体积大
2. 首次构建需要完整收集依赖
3. 杀软实时扫描会拖慢 IO

优化方向：

1. 固定稳定构建机
2. 减少非必要依赖
3. 缩小资源集

---

## 10. 从“会跑命令”到“会做工程”的学习路线

### 第 1 阶段（1 周）：看懂主流程

目标：

1. 能解释 `main.py -> director.py -> ui/ai/core` 调度路径
2. 能独立完成一次标准打包
3. 能读懂启动日志关键信息

任务：

1. 逐行读 `src/main.py`
2. 画出组件关系图（纸上即可）
3. 跑一遍完整 build + smoke-check

### 第 2 阶段（1 周）：会定位问题

目标：

1. 出问题先定位，不盲改
2. 会根据日志分类：配置问题 / 依赖问题 / 环境问题 / 业务逻辑问题

任务：

1. 人工制造一个 hidden import 问题并修复
2. 人工制造一个配置缺失问题并修复
3. 总结 10 条“日志 -> 根因”映射

### 第 3 阶段（1-2 周）：会做稳定发布

目标：

1. 能输出发布清单
2. 能独立做 tag + release
3. 能写最小可回滚说明

任务：

1. 做一次 `release2.x` 演练
2. 写 changelog 模板
3. 记录“失败回滚流程”

---

## 11. 给你的一套“新手不迷路”原则

1. 先保可运行，再谈优雅。
2. 先看日志，再改代码。
3. 先确认现象可复现，再判断根因。
4. 先做最小修复，再做结构优化。
5. 先建立稳定发布流程，再追求自动化。

这 5 条会比背语法更快提升你的工程能力。

---

## 12. 附：发布前检查清单（可直接复制）

### 12.1 代码与依赖

1. `requirements-build.txt` 已更新且正确
2. `build.spec` 与依赖配置同步
3. 关键模块导入检查通过

### 12.2 构建与验证

1. `build_env` 已重建
2. `pyinstaller --clean --noconfirm build.spec` 成功
3. `dist/CyberCompanion/CyberCompanion-core.exe` 可启动
4. `%LOCALAPPDATA%/CyberCompanion/logs/app.log` 无启动即崩溃

### 12.3 发布

1. commit + push 已完成
2. tag（例如 `release2`）已推送
3. GitHub Release 已上传 `CyberCompanion.zip`
4. Release note 包含变更与已知问题

---

## 13. 最后的理解升级：你已经不只是“写代码”

当你做 exe 发布时，你在做的是：

1. 运行时系统工程
2. 依赖与环境管理
3. 可观测性与故障恢复设计
4. 交付稳定性工程

这比“写一个功能”更接近真实工业开发。  
所以你现在遇到的“麻烦”，其实就是你工程能力升级的入口。

