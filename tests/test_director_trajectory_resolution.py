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
    def test_resolves_default_file_under_base_recorded_paths(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            default = root / "recorded_paths" / Director.VOICE_TRAJECTORY_FILE
            default.parent.mkdir(parents=True, exist_ok=True)
            default.write_text("{}", encoding="utf-8")

            subject = _TrajectoryResolutionSubject(root)
            with patch.dict(os.environ, {"CYBERCOMPANION_TRAJECTORY_PATH": ""}, clear=False):
                resolved = Director._resolve_voice_trajectory_path(subject)

            self.assertEqual(resolved, default)

    def test_env_file_has_higher_priority_than_default(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            default = root / "recorded_paths" / Director.VOICE_TRAJECTORY_FILE
            default.parent.mkdir(parents=True, exist_ok=True)
            default.write_text("{}", encoding="utf-8")

            env_file = root / "custom_env.json"
            env_file.write_text("{}", encoding="utf-8")

            subject = _TrajectoryResolutionSubject(root)
            with patch.dict(
                os.environ,
                {"CYBERCOMPANION_TRAJECTORY_PATH": str(env_file)},
                clear=False,
            ):
                resolved = Director._resolve_voice_trajectory_path(subject)

            self.assertEqual(resolved, env_file)

    def test_env_directory_resolves_default_filename(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            env_dir = root / "custom_dir"
            env_dir.mkdir(parents=True, exist_ok=True)
            env_file = env_dir / Director.VOICE_TRAJECTORY_FILE
            env_file.write_text("{}", encoding="utf-8")

            subject = _TrajectoryResolutionSubject(root)
            with patch.dict(os.environ, {"CYBERCOMPANION_TRAJECTORY_PATH": str(env_dir)}, clear=False):
                resolved = Director._resolve_voice_trajectory_path(subject)

            self.assertEqual(resolved, env_file)

    def test_returns_none_when_no_candidate_exists(self) -> None:
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            subject = _TrajectoryResolutionSubject(root)
            with patch.dict(os.environ, {"CYBERCOMPANION_TRAJECTORY_PATH": ""}, clear=False):
                resolved = Director._resolve_voice_trajectory_path(subject)

            self.assertIsNone(resolved)


if __name__ == "__main__":
    unittest.main()
