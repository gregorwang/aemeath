# Cyber Companion

Cyber Companion is a Windows desktop companion that appears around screen edges, reacts to idle/activity signals, and supports optional voice/vision/LLM features.

## Run (Development)

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python src/main.py
```

## Build (PyInstaller)

```powershell
python -m venv build_env
.\build_env\Scripts\Activate.ps1
pip install -r requirements-build.txt
pip install pyinstaller>=6.0.0
pyinstaller build.spec
```

## Launcher Update URL

- Default API endpoint: `https://api.github.com/repos/gregorwang/aemeath/releases/latest`
- Override by env var: `CYBERCOMPANION_UPDATE_URL`
- Or set `update_url` in `version.json`

## Project Highlights

- Idle detection via Windows API (`GetLastInputInfo`)
- Transparent ASCII-rendered window with staged animation
- Cache-first TTS playback with priority interrupt
- FSM-driven behavior orchestration
- Optional Phase 4 modules (camera gaze, presence detection, LLM commentary)
- Phase 5 engineering assets (packaging spec, runtime hook, launcher, CI workflow)
