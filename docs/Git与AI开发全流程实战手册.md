# Git 与 AI 开发全流程实战手册

版本：2026-02-25
适用对象：主要通过 AI 进行代码开发、需求迭代、调试和发布的个人开发者或小团队

---

## 0. 先读这一页

如果你只记住三句话，就记这三句：

1. Git 分支的目的不是让文件不变，而是让主线历史不被污染。
2. AI 开发要小步提交，不要一口气让 AI 改 30 个文件后再看。
3. 你永远要有“可回到可运行版本”的路径，这条路径就是 `main + tag + reflog`。

---

## 1. 为什么这份手册要写得很长

你现在的痛点不是不会输命令。
你真正的痛点是：

- 需求大，改动快，决策频繁。
- AI 能一次改很多，但不保证一次就对。
- 你担心自己看不懂历史，怕把项目改坏。

短教程只会告诉你“怎么做”。
你需要的是“为什么这样做才稳”。

所以这份手册会覆盖：

- Git 基础心智模型
- AI 开发工作节奏
- 分支策略
- 提交策略
- 合并策略
- 回滚与救援
- 多目录并行开发
- 你这个仓库可直接复制的命令模板

---

## 2. Git 心智模型（不用背术语版）

把 Git 想成三个层：

1. 历史层：提交树
2. 准备层：暂存区
3. 文件层：工作区

你每天看到的是“文件层”。
但真正保护你的是“历史层”。

### 2.1 分支是什么

分支不是目录副本。
分支是一个可移动指针。

- `main` 指向主线最新提交。
- `feat/x` 指向某条功能线最新提交。

当你在 `feat/x` 提交时：

- `feat/x` 指针向前走。
- `main` 不会动。

这就是隔离。

### 2.2 为什么切分支时文件会变化

因为 Git 要把你的工作区“投影”成目标分支对应提交的样子。

所以你看到文件变了。
这很正常。

这不代表 main 被改了。
只代表你在看另一个分支版本。

### 2.3 “我在 main 上直接改也能回退”哪里有坑

能回退不等于回退成本低。

尤其 AI 开发时常见情况：

- 改动量大
- 提交密
- 多文件耦合
- 逻辑改动和调试改动混杂

在 main 上回退，容易伤到正确改动。
在分支上回退，最多丢掉这个分支。

---

## 3. AI 开发为什么更需要分支

传统手写开发：
改动节奏较慢，开发者通常知道每一行为什么改。

AI 开发：

- 你发一句需求，AI 会重排多个函数。
- 你说“再优化一下”，AI 可能把前一版结构推翻。
- 你说“顺手修一下”，AI 可能引入额外副作用。

所以 AI 开发最怕：

- 改动边界不清
- 历史可读性差
- 主线被实验代码污染

分支是你的缓冲区。
提交是你的里程碑。
标签是你的保险丝。

---

## 4. 你需要掌握的最小命令集合

不用一上来学完 Git。
先会这 16 个：

```powershell
git status
git branch --show-current
git switch main
git switch -c feat/xxx
git add -A
git commit -m "..."
git log --oneline --decorate --graph -20
git diff
git diff main...HEAD
git merge --no-ff feat/xxx
git branch -d feat/xxx
git stash push -u -m "..."
git stash pop
git tag backup/main-2026-02-25
git reflog --date=local
git revert <sha>
```

这套命令能覆盖 90% 日常。

---

## 5. 推荐分支模型（个人 AI 开发版）

你不需要复杂 Git Flow。
你用这个简化模型就够：

- `main`：永远保持可运行。
- `feat/*`：每个需求一个分支。
- `fix/*`：线上或主线问题修复。
- `spike/*`：纯实验分支，可随时丢弃。

### 5.1 命名规范建议

- `feat/idle-invasion-stage2`
- `fix/audio-monitor-startup`
- `spike/new-director-strategy`

命名要“看到就知道目的”。

### 5.2 为什么一个需求一个分支

因为这样你可以：

- 单独审查
- 单独回滚
- 单独合并

如果一个分支塞三个需求，后面都很痛苦。

---

## 6. AI 任务拆分策略（非常关键）

你给 AI 的任务粒度，直接决定 Git 历史是否可救。

### 6.1 错误方式

“把 idle invasion 相关逻辑都优化一下，顺便把 UI 和打包也修了。”

结果：

- 改动跨层
- 难验证
- 出问题不知谁导致

### 6.2 正确方式

把任务拆成“可提交单元”：

1. 只改 `src/ui/gif_particle.py` 的尺寸初始化路径。
2. 只改 `src/core/...` 的触发条件判定。
3. 只补对应测试。
4. 再做一次日志字段统一。

每个单元都能独立提交。
每个提交都有明确意图。

