# Repository Guidelines

## Project Structure & Module Organization
- `src/` contains application code.
- `src/core/` runtime logic (state machine, config, scheduling, audio, lifecycle).
- `src/ui/` PySide6 windows, tray integration, and rendering.
- `src/ai/` optional AI/vision/LLM features and adapters.
- `src/main.py` is the desktop entry point.
- `tests/` contains unit tests (`test_*.py`) using `unittest` conventions.
- `assets/`, `characters/`, and `config/` hold runtime resources and defaults.
- `.github/workflows/build.yml` defines CI build/test/release on Windows.

## Build, Test, and Development Commands
- Create dev env and run locally:
```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python src/main.py
```
- Run test suite (same runner style as CI):
```powershell
pytest tests/ -v --tb=short
```
- Build distributable with PyInstaller:
```powershell
python -m venv build_env
.\build_env\Scripts\Activate.ps1
pip install -r requirements-build.txt
pip install "pyinstaller>=6.0.0"
pyinstaller --clean --noconfirm build.spec
```

## Packaging Reliability Rules (Important)
- Build dependencies source of truth is **only** `requirements-build.txt`; do not use `requirements.txt` for packaging.
- Always build from `build.spec`; do not use `pyinstaller src/main.py`.
- Recreate `build_env` before release builds to avoid stale dependency contamination.
- Before packaging, verify critical modules are importable:
```powershell
.\build_env\Scripts\python -c "import importlib.util as u; print('pycaw', bool(u.find_spec('pycaw'))); print('comtypes', bool(u.find_spec('comtypes'))); print('PySide6', bool(u.find_spec('PySide6')))"
```
- Keep `requirements-build.txt` and `build.spec` synchronized when adding/removing runtime dependencies.
  - If a dependency is required at runtime in exe, update both packaging dependency list and hidden imports/excludes when needed.
- Post-build smoke-check is mandatory:
  - Launch `dist/CyberCompanion/CyberCompanion-core.exe` once.
  - Verify `%LOCALAPPDATA%/CyberCompanion/logs/app.log` contains startup logs and no immediate fatal import/runtime errors.
  - For audio state logic changes, confirm log shows `AudioOutputMonitor` startup.

## Coding Style & Naming Conventions
- Target Python 3.11+ and keep 4-space indentation.
- Use `snake_case` for functions/modules, `PascalCase` for classes, and `UPPER_SNAKE_CASE` for constants.
- Prefer explicit type hints (already common in `src/` and `tests/`).
- Keep modules focused by domain (`core`, `ui`, `ai`) instead of feature dumping into `main.py`.

## Testing Guidelines
- Add tests under `tests/` as `test_<feature>.py`.
- Follow the current pattern: `unittest.TestCase` classes with deterministic, isolated cases.
- Cover state transitions, fallback behavior, and config-driven branches for new logic.
- Run `pytest tests/ -v --tb=short` before opening a PR.

## Commit & Pull Request Guidelines
- Follow existing history style: short, imperative commit subjects (for example, `Implement ...`, `Merge ...`).
- Keep one logical change per commit; avoid mixing refactors with behavior changes.
- PRs should include:
  - What changed and why.
  - Test evidence (command + result).
  - Screenshots/GIFs for UI-visible changes (`src/ui`, character behavior, tray flows).
  - Packaging evidence when build chain is touched (build command + smoke-check result).

## Security & Configuration Tips
- Never commit real API keys or secrets in `config.json`/`version.json`.
- Prefer environment variables for credentials (for example `OPENAI_API_KEY`).
