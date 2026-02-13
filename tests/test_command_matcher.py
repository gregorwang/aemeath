from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.command_matcher import VoiceCommandMatcher


class VoiceCommandMatcherTest(unittest.TestCase):
    def test_exact_contains_match(self) -> None:
        match = VoiceCommandMatcher.match("帮我看看屏幕上是什么")
        self.assertIsNotNone(match)
        assert match is not None
        self.assertEqual(match.action, "screen_commentary")

    def test_fuzzy_match_for_similar_phrase(self) -> None:
        match = VoiceCommandMatcher.match("请你现身一下")
        self.assertIsNotNone(match)
        assert match is not None
        self.assertEqual(match.action, "summon")

    def test_unmatched_text_returns_none(self) -> None:
        self.assertIsNone(VoiceCommandMatcher.match("今天天气怎么样"))

    def test_normalize_text(self) -> None:
        self.assertEqual(VoiceCommandMatcher.normalize_text("  看，看 屏幕！ "), "看看屏幕")


if __name__ == "__main__":
    unittest.main()
