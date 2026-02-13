from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.asset_manager import Script
from core.director import Director


class _EntityWindowStub:
    def __init__(self) -> None:
        self.sprite_calls: list[str] = []
        self.ascii_calls: list[str] = []

    def set_sprite_content(self, sprite_path: str) -> None:
        self.sprite_calls.append(sprite_path)

    def set_ascii_content(self, html: str) -> None:
        self.ascii_calls.append(html)


class _DirectorVisualSubject:
    def __init__(self) -> None:
        self._entity_window = _EntityWindowStub()
        self._current_ascii_template = ""
        self.state_calls: list[tuple[str, bool]] = []
        self.resolve_ascii_calls = 0

    def _set_entity_state(self, state_name: str, *, as_base: bool = True) -> bool:
        self.state_calls.append((state_name, as_base))
        return True

    def _resolve_ascii_content(self, script: Script) -> str:
        self.resolve_ascii_calls += 1
        return f"ASCII:{script.text}"

    @staticmethod
    def _apply_current_gaze(ascii_template: str) -> str:
        return ascii_template


class DirectorVisualsTest(unittest.TestCase):
    def test_state_switch_does_not_skip_sprite_render(self) -> None:
        subject = _DirectorVisualSubject()
        script = Script(id="sprite", text="hello", sprite_path="characters/state1.gif")

        Director._set_visual_from_script(subject, script)

        self.assertEqual(subject.state_calls, [("state1", True)])
        self.assertEqual(subject._entity_window.sprite_calls, ["characters/state1.gif"])
        self.assertEqual(subject.resolve_ascii_calls, 0)
        self.assertEqual(subject._current_ascii_template, "")

    def test_state_switch_does_not_skip_ascii_render(self) -> None:
        subject = _DirectorVisualSubject()
        script = Script(id="ascii", text="fallback", sprite_path=None)

        Director._set_visual_from_script(subject, script)

        self.assertEqual(subject.state_calls, [("state1", True)])
        self.assertEqual(subject.resolve_ascii_calls, 1)
        self.assertEqual(subject._entity_window.ascii_calls, ["ASCII:fallback"])
        self.assertEqual(subject._current_ascii_template, "ASCII:fallback")


if __name__ == "__main__":
    unittest.main()
