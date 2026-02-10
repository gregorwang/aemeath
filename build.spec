# PyInstaller build spec
# Usage: pyinstaller build.spec

from pathlib import Path

PROJECT_ROOT = Path(SPECPATH)

a = Analysis(
    [str(PROJECT_ROOT / "src" / "main.py")],
    pathex=[str(PROJECT_ROOT / "src")],
    binaries=[],
    datas=[
        (str(PROJECT_ROOT / "characters"), "characters"),
        (str(PROJECT_ROOT / "assets"), "assets"),
        (str(PROJECT_ROOT / "config.json"), "."),
        (str(PROJECT_ROOT / "version.json"), "."),
    ],
    hiddenimports=[
        "PySide6.QtMultimedia",
        "PySide6.QtMultimediaWidgets",
        "edge_tts",
        "aiohttp",
        "cv2",
        "mediapipe",
        "paddle",
        "mss",
        "paddleocr",
        "openai",
        "httpx",
        "json",
        "yaml",
        "hashlib",
    ],
    excludes=[
        "tkinter",
        "_tkinter",
        "tk",
        "matplotlib",
        "scipy",
        "pandas",
        "sklearn",
        "scikit-learn",
        "IPython",
        "jupyter",
        "notebook",
        "ipykernel",
        "ipywidgets",
        "pytest",
        "unittest",
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