### 6.3 给 AI 的稳定提示模板

```text
你只改以下文件：
- src/ui/gif_particle.py

目标：
- 修复粒子尺寸在首帧未加载时可能为 0 的问题

约束：
- 不改动函数签名
- 不改动其他模块
- 保持现有日志格式

完成标准：
- 提供修改点说明
- 给出最小验证步骤
```

你会发现，AI 更可控，Git 历史也更干净。

---

## 7. 单次迭代标准流程（你每天照着跑）

### 7.1 开始前

```powershell
git switch main
git pull
```

可选但强烈建议：

```powershell
git tag backup/main-$(Get-Date -Format yyyy-MM-dd-HHmm)
```

### 7.2 开分支

```powershell
git switch -c feat/<topic>
```

### 7.3 做一个最小改动

让 AI 完成一个明确任务。
不要贪多。

### 7.4 立刻提交

```powershell
git add -A
git commit -m "Implement <small scoped change>"
```

### 7.5 验证

```powershell
pytest tests/ -v --tb=short
```

如果改动涉及 UI 或启动链路：

```powershell
python src/main.py
```

### 7.6 下一轮改动

重复 7.3 到 7.5。

### 7.7 合并前检查

```powershell
git diff --stat main...HEAD
git log --oneline main..HEAD
```

### 7.8 合并

```powershell
git switch main
git merge --no-ff feat/<topic>
```

### 7.9 清理

```powershell
git branch -d feat/<topic>
```

---

## 8. 提交信息怎么写，后面你才看得懂

提交信息建议使用“动词 + 对象 + 目的”：

- `Fix particle size init when first GIF frame is unavailable`
- `Add fallback path for idle invasion trigger logging`
- `Refactor director stage transition guard conditions`

避免这种：

- `update`
- `fix bug`
- `改一下`

### 8.1 一个提交只干一件事

如果一个提交同时包含：

- 功能改动
- 格式化
- 日志改名
- 重构

那它就是“不可维护提交”。

以后你自己也读不懂。

---

## 9. 合并策略：为什么建议 `--no-ff`

命令：

```powershell
git merge --no-ff feat/<topic>
```

好处：

- 主线历史会保留“这是一个完整需求分支”的节点。
- 以后回滚时可以按需求块回滚。
- 你做复盘时更容易看懂上下文。

个人项目也建议这样做。

---

## 10. 什么时候用 `revert`，什么时候用 `reset`

这是非常容易混淆的一对。

### 10.1 `revert`

- 生成一个“反向提交”。
- 不改写已有历史。
- 适合已经 push 的主线。

### 10.2 `reset`

- 把分支指针强行移回旧提交。
- 会改写历史。
- 适合本地未推送、自己可控的场景。

### 10.3 你的默认策略

- `main` 上：优先 `revert`。
- 功能分支本地：可用 `reset` 做整理。

---

## 11. 当你“不小心在 main 上开发了”

不要慌。
按这组命令抢救：

```powershell
# 还在 main 且已经提交了
git branch feat/salvage-<topic>
git switch main
# 如果尚未推送可用 reset；若已推送改用 revert
git reset --hard HEAD~1
git switch feat/salvage-<topic>
```

如果 main 已推送：

```powershell
git switch main
git revert <bad_sha>
```

然后继续在 `feat/salvage-<topic>` 迭代。

---

## 12. `stash` 正确用法（切分支前防丢）

场景：

- 你改到一半发现分支开错了。

做法：

```powershell
git stash push -u -m "wip before moving to feature branch"
git switch -c feat/<topic>
git stash pop
```

注意：

- `stash` 不是长期存档。
- 它是临时搬运工具。
- 长期保存请用 commit。

---

## 13. `reflog`：最后一道救命线

你几乎总能靠它找回“丢失的提交”。

```powershell
git reflog --date=local
```

找到目标 SHA 后：

```powershell
git switch -c rescue/<name> <sha>
```

建议养成习惯：

- 出事故先 `reflog`
- 别急着乱 reset

---

## 14. 对 AI 开发最有价值的高级功能：`worktree`

你会非常受益。

### 14.1 解决什么问题

你不想频繁切分支导致当前目录来回变。
你想一边保留 main 的稳定目录，一边让 AI 在另一个目录猛改。

### 14.2 怎么做

```powershell
# 在同级目录创建一个新工作区
git worktree add ..\aemeath-feat feat/<topic>
```

结果：

- `aemeath`：main 稳定线
- `aemeath-feat`：功能实验线

你可以同时打开两个编辑器窗口。
这对 AI 并行开发很实用。

### 14.3 收尾

分支合并后可清理：

```powershell
git worktree list
git worktree remove ..\aemeath-feat
```

---

## 15. 把“可运行”当成主线最高约束

