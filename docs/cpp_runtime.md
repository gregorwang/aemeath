# Native C++ Runtime

`CyberCompanionCpp` is the native `Qt 6 + CMake + MinGW` desktop runtime that now runs in parallel with the original Python entrypoint.

## Scope

- Native tray, single-instance guard, logging, config persistence, startup metadata, and auto-start registration
- Native character window, trajectory summon, idle invasion, presence/gaze behaviors, and runtime director orchestration
- Native voice input, screen commentary, update check, character pack switching, and settings dialog
- Native build/test/package pipeline with `CMake`, `CTest`, `windeployqt`, smoke-check, and Inno Setup

## Key Paths

- `src_cpp/app/`: application bootstrap and runtime composition
- `src_cpp/runtime/`: config, paths, logging, manifests, director, invasion, version/update helpers
- `src_cpp/ui/`: entity widget, settings dialog, tray controller, trajectory player
- `src_cpp/services/`: audio, voice input, commentary, fullscreen, idle, hotkey, vision, audio monitor
- `tests_cpp/`: native test suite
- `tools/cpp/`: build, deploy, doctor, and smoke-check scripts
- `installer/`: Inno Setup packaging

## Local Build

```powershell
cmake --preset windows-ninja-release
cmake --build --preset build-windows-ninja-release
ctest --test-dir out/build/windows-ninja-release --output-on-failure
```

## Local Packaging

```powershell
.\tools\cpp\deploy_cpp.ps1 -BuildDir out/build/windows-ninja-release -DeployMode Release -OutputDir out/package/windows-ninja-release
.\tools\cpp\smoke_check_cpp.ps1
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" "installer\cybercompanioncpp.iss"
```

## Current Runtime Guarantees

- Release build and native test suite are green locally.
- Staged package includes Qt runtime dependencies and `version.json`.
- Smoke-check validates startup logs and catches missing multimedia backends.
- Legacy `google/google_webspeech` ASR configs are auto-migrated to `zhipu_asr`.
- Legacy non-`edge` TTS configs are auto-migrated to `edge-tts`.

For the historical migration log, see `docs/cpp_phase0.md`.
