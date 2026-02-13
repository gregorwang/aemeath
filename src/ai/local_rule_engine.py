from __future__ import annotations

import re


class LocalRuleEngine:
    """Offline fallback text response rules."""

    @staticmethod
    def build_reply(screen_text: str, mood_value: float = 0.5) -> str:
        text = (screen_text or "").strip().lower()
        if not text:
            return "离线中，我先安静陪你。"

        if any(key in text for key in ("traceback", "exception", "error", "报错", "失败", "failed")):
            return "你这报错挺明显，先看第一条异常。"
        if any(key in text for key in ("github", "git", "pull request", "merge")):
            return "代码不少，先跑测试再提交。"
        if any(key in text for key in ("word", "ppt", "excel", "文档", "表格")):
            return "文档先搭提纲，效率会高很多。"
        if any(key in text for key in ("bilibili", "youtube", "douyin", "微博")):
            return "又在刷内容？别忘了正事。"
        if re.search(r"\b(todo|fixme|deadline)\b", text):
            return "先清最急那条任务，别分心。"

        if mood_value < 0.3:
            return "我离线了，但还在看着你。"
        if mood_value > 0.7:
            return "离线陪伴模式启动，我一直在。"
        return "网络不稳，先用本地模式陪你。"