对你这种桌面应用项目，主线“可运行”比“最新功能”更重要。

建议在 main 上坚持三条：

1. 能启动
2. 核心流程不报错
3. 测试至少不退化

如果某次需求做不到，宁可暂不合并。

---

## 16. 结合你仓库的验证门禁

你仓库已有明确规则。
以下是建议执行顺序：

### 16.1 单元测试

```powershell
pip install -r requirements-dev.txt
pytest tests/ -v --tb=short
```

### 16.2 应用启动冒烟

```powershell
python src/main.py
```

### 16.3 打包链路（涉及发布时）

```powershell
python -m venv build_env
.\build_env\Scripts\Activate.ps1
pip install -r requirements-build.txt
pip install "pyinstaller>=6.0.0"
pyinstaller --clean --noconfirm build.spec
```

### 16.4 打包后检查

- 启动 `dist/CyberCompanion/CyberCompanion-core.exe`
- 检查 `%LOCALAPPDATA%/CyberCompanion/logs/app.log`
- 无立即 fatal import/runtime 错误

涉及音频状态逻辑时，确认日志含 `AudioOutputMonitor` 启动信息。

---

## 17. AI 开发中的“提交频率”建议

这是很多人忽略的关键变量。

建议：

- 普通改动：每 10 到 30 分钟 1 个提交。
- 风险改动：每完成一个函数级目标就提交。
- 重大重构：先提交“纯重构无行为变化”，再提交“行为变化”。

这样回滚成本会非常低。

---

## 18. 如何让 AI 生成“可审查”的改动

你可以给 AI 增加这 4 个约束：

1. 仅修改白名单文件
2. 不跨模块重构
3. 每次只做一个目标
4. 输出改动理由与验证步骤

模板：

```text
任务目标：<一句话>
允许改动文件：<列表>
禁止事项：<列表>
验收标准：<列表>
```

有了这些约束，Git 历史会好很多。

---

## 19. 常见错误与纠偏

### 错误 1：AI 一次改太多

纠偏：拆任务，拆提交，分阶段验证。

### 错误 2：不看分支名就开改

纠偏：每轮改动前先跑：

```powershell
git branch --show-current
```

### 错误 3：合并前不看差异

纠偏：合并前固定执行：

```powershell
git diff --stat main...HEAD
git log --oneline main..HEAD
```

### 错误 4：把临时代码带到 main

纠偏：把 debug 打印、临时脚本放单独提交，合并前清理。

### 错误 5：把 `stash` 当备份

纠偏：重要内容立即 commit，不依赖 stash。

---

## 20. 合并前自检清单（可复制）

```text
[ ] 当前在 feature/fix 分支，不在 main
[ ] 每个提交单一职责，提交信息可读
[ ] 与需求无关文件未混入
[ ] 本地测试通过（至少无新增失败）
[ ] 启动冒烟通过（如改动涉及运行链路）
[ ] diff main...HEAD 已人工审查
[ ] 计划合并命令为 --no-ff
```

---

## 21. 发布前保护动作

在你准备把 main 用于发布前，建议：

```powershell
git switch main
git pull
git tag release-candidate-$(Get-Date -Format yyyyMMdd-HHmm)
```

如果之后出现问题，可快速定位发布点。

---

## 22. 与 GitHub 协作时的最小流程

即使你目前主要单人开发，也建议保持：

1. 本地 feature 分支开发
2. push 分支到远端
3. 走 PR 或至少自审 diff
4. 再并回 main

这样以后团队协作几乎无缝切换。

---

## 23. 你可以采用的两种节奏

### 节奏 A：快速试错（探索期）

- 分支：`spike/*`
- 允许改动快
- 提交可粗
- 不直接合 main

### 节奏 B：收敛上线（实现期）

- 分支：`feat/*` / `fix/*`
- 提交粒度严格
- 每轮验证
- 合并前审查

先用 A 探索，再把有效结果搬到 B。

---

## 24. 文档与代码同步原则

AI 开发特别容易“代码改了，文档没改”。

建议每个需求分支至少包含：

- 代码提交
- 测试提交（如有）
- 文档提交（若行为变化对使用者可见）

这样你的知识不会丢在聊天记录里。

---

## 25. 你这个项目的一份日常脚本化流程（建议）

```powershell
# 1) 拉齐主线
git switch main
git pull

# 2) 新需求分支
git switch -c feat/<topic>

# 3) AI 执行最小改动后提交
git add -A
git commit -m "Implement <small unit 1>"

# 4) 验证
pytest tests/ -v --tb=short

# 5) 下一单元
git add -A
git commit -m "Implement <small unit 2>"

# 6) 再验证
pytest tests/ -v --tb=short

# 7) 合并前审查
git diff --stat main...HEAD
git log --oneline main..HEAD

# 8) 合并
git switch main
git merge --no-ff feat/<topic>

# 9) 推送
git push origin main

# 10) 删除分支
git branch -d feat/<topic>
```

