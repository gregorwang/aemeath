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

> 建议每次发布前重建 `build_env`，避免历史残留依赖污染打包结果。

## Launcher Update URL

- Default API endpoint: `https://api.github.com/repos/gregorwang/aemeath/releases/latest`
- Override by env var: `CYBERCOMPANION_UPDATE_URL`
- Or set `update_url` in `version.json`

## Project Highlights

- Idle detection via Windows API (`GetLastInputInfo`)
- Transparent ASCII-rendered window with staged animation
- Character interaction: drag, double-click toggle, right-click context menu
- System tray menu + settings panel (API key, TTS, camera/mic/wakeup toggles)
- Cache-first TTS playback with priority interrupt
- FSM-driven behavior orchestration
- Optional Phase 4 modules (camera gaze, presence detection, LLM commentary)
- Degradation strategy: AI/network failure -> local rule replies; camera/mic failure -> feature auto-disable
- Phase 5 engineering assets (packaging spec, runtime hook, launcher, CI workflow)

## API Compatibility Notes

- OpenAI-compatible chat endpoint is supported via `llm.base_url` + `llm.api_key`.
- `base_url` can be either `https://host` or `https://host/v1` (the app normalizes it).
- Environment fallback for LLM key: `OPENAI_API_KEY`, then `POLOAI_API_KEY`.
- TTS uses `edge-tts`.
