# PyInstaller build spec
# Usage: pyinstaller build.spec

from pathlib import Path
from PyInstaller.utils.hooks import (
    collect_data_files,
    collect_dynamic_libs,
)

PROJECT_ROOT = Path(SPECPATH)

mp_datas = collect_data_files("mediapipe")
mp_binaries = collect_dynamic_libs("mediapipe")
mp_hiddenimports = [
    "mediapipe",
]
cv2_datas = collect_data_files("cv2")
cv2_binaries = collect_dynamic_libs("cv2")
recorded_paths_dir = PROJECT_ROOT / "recorded_paths"
recorded_paths_datas = (
    [(str(recorded_paths_dir), "recorded_paths")]
    if recorded_paths_dir.exists()
    else []
)

a = Analysis(
    [str(PROJECT_ROOT / "src" / "main.py")],
    pathex=[str(PROJECT_ROOT / "src")],
    binaries=[
        *mp_binaries,
        *cv2_binaries,
    ],
    datas=[
        (str(PROJECT_ROOT / "characters"), "characters"),
        (str(PROJECT_ROOT / "assets"), "assets"),
        *recorded_paths_datas,
        (str(PROJECT_ROOT / "config.json"), "."),
        (str(PROJECT_ROOT / "version.json"), "."),
        *mp_datas,
        *cv2_datas,
    ],
    hiddenimports=[
        "PySide6.QtMultimedia",
        "PySide6.QtMultimediaWidgets",
        "edge_tts",
        "aiohttp",
        "cv2",
        "mediapipe",
        "matplotlib",
        "openai",
        "httpx",
        "websocket",
        "speech_recognition",
        "pyaudio",
        "rapidfuzz",
        "pycaw",
        "pycaw.pycaw",
        "comtypes",
        "comtypes.client",
        "comtypes.stream",
        "json",
        "yaml",
        "hashlib",
        "unittest",
        "unittest.mock",
        *mp_hiddenimports,
    ],
    excludes=[
        "tkinter",
        "_tkinter",
        "tk",
        "paddle",
        "paddleocr",
        "pandas",
        "sklearn",
        "scikit-learn",
        "IPython",
        "jupyter",
        "notebook",
        "ipykernel",
        "ipywidgets",
        "pytest",
        "doctest",
        "lib2to3",
        "pydoc_data",
        "ensurepip",
        "venv",
    ],
    runtime_hooks=[str(PROJECT_ROOT / "tools" / "runtime_hook.py")],
    hookspath=[],
    hooksconfig={},
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data)
splash = Splash(
    str(PROJECT_ROOT / "assets" / "splash.png"),
    binaries=a.binaries,
    datas=a.datas,
    text_pos=None,
    text_size=12,
    minify_script=True,
    always_on_top=True,
)

exe = EXE(
    pyz,
    a.scripts,
    splash,
    [],
    exclude_binaries=True,
    name="CyberCompanion-core",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    icon=str(PROJECT_ROOT / "assets" / "icon.ico"),
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    splash.binaries,
    strip=False,
    upx=True,
    upx_exclude=[
        "vcruntime140.dll",
        "python3*.dll",
        "Qt6*.dll",
    ],
    name="CyberCompanion",
)
