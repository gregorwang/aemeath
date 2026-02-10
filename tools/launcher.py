from __future__ import annotations

import json
import os
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path


DEFAULT_GITHUB_REPO = "gregorwang/aemeath"
DEFAULT_UPDATE_URL = f"https://api.github.com/repos/{DEFAULT_GITHUB_REPO}/releases/latest"
CORE_DIR = Path(os.path.dirname(sys.executable)) / "Core"
TEMP_DIR = Path(os.environ.get("TEMP", "/tmp")) / "CyberCompanion_update"


def _parse_version(value: str) -> tuple[int, ...]:
    clean = value.strip().lstrip("v")
    parts = []
    for token in clean.split("."):
        try:
            parts.append(int(token))
        except ValueError:
            parts.append(0)
    return tuple(parts or [0])


def _read_local_version_payload() -> dict:
    version_file = CORE_DIR / "version.json"
    if not version_file.exists():
        return {}
    try:
        data = json.loads(version_file.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def get_update_url() -> str:
    env_update_url = os.environ.get("CYBERCOMPANION_UPDATE_URL", "").strip()
    if env_update_url:
        return env_update_url

    file_update_url = str(_read_local_version_payload().get("update_url", "")).strip()
    if file_update_url:
        return file_update_url

    env_repo = os.environ.get("CYBERCOMPANION_GITHUB_REPO", "").strip()
    if env_repo:
        return f"https://api.github.com/repos/{env_repo}/releases/latest"

    return DEFAULT_UPDATE_URL


def get_current_version() -> str:
    return str(_read_local_version_payload().get("version", "0.0.0"))


def check_for_update() -> dict | None:
    try:
        req = urllib.request.Request(get_update_url())
        req.add_header("Accept", "application/vnd.github.v3+json")
        req.add_header("User-Agent", "CyberCompanion-Launcher")
        with urllib.request.urlopen(req, timeout=10) as response:
            release = json.loads(response.read())

        remote_version = str(release.get("tag_name", "v0.0.0")).lstrip("v")
        local_version = get_current_version()
        if _parse_version(remote_version) <= _parse_version(local_version):
            return None

        for asset in release.get("assets", []):
            name = str(asset.get("name", ""))
            if name.endswith(".zip"):
                return {
                    "version": remote_version,
                    "download_url": str(asset.get("browser_download_url", "")),
                    "size": int(asset.get("size", 0) or 0),
                }
    except Exception as exc:
        print(f"[Launcher] 更新检查失败: {exc}")
    return None


def download_and_apply_update(update_info: dict) -> bool:
    backup_dir = CORE_DIR.parent / "Core_backup"
    try:
        TEMP_DIR.mkdir(parents=True, exist_ok=True)
        zip_path = TEMP_DIR / "update.zip"
        urllib.request.urlretrieve(update_info["download_url"], str(zip_path))

        if int(update_info.get("size", 0)) > 0:
            actual_size = zip_path.stat().st_size
            if actual_size != int(update_info["size"]):
                print("[Launcher] 文件大小不匹配")
                return False

        if backup_dir.exists():
            shutil.rmtree(backup_dir)
        if CORE_DIR.exists():
            shutil.copytree(CORE_DIR, backup_dir)

        if CORE_DIR.exists():
            shutil.rmtree(CORE_DIR)
        CORE_DIR.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(zip_path, "r") as archive:
            archive.extractall(CORE_DIR)

        shutil.rmtree(TEMP_DIR, ignore_errors=True)
        shutil.rmtree(backup_dir, ignore_errors=True)
        return True
    except Exception as exc:
        print(f"[Launcher] 更新失败: {exc}")
        if backup_dir.exists():
            if CORE_DIR.exists():
                shutil.rmtree(CORE_DIR, ignore_errors=True)
            shutil.copytree(backup_dir, CORE_DIR)
        return False


def launch_core() -> None:
    core_exe = CORE_DIR / "CyberCompanion-core.exe"
    if not core_exe.exists():
        print("[Launcher] 错误: 核心程序不存在")
        sys.exit(1)
    os.execv(str(core_exe), [str(core_exe)])


def main() -> None:
    update = check_for_update()
    if update:
        download_and_apply_update(update)
    launch_core()


if __name__ == "__main__":
    main()
