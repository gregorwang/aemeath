from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

import launcher


class LauncherVersionTest(unittest.TestCase):
    def test_parse_version(self) -> None:
        self.assertGreater(launcher._parse_version("1.10.0"), launcher._parse_version("1.2.0"))
        self.assertEqual(launcher._parse_version("v1.2.3"), (1, 2, 3))
        self.assertEqual(launcher._parse_version("bad"), (0,))

    def test_update_url_resolution(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            core_dir = Path(tmp) / "Core"
            core_dir.mkdir(parents=True, exist_ok=True)
            version_file = core_dir / "version.json"
            version_file.write_text(
                json.dumps(
                    {
                        "version": "1.0.0",
                        "update_url": "https://api.github.com/repos/custom-owner/custom-repo/releases/latest",
                    }
                ),
                encoding="utf-8",
            )

            old_core_dir = launcher.CORE_DIR
            old_env_update = os.environ.get("CYBERCOMPANION_UPDATE_URL")
            old_env_repo = os.environ.get("CYBERCOMPANION_GITHUB_REPO")
            try:
                launcher.CORE_DIR = core_dir
                os.environ.pop("CYBERCOMPANION_UPDATE_URL", None)
                os.environ.pop("CYBERCOMPANION_GITHUB_REPO", None)

                self.assertEqual(
                    launcher.get_update_url(),
                    "https://api.github.com/repos/custom-owner/custom-repo/releases/latest",
                )

                os.environ["CYBERCOMPANION_UPDATE_URL"] = "https://example.com/latest.json"
                self.assertEqual(launcher.get_update_url(), "https://example.com/latest.json")

                os.environ.pop("CYBERCOMPANION_UPDATE_URL", None)
                version_file.write_text(json.dumps({"version": "1.0.0"}), encoding="utf-8")
                os.environ["CYBERCOMPANION_GITHUB_REPO"] = "foo/bar"
                self.assertEqual(
                    launcher.get_update_url(),
                    "https://api.github.com/repos/foo/bar/releases/latest",
                )
            finally:
                launcher.CORE_DIR = old_core_dir
                if old_env_update is None:
                    os.environ.pop("CYBERCOMPANION_UPDATE_URL", None)
                else:
                    os.environ["CYBERCOMPANION_UPDATE_URL"] = old_env_update
                if old_env_repo is None:
                    os.environ.pop("CYBERCOMPANION_GITHUB_REPO", None)
                else:
                    os.environ["CYBERCOMPANION_GITHUB_REPO"] = old_env_repo


if __name__ == "__main__":
    unittest.main()
