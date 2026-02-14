from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from core.director import Director


class _TrajectoryResolutionSubject:
    VOICE_TRAJECTORY_FILE = Director.VOICE_TRAJECTORY_FILE
    LOGGER = Director.LOGGER

    def __init__(self, base_dir: Path) -> None:
        self._base_dir = base_dir


class DirectorTrajectoryResolutionTest(unittest.TestCase):
    def test_auto_selects_latest_recorded_path(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            recorded = root / "recorded_paths"
            recorded.mkdir(parents=True, exist_ok=True)
            older = recorded / "trajectory_1000.json"
            newer = recorded / "trajectory_2000.json"
            older.write_text("{}", encoding="utf-8")
            newer.write_text("{}", encoding="utf-8")
            os.utime(older, (1_700_000_000, 1_700_000_000))
            os.utime(newer, (1_700_000_100, 1_700_000_100))

            subject = _TrajectoryResolutionSubject(root)
            with patch.dict(os.environ, {"CYBERCOMPANION_TRAJECTORY_PATH": ""}, clear=False):
                with patch("core.director.Path.cwd", return_value=root):
                    with patch("core.director.get_user_data_dir", return_value=root / "userdata"):
                        with patch("core.director.sys.executable", str(root / "python.exe")):
                            resolved = Director._resolve_voice_trajectory_path(subject)

            self.assertEqual(resolved, newer)

    def test_env_path_has_higher_priority_than_latest_scan(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            recorded = root / "recorded_paths"
            recorded.mkdir(parents=True, exist_ok=True)
            latest = recorded / "trajectory_9999.json"
            latest.write_text("{}", encoding="utf-8")
            os.utime(latest, (1_700_000_200, 1_700_000_200))

            env_file = root / "custom_env.json"
            env_file.write_text("{}", encoding="utf-8")

            subject = _TrajectoryResolutionSubject(root)
            with patch.dict(
                os.environ,
                {"CYBERCOMPANION_TRAJECTORY_PATH": str(env_file)},
                clear=False,
            ):
                with patch("core.director.Path.cwd", return_value=root):
                    with patch("core.director.get_user_data_dir", return_value=root / "userdata"):
                        with patch("core.director.sys.executable", str(root / "python.exe")):
                            resolved = Director._resolve_voice_trajectory_path(subject)

            self.assertEqual(resolved, env_file)

    def test_falls_back_to_default_filename_when_no_recorded_paths_exist(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            fallback = root / Director.VOICE_TRAJECTORY_FILE
            fallback.write_text("{}", encoding="utf-8")

            subject = _TrajectoryResolutionSubject(root)
            with patch.dict(os.environ, {"CYBERCOMPANION_TRAJECTORY_PATH": ""}, clear=False):
                with patch("core.director.Path.cwd", return_value=root):
                    with patch("core.director.get_user_data_dir", return_value=root / "userdata"):
                        with patch("core.director.sys.executable", str(root / "python.exe")):
                            resolved = Director._resolve_voice_trajectory_path(subject)

            self.assertEqual(resolved, fallback)


if __name__ == "__main__":
    unittest.main()
