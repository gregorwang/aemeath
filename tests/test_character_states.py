from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.character_states import CHARACTER_STATES, build_expression_state_map, get_gif_filename, get_state_label


class CharacterStatesTest(unittest.TestCase):
    def test_getters_return_expected_known_and_fallback_values(self) -> None:
        self.assertEqual(get_gif_filename(1), "state1.gif")
        self.assertEqual(get_state_label(8), "主角色")
        self.assertEqual(get_gif_filename(999), "state999.gif")
        self.assertEqual(get_state_label(999), "未知状态 999")

    def test_expression_map_is_built_from_state_metadata(self) -> None:
        mapping = build_expression_state_map()
        self.assertEqual(mapping["happy"], "state6")
        self.assertEqual(mapping["neutral"], "state1")
        self.assertEqual(mapping["angry"], "state4")
        self.assertEqual(mapping["sad"], "state5")
        self.assertTrue(all(label in CHARACTER_STATES for label in [1, 4, 5, 6]))


if __name__ == "__main__":
    unittest.main()
