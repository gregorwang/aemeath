from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from tools.trajectory_to_qt_timeline import convert_payload


class TrajectoryToQtTimelineTest(unittest.TestCase):
    def test_converts_points_payload_to_qt_schema(self) -> None:
        payload = {
            "total_duration": 1.0,
            "points": [
                {"x": 10, "y": 20, "t": 0.0, "s": 1},
                {"x": 20, "y": 30, "t": 0.5, "s": 2},
                {"x": 50, "y": 90, "t": 1.0, "s": 2},
            ],
        }

        converted = convert_payload(payload, source_file="demo.json", fps=10)

        self.assertEqual(converted["schema"], "qt.animation.timeline.v1")
        self.assertEqual(converted["source_file"], "demo.json")
        self.assertGreaterEqual(int(converted["duration_ms"]), 1000)
        self.assertTrue(isinstance(converted["keyframes"], list) and converted["keyframes"])
        self.assertEqual(converted["keyframes"][-1]["at"], 1.0)
        self.assertEqual(converted["state_events"][0]["state"], 1)
        self.assertEqual(converted["state_events"][-1]["state"], 2)

    def test_supports_keyframes_input(self) -> None:
        payload = {
            "duration_ms": 500,
            "keyframes": [
                {"time_ms": 0, "x": 0, "y": 0, "state": 1},
                {"time_ms": 250, "x": 100, "y": 50, "state": 1},
                {"time_ms": 500, "x": 200, "y": 100, "state": 3},
            ],
        }

        converted = convert_payload(payload, source_file="qt_source.json", fps=20)

        self.assertEqual(converted["source_file"], "qt_source.json")
        self.assertEqual(converted["fps_hint"], 20)
        self.assertGreaterEqual(int(converted["duration_ms"]), 500)
        states = {int(item["state"]) for item in converted["state_events"]}
        self.assertIn(1, states)
        self.assertIn(3, states)


if __name__ == "__main__":
    unittest.main()
