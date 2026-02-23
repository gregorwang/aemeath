# 全量修复执行报告（2026-02-22）

## 修复范围
基于 `docs/multi_agent_review_2026-02-22.md` 的问题清单，已完成对应代码修复与回归测试补齐。

## 已完成修复
1. 修复 idle invasion 路径未重置 idle monitor
- 文件: `src/core/director.py`
- 变更: `on_user_idle` 在 `idle_invasion.enabled=true` 时，补充 `reset_to_standby()` 与 `_arm_idle_threshold_with_jitter()`。
- 结果: 避免首轮触发后 monitor 停留导致后续 idle 事件失效。

2. 修复 `_pending_idle_script` 被跨周期复用
- 文件: `src/core/director.py`
- 变更: 在 `_enter_hidden` 中清理 `self._pending_idle_script`。
- 结果: 每个 hidden->engaged 周期重新选脚本，恢复时间段/冷却/随机策略。

3. 修复设置页无法清空 preamble 文本
- 文件: `src/ui/settings_dialog.py`
- 变更: `to_config` 中 `screen_commentary.preamble_text` 不再回退旧值，按输入原样保存（允许空字符串）。
- 结果: 用户可通过 UI 持久化“无过渡语”。

4. 修复 ASR model/base_url 字段无法真正清空
- 文件: `src/ui/settings_dialog.py`
- 变更: `to_config` 中 `audio.asr_model` 与 `audio.asr_base_url` 取消“空值回退旧值”。
- 结果: 切换 provider 后可清空并由运行时/加载逻辑走默认分支。

5. 修复 LLM 迁移函数幂等性并抽取 ASR 运行时解析 helper
- 文件: `src/main.py`
- 变更:
  - `_migrate_legacy_llm_defaults` 仅在 `""` 或 `"https://api.x.ai/v1"` 时改写 base_url，避免已规范值重复标记 changed。
  - 抽取顶层 `_resolve_asr_runtime(config)`，统一 PTT 与 continuous 路径调用。
- 结果: 迁移逻辑可测试且幂等，ASR 运行时参数解析更稳定。

## 新增/增强测试
1. `tests/test_director_idle_behavior.py`
- 新增 idle invasion 分支 monitor 重置/重臂断言。
- 新增 `_enter_hidden` 清理 `_pending_idle_script` 断言。

2. `tests/test_settings_dialog.py`（新增）
- 覆盖 preamble 可清空保存。
- 覆盖 ASR model/base_url 可清空保存。

3. `tests/test_main_runtime_migrations.py`（新增）
- 覆盖 `_migrate_legacy_llm_defaults` 正常迁移与幂等分支。
- 覆盖 `_resolve_asr_runtime(config)` 的 key/base_url 选择逻辑。

## 验证结果
执行命令：
- `python -m pytest tests/test_director_idle_behavior.py tests/test_settings_dialog.py tests/test_main_runtime_migrations.py -v --tb=short`
- `python -m pytest tests -k "director or config_manager_phase4 or main_runtime_migrations or settings_dialog" -v --tb=short`

结果：
- 相关测试通过（29 passed, 74 deselected）。