---

## 26. 常见问答（FAQ）

### Q1：切分支时文件变化，我会不会丢内容？

如果你有未提交且会冲突的改动，Git 会阻止切换。
它是在保护你。

### Q2：我只一个人开发，还需要分支吗？

需要。
因为你和 AI 已经是“高并发改动场景”。
分支是在给你自己做隔离。

### Q3：我可以永远只用 main 吗？

可以，但事故恢复成本会明显更高。

### Q4：我最少要做到什么程度才算安全？

- 需求分支开发
- 小步提交
- 合并前 diff 审查

做到这三条，风险就会大幅下降。

### Q5：push 到远端会不会把分支都弄乱？

不会。
远端也有分支隔离。
你 push `feat/x` 不会自动改 `main`。

---

## 27. 事故演练（建议你亲自做一次）

练一遍，之后就不慌：

1. 开 `feat/drill`
2. 故意提交一个错误改动
3. 再提交一个正确改动
4. 试一次 `revert`
5. 看主线如何保持干净

你做完一次，理解会从“知道”变成“会用”。

---

## 28. 你未来会感谢自己的 8 条纪律

1. 大改动前先看当前分支。
2. 每次只给 AI 一个清晰目标。
3. 每次改动后尽快提交。
4. 提交信息写清意图。
5. 合并前必须看 diff。
6. main 只接收验证过的改动。
7. 出事故先 reflog，不要慌乱 reset。
8. 发布前打 tag。

---

## 29. 一分钟速查卡

```text
要开始新需求：
main -> pull -> switch -c feat/x

要中途保存：
add -A -> commit

要看和主线差异：
diff main...HEAD

要安全合并：
switch main -> merge --no-ff feat/x

要撤销主线错误：
revert <sha>

要救回误删提交：
reflog -> switch -c rescue/x <sha>
```

---

## 30. 最后给你的结论

你之前的问题非常典型，也非常关键：

“分支会切换文件，那是不是没意义？”

标准答案是：

- 文件层会变化，这是正常行为。
- 历史层被隔离，这才是核心价值。
- 对 AI 高频改动开发，分支不是加分项，是基础设施。

当你把“分支 + 小步提交 + 合并前审查 + 可回滚路径”变成习惯后，
你会明显感觉到：

- 改需求不再心慌
- 让 AI 改代码更敢放开
- 项目稳定性显著提升

---

## 附录 A：命令字典（按目的分类）

### 查看类

```powershell
git status
git branch --show-current
git log --oneline --decorate --graph -20
git diff
git diff --stat main...HEAD
```

### 分支类

```powershell
git switch main
git switch -c feat/<topic>
git branch -d feat/<topic>
```

### 提交类

```powershell
git add -A
git commit -m "..."
```

### 合并类

```powershell
git merge --no-ff feat/<topic>
```

### 临时搬运类

```powershell
git stash push -u -m "..."
git stash pop
```

### 救援类

```powershell
git revert <sha>
git reflog --date=local
git switch -c rescue/<name> <sha>
```

### 标记类

```powershell
git tag backup/main-2026-02-25
git tag release-candidate-20260225-2300
```

---

## 附录 B：给 AI 的四种高质量请求模板

### 模板 1：最小修复

```text
目标：修复 <bug>
改动范围：<单文件或小范围>
禁止：不做重构，不改接口
验收：给出复现前后行为 + 测试步骤
```

### 模板 2：小型重构

```text
目标：重构 <模块> 以提升可读性
约束：不改变行为，不改外部接口
要求：先给重构计划，再分 2-3 次提交实施
```

### 模板 3：需求实现

```text
目标：实现 <需求>
分解：先数据层，再业务层，再 UI 层
每步要求：提交前说明影响范围和验证方法
```

### 模板 4：风险排查

```text
目标：定位 <异常>
要求：先给最可能的 3 个根因，再最小修复方案
约束：不做无关改动
```

---

## 附录 C：你可以直接执行的“今日开始”版本

```powershell
# A. 保底动作
git switch main
git pull
git tag backup/main-$(Get-Date -Format yyyy-MM-dd-HHmm)

# B. 新需求分支
git switch -c feat/<today-topic>

# C. 每轮 AI 改完就提交
git add -A
git commit -m "Implement <small change>"

# D. 验证
pytest tests/ -v --tb=short

# E. 合并与推送
git switch main
git merge --no-ff feat/<today-topic>
git push origin main
```

执行完这套，你就已经进入稳定的 AI 工程节奏了。
