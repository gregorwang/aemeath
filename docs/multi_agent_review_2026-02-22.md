# 多子 Agent 并行代码审查综合报告（2026-02-22）

## 审查目标
针对当前工作区未提交改动进行并行 review，重点识别：
- 逻辑冲突与行为回归
- 状态机/时序问题
- UI 配置与持久化不一致
- 测试覆盖缺口

## 审查范围
本次改动文件：
- `src/core/config_manager.py`
- `src/core/director.py`
- `src/main.py`
- `src/ui/settings_dialog.py`
- `tests/test_director_idle_behavior.py`

## 审查方式
使用 3 个子 agent 并行审查并交叉汇总：
- Agent A：`config_manager.py` + `main.py`
- Agent B：`director.py` + `test_director_idle_behavior.py`
- Agent C：`settings_dialog.py` 及与 `main.py`/`config_manager.py` 的耦合

## 关键发现（按严重级别）

### High
1. Idle invasion 分支未重置 IdleMonitor，导致后续 idle 事件失效
- 位置：`src/core/director.py:270-276`
- 现象：`idle_invasion.enabled = true` 时，`on_user_idle` 提前返回，未执行 monitor 的 reset/re-arm。
- 风险：首次 `user_idle_confirmed` 触发后 monitor 可能停留在 `ACTIVE`，后续不再发出 idle 信号；闲置召唤/自动收回逻辑在运行一段时间后失效，通常需重启恢复。
- 触发条件：开启 idle invasion，经历一次 idle 周期后继续使用（尤其是切换回 legacy 路径时更明显）。

### Medium
1. `_pending_idle_script` 生命周期不完整，脚本可能被无限复用
- 位置：`src/core/director.py:497-503`
- 现象：首次选中的 script 被缓存后，在脚本结束或离开 `ENGAGED` 时未清理。
- 风险：后续每次 `_enter_engaged` 都重复同一个脚本，时间段筛选、权重随机和冷却策略失效。

2. `screen_commentary.preamble_text` 无法在 UI 中清空
- 位置：`src/ui/settings_dialog.py:683-690`
- 耦合路径：`src/main.py:505-518`，`src/core/config_manager.py:234-241`
- 现象：用户在设置页把 preamble 文本删空后，`to_config` 会回退到旧值并重新写回配置。
- 风险：用户无法通过 UI 持久化“无前导播报”；下次保存仍恢复旧文案，形成 UI 与用户预期不一致。

3. 多个 provider 相关文本字段无法真正清空，导致旧配置残留
- 位置：`src/ui/settings_dialog.py:614-641`，`src/ui/settings_dialog.py:655-659`
- 耦合路径：`src/main.py:305-319`，`src/core/config_manager.py:295-323`
- 现象：字段为空时回退 `self._source`，导致切换 ASR provider 后旧 model/base_url 仍被保存。
- 风险：运行时仍使用旧 endpoint/provider；UI 显示“已清空”，但实际配置未清空，需手改 JSON 才能恢复默认链路。

## 无直接缺陷发现
- 在 `src/core/config_manager.py` 与 `src/main.py` 的本轮改动中，未发现新增的直接功能性缺陷（但存在测试覆盖不足，见下）。

## 测试覆盖缺口
1. 缺少 `_migrate_legacy_llm_defaults` 的迁移行为测试
- 位置：`src/main.py:136-176`
- 建议：验证 legacy `xai` 配置向新默认值迁移、以及已规范配置不会被重复标记为变更。

2. 缺少 `_resolve_asr_runtime` 与 Voice Wakeup/Push-to-talk 的联动测试
- 位置：`src/main.py:314-318`（及调用链）
- 建议：覆盖“未显式 ASR base_url 时”的 fallback 与归一化路径。

3. 新增 `source` 参数的 commentary 请求链缺少来源分支测试
- 位置：`src/main.py`（hotkey / voice / tray / context menu 到 `director.request_screen_commentary`）
- 建议：分别验证来源字段传递与日志/通知行为一致性。

4. 缺少 `SettingsDialog.to_config` 的“可清空字段”单测
- 位置：`src/ui/settings_dialog.py`
- 建议：针对 preamble、ASR model/base_url、provider 切换后的空值保存行为建立回归测试。

## 修改冲突视角下的综合结论
- 当前最明显的冲突点集中在“UI 输入语义”与“配置持久化回退策略”的不一致：UI 允许清空，但持久化层回填旧值，最终影响 runtime 行为。
- `director` 的 idle 路径引入了状态机级别回归（High），优先级高于 UI 体验问题，建议先修复。

## 建议修复顺序
1. 修复 `on_user_idle` 在 idle invasion 路径的 monitor 重置/重臂逻辑（High）。
2. 修复 `_pending_idle_script` 的清理时机，避免脚本被永久缓存（Medium）。
3. 调整 `SettingsDialog.to_config` 的空值策略，区分“用户明确清空”与“保留旧值”（Medium）。
4. 补齐上述 4 类回归测试，再进行一次全量回归。
