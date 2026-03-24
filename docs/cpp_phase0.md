# C++ Phase 0 / 1 / 2 / 3 / 4 / 5 / 6 / 7 / 8 / 9 / 10 / 11 / 12 / 13 / 14 / 15 / 16 / 17 / 18 / 19 / 20 / 21 / 22 / 23 / 24 / 25 / 26 / 27 / 28 / 29 / 30 / 31 / 32 / 33 / 34 / 35 / 36 / 37 / 38 / 39 / 40 / 41 / 42 / 43 / 44 / 45 / 46 / 47 / 48 / 49 / 50

This repository now contains a parallel pure C++/Qt skeleton under `src_cpp/`.

Goals of Phase 0:

- keep the current Python app untouched
- add a compilable Qt 6 desktop executable
- establish app/runtime/ui boundaries for later migration
- provide a minimal runtime loop with:
  - single instance
  - tray icon
  - placeholder entity window
  - JSON config persistence
- log file initialization

Current target:

- executable: `CyberCompanionCpp`
- entrypoint: `src_cpp/main.cpp`

Phase 1 additions:

- bootstrap config from legacy `%LOCALAPPDATA%/CyberCompanion/config.json` when present
- persist first-run flag
- persist window visibility and last position
- add tray commands for config/data/log folders and reset-position
- add C++ config repository test executable

Phase 2 additions:

- add `RuntimeDirector` as native behavior orchestration entry point
- add `TrajectoryPlayer` for reusable QWidget trajectory playback
- add tray debug actions for summon/peek/flee/demo trajectory
- connect director status updates to entity window
- add native `RuntimeDirector` test coverage

Phase 3 additions:

- add native service layer skeletons for hotkeys, idle monitor, audio, and screen commentary
- wire hotkey and idle monitor services into `RuntimeDirector`
- wire director speech/commentary requests into stub services
- add native screen commentary stub test coverage

Phase 4 additions:

- replace audio stub with `QtMultimedia` playback shell
- replace commentary stub with `OpenAI-compatible` QtNetwork client shell
- add native foreground-window screen capture helper for commentary requests
- extend config schema with LLM provider/model/key/base_url
- add native commentary client parsing tests

Phase 5 additions:

- add reusable `OpenAI-compatible` client abstraction with centralized error classification
- upgrade `EntityWidget` to a stateful visual component with per-state styling
- route commentary requests through provider abstraction instead of service-local network code
- add failure-message parsing tests for commentary network errors

Phase 6 additions:

- add native `CharacterAssetCatalog` for default GIF resolution
- upgrade `EntityWidget` to render real `QMovie` GIF assets per runtime state
- wire role visuals closer to existing Python `state1..state6.gif` conventions
- add native asset catalog test coverage

Phase 7 additions:

- add `BehaviorMode` to `RuntimeDirector` so entity state and commentary session state are no longer collapsed into one enum
- serialize screen commentary requests to block duplicate triggers while a summon/request is already in flight
- stage commentary from `Hidden -> Peeking -> Engaged -> request` instead of firing the network request immediately
- ignore late commentary callbacks after manual cancel/flee to avoid stale speech playback
- extend native `RuntimeDirector` tests to cover staged commentary flow and duplicate-request suppression

Phase 8 additions:

- upgrade `EntityWidget` from coarse state motion to explicit `peek / enter / summon / flee / hideNow` actions
- add summon sequence reuse (`peek -> pause -> enter`) inside the widget instead of overloading one generic motion method
- add screen clamping helper for full visibility and partial visibility cases
- clamp drag movement to the current screen and add a drag threshold to avoid accidental drags from small pointer jitter
- replace widget-local double-click visibility toggle with a `doubleClicked` signal so the application shell owns the interaction policy
- extend native UI tests to cover screen clamping behavior

Phase 9 additions:

- add layered sprite-state composition to `EntityWidget` so rendered GIFs are decided from movement / click override / hover / probe / base state instead of one coarse enum
- add alias-aware `CharacterAssetCatalog` resolution (`state1..state6`, `hover`, `roaming`, `engaged`, `commentary`, etc.)
- add widget-owned autonomous roam/probe timers and animations so idle movement no longer belongs in `RuntimeDirector`
- add `applyGazeFollow(...)` to the native widget with deadzone/smoothing logic
- wire `RuntimeDirector::BehaviorMode` into widget base state and autonomy toggles from the application shell
- extend asset tests to cover alias normalization

Phase 10 additions:

- wire `EntityWidget::contextMenuRequested` into the tray menu so widget right-click now opens a real native command surface
- add a dedicated native widget test target instead of overloading the existing UI position tests
- add widget tests for canonical state rendering, transient click override, context-menu signal emission, and double-click emission
- keep the widget interaction contract aligned with the Python app shell by routing visibility decisions back through `ApplicationBootstrap`

Phase 11 additions:

- upgrade `TrajectoryPlayer` to load Qt timeline JSON from `recorded_paths/*.json` instead of only supporting hard-coded offset demos
- add timeline state cues so recorded trajectory `state` changes can drive the native widget sprite state during playback
- make `RuntimeDirector::summonNow()` prefer scripted trajectory summon when a recorded file is available, with automatic fallback to the normal summon path
- keep recorded end positions intact by skipping the next default engaged transition after scripted playback completes
- extend director tests to cover scripted summon path selection via `CYBERCOMPANION_TRAJECTORY_PATH`

Phase 12 additions:

- add `TrajectoryPlayer::timelineAborted` and explicit timeline-state cleanup so interrupted scripted playback no longer leaves stale internal state behind
- add a scripted trajectory watchdog in `ApplicationBootstrap` to force recovery when playback does not naturally complete
- stop the watchdog on manual hide/flee, abort, and normal completion so scripted summon now has explicit cancellation and timeout paths
- add a dedicated native `TrajectoryPlayer` test target for timeline finish/abort behavior
- extend director tests to cover canceling scripted summon without a late `Engaged` bounce-back

Phase 13 additions:

- add config-backed `behavior.scripted_trajectory_path` so scripted summon no longer depends only on an environment variable or one hard-coded file
- upgrade `RuntimeDirector` scripted trajectory resolution order to `CYBERCOMPANION_TRAJECTORY_PATH -> config path -> recorded_paths directory scan -> legacy default file`
- prefer the newest `*_qt_animation.json` file when a trajectory directory is configured, with fallback to generic `trajectory_*.json`
- pass the configured scripted trajectory path from `ApplicationBootstrap` into `RuntimeDirector`
- extend native config and director tests to cover persisted trajectory-path configuration and directory-based trajectory selection

Phase 14 additions:

- extract scripted trajectory resolution into a dedicated `ScriptedTrajectoryCatalog` instead of keeping directory/file heuristics inside `RuntimeDirector`
- validate candidate trajectory files before selection so malformed JSON or files without usable `keyframes/points` are skipped during scanning
- keep Python-compatible preferred-file behavior for `trajectory_1771029879_qt_animation.json` while still supporting directory fallback to the newest usable trajectory file
- add a dedicated native `ScriptedTrajectoryCatalog` test target to lock file-vs-directory resolution and invalid-file fallback behavior
- extend `TrajectoryPlayer` points-schema compatibility to respect recorded `duration_ms / total_duration`, matching the Python player more closely for raw recorder outputs

Phase 15 additions:

- add a dedicated native scripted-trajectory debug trigger in `RuntimeDirector` instead of overloading the normal summon path
- align skip conditions with the Python app more closely by rejecting the debug trigger while scripted playback is already active or the entity is fleeing
- add a matching tray action for `调试轨迹登场` so the C++ shell now exposes the same debug affordance as the Python tray/context menu
- extend native director tests to cover engaged-state debug start and fleeing-state debug rejection

Phase 16 additions:

- align scripted entrance completion with the Python `Director`: trajectory playback now resolves back to `Hidden` instead of landing in `Engaged`
- remove the C++-only “skip next engaged animation” workaround from `ApplicationBootstrap`, since scripted playback no longer hands off into the normal engaged-state animation path
- extend native director tests so both normal scripted summon and debug-triggered scripted summon assert the final `Hidden` state on completion

Phase 17 additions:

- add `BehaviorMode::Summoning` to the native `RuntimeDirector` so scripted entrance no longer reuses the generic busy mode
- map `Summoning` to the widget’s `state6` visual in `ApplicationBootstrap`, matching the Python director’s `BehaviorMode.SUMMONING -> state6`
- extend director tests to assert the behavior-mode transition into `Summoning` during scripted entrance and back to `Busy` after completion

Phase 18 additions:

- stop active trajectory playback before starting screen commentary so commentary and scripted timeline no longer fight over the same native widget
- extend director tests to cover the case where commentary takes over after scripted summon starts and ensure a late scripted-completion callback is ignored

Phase 19 additions:

- align native `BehaviorMode::Busy` visual mapping with the Python director by switching the base widget state from `state1` to `state4`
- keep `Idle -> state1`, `Summoning -> state6`, and `Commentary -> state2`, so the full behavior-mode-to-sprite mapping now matches the Python runtime much more closely

Phase 20 additions:

- make hidden-state screen commentary prefer scripted summon when a usable trajectory file is available, instead of always falling back to the simple peeking path
- after a scripted summon launched for commentary completes, hand off directly into the commentary request flow instead of hiding first
- extend native director tests to cover `requestScreenCommentary()` using scripted trajectory + commentary handoff from `Hidden`

Phase 21 additions:

- add `BehaviorMode::MediaPlaying` to the native `RuntimeDirector` so the behavior layer now has a first-class contract for external media playback
- map `MediaPlaying` to the widget’s `state3` visual in `ApplicationBootstrap`, matching the Python director’s `BehaviorMode.MEDIA_PLAYING -> state3`
- add native director tests covering `onAudioOutputStarted()` -> `MediaPlaying` and `onAudioOutputStopped()` -> `Idle` recovery

Phase 22 additions:

- add a dedicated `AudioOutputMonitorService` contract to the native service layer instead of leaving media-playback behavior disconnected from the application composition root
- add a `NullAudioOutputMonitorService` stub and wire it into `ApplicationBootstrap` lifecycle/startup/shutdown so the final Windows implementation has a stable place to slot in

Phase 23 additions:

- align native `RuntimeDirector` audio-output behavior with the Python contract more closely: audio start now records `MediaPlaying` even from `Hidden`, while commentary/scripted entrance still suppress reactive transitions
- restore `Busy` after media playback stops from `Hidden`, and restore `Idle` after playback stops from visible states
- extend native director tests to lock hidden-state media start/stop behavior and commentary suppression

Phase 24 additions:

- replace the `NullAudioOutputMonitorService` stub with a native polling shell that preserves the Python monitor’s public contract: `start()/stop()`, `isPlaying()`, `audioOutputStarted`, `audioOutputStopped`, and `audioStateChanged(bool)`
- add `500ms`-style polling configuration, single-inflight polling, `5`-sample silence debounce, current-process filtering, media-session preference, and optional master-peak fallback options to the native service layer
- add a Windows WASAPI backend shell for session-meter polling plus a fake-backend injection path so the monitor behavior can be tested without real system audio
- add a dedicated native `AudioOutputMonitorService` test target covering stop-edge emission, silence debounce, non-media ignore behavior, and unavailable-backend soft failure

Phase 25 additions:

- add `AudioService::playbackStarted/playbackFinished` so the native app now has an explicit self-playback lifecycle instead of treating speech playback as a fire-and-forget side effect
- align `RuntimeDirector` with the Python `_self_playback_active` contract: self playback suppresses `MediaPlaying` visual mode while speech is active, and restores media mode immediately after speech ends if external audio is still active
- wire audio-service lifecycle signals into `ApplicationBootstrap` and add native tests for both director recovery logic and fallback audio-service signal emission

Phase 26 additions:

- add `AudioService::interrupt()` to the native service contract so the application shell can explicitly stop current speech and clear queued playback
- interrupt the audio service automatically when the entity enters `Fleeing` or `Hidden`, keeping native dismiss/hide behavior aligned with the Python director’s speech cutoff policy
- extend native audio-service tests to lock fallback interrupt behavior

Phase 27 additions:

- extend `AppConfig` and `ConfigRepository` with `audio_output_reactive` plus native monitor options for poll interval, current-process filtering, media-session preference, and master-peak fallback
- pass those config values into `RuntimeDirector` and `WindowsAudioOutputMonitorService`, so media reactivity is no longer hard-coded in the native composition root
- extend native config and director tests to lock config round-trip / legacy bootstrap and disabled-reactivity behavior

Phase 28 additions:

- add a native `SettingsDialog` for the most important migrated configuration fields instead of forcing all C++ settings edits through manual JSON editing
- wire a new tray `设置` action into the application shell and apply edited values back into `RuntimeDirector`, `WindowsAudioOutputMonitorService`, and `OpenAiCommentaryService` at runtime
- add a dedicated native settings-dialog test target to lock widget loading and accepted-config export behavior

Phase 29 additions:

- upgrade `QtAudioService` from a fixed notification-sound shell into a cached speech service that resolves a `tts-cache` directory under user data
- add `edge-tts` process integration as the first native-side synthesis bridge, with cache hit reuse and fallback to the notification sound or log-only completion when synthesis is unavailable
- pass the TTS cache directory from `ApplicationBootstrap` via `AppPaths::ttsCacheDir()` so the audio layer no longer depends on ad hoc paths

Phase 30 additions:

- add structured `AudioPlaybackRequest` support to the native audio layer, including priority and interrupt flags instead of treating every spoken line as the same class of work
- upgrade `QtAudioService` from FIFO text queuing to a priority-aware queue with low-priority drop behavior and critical-request interrupt handling, bringing it closer to the Python `AudioManager` contract
- route native commentary speech through structured audio requests so failure responses can be emitted with higher priority than normal commentary narration

Phase 31 additions:

- align native `VoiceScriptCatalog` with the existing `characters/default/scripts.json` structure instead of treating idle/panic lines as plain text-only defaults
- add support for script metadata such as `audio_cache/audio_path`, `sprite`, `anim_speed`, and `tags`, with relative-path resolution from the script file location
- match Python-style idle selection more closely with exact-time-range preference, end-exclusive `HH:MM-HH:MM` matching, cooldown filtering, weighted probability, and immediate-repeat avoidance
- propagate script-provided cached audio paths through `AudioPlaybackRequest`, so native speech playback can reuse character-pack audio before falling back to TTS
- remove the dead `speakRequested(QString)` signal and keep the native speech pipeline on the single structured `speechRequestRequested(AudioPlaybackRequest)` contract

Phase 32 additions:

- add `voiceVisualRequested(spritePath, animSpeed)` to `RuntimeDirector` so selected idle/panic scripts can drive native widget visuals in addition to audio
- upgrade `EntityWidget` with script visual override support, allowing `scripts.json` `sprite` assets to temporarily replace the usual state-based GIF mapping
- map script `anim_speed` into native `QMovie::setSpeed(...)` presets so slow/fast script lines can change playback feel without replacing the main state machine
- clear script visual override on behavior-mode transitions and hidden-state teardown so commentary / media / dismiss flows do not get stuck on stale script GIFs
- extend native director and widget tests to cover sprite metadata propagation and override reset behavior

Phase 33 additions:

- add explicit `cancel()` to the native `ScreenCommentaryService` contract instead of only ignoring late callbacks in `RuntimeDirector`
- teach `OpenAiCompatibleClient` to abort an in-flight `QNetworkReply` before starting a new request, and suppress cancellation errors from surfacing as normal failures
- propagate commentary cancellation through `OpenAiCommentaryService`, application shutdown, commentary-service rebuild, and `Hidden/Fleeing` transitions
- reduce wasted network work and tighten parity with the Python runtime’s “retreat/hide stops the active commentary flow” expectation

Phase 34 additions:

- add `tts_voice` and `tts_rate` to the native `AppConfig`, `ConfigRepository`, and `SettingsDialog`, matching the Python runtime’s configurable speech voice/rate model more closely
- extend the `AudioService` contract with runtime voice configuration so `ApplicationBootstrap` can apply new TTS settings without rebuilding unrelated services
- upgrade `QtAudioService` to synthesize with configured voice/rate instead of hard-coded `zh-CN-XiaoxiaoNeural/+0%`
- include voice/rate in the native TTS cache key, avoiding cache collisions when the same text is spoken with different synthesis settings

Phase 35 additions:

- add configurable commentary prompts (`system`, `user`, `no-image`) plus `max_tokens` and `temperature` to the native `AppConfig`, `ConfigRepository`, and `SettingsDialog`
- propagate those settings through `ApplicationBootstrap` into `OpenAiCompatibleConfig`, so the native commentary request is no longer hard-coded around one fixed instruction set
- make `OpenAiCommentaryService::buildChatPayload(...)` honor configured prompts and limits, with safe defaults when fields are blank
- extend native config/settings/commentary tests to cover prompt round-trip and payload generation

Phase 36 additions:

- add provider-aware `resolveApiKey(...)` to the native `OpenAiCompatibleClient` instead of assuming only `OPENAI_API_KEY`
- keep explicit config API keys highest priority, then fall back through provider-specific environment variables such as `DEEPSEEK_API_KEY`, `ZHIPU_API_KEY`, `DOUBAO_API_KEY`, `ARK_API_KEY`, and the shared OpenAI-compatible fallbacks
- route `ApplicationBootstrap` commentary client setup through that resolver so runtime behavior matches the documented env fallback chain much more closely
- extend native commentary tests to cover explicit-key priority and provider-aware environment fallback behavior

Phase 37 additions:

- align native `normalizeBaseUrl(...)` with the Python OpenAI-compatible provider instead of always forcing a `/v1` suffix
- strip `/chat/completions`, `/responses`, and `/audio/transcriptions` suffixes before rebuilding the base URL
- preserve provider-specific prefixes such as zhipu `/api/paas/v4` and doubao `/api/v3`, which are valid final API roots and must not be rewritten to `/v1`
- extend native commentary tests to cover `responses` suffix stripping plus zhipu/doubao base URL preservation

Phase 38 additions:

- restore Python parity for `scripted_entrance_enabled`, so scripted summon is gated by config instead of always firing when a trajectory exists
- wire that flag through native config persistence, settings UI, bootstrap, and director tests
- fix several MinGW/Qt test-target link issues and bring native `RuntimeDirector` tests back to green

Phase 39 additions:

- add `windows-ninja-release` CMake presets and switch helper build defaults to the MinGW/Ninja release flow that now exists locally
- validate native `Debug` and `Release` configure/build/test end to end with the installed Qt 6.6.3 MinGW toolchain
- confirm all current native test targets pass under both configurations

Phase 40 additions:

- stage deployment output into `out/package/windows-ninja-release` instead of packaging the raw build tree
- update the Inno Setup script to package only the staged runtime files, excluding test executables and intermediate artifacts
- generate `out/installer/CyberCompanionCppSetup.exe` successfully from the staged runtime directory
- add `tools/cpp/smoke_check_cpp.ps1` for local startup-log smoke validation against the staged package

Phase 41 additions:

- add a first native `IdleInvasionController`, driven by `IdleMonitorService::idleTimeUpdated(...)`, with debug trigger, grid placement, spawn cadence, and retreat handling
- introduce native `idle_invasion` config persistence for enable/start delay/spawn intervals/max invaders/scale/cell padding/participating gifs/retreat style
- wire invasion settings into the native `SettingsDialog`, tray debug menu, bootstrap lifecycle, and runtime director gating so legacy idle peek stays disabled when invasion is enabled
- extend the native test suite with idle invasion coverage and keep the full MinGW release suite green after the new integration

Phase 42 additions:

- add tray-level `DND` toggle plumbing and propagate it into the native `RuntimeDirector` and `IdleInvasionController`
- suppress legacy auto-idle peek while DND is enabled, while keeping manual actions such as summon/commentary/debug triggers available
- add C++ idle invasion observability logs for config apply, debug trigger, grid init, spawn, saturation, retreat, and reset paths
- extend native director and idle invasion tests to lock the new DND behavior

Phase 43 additions:

- add native `full_screen_pause` and `resident_mode` config persistence, settings UI, and runtime application
- add `WindowsFullscreenMonitorService` to poll the foreground window and detect fullscreen occupancy at runtime
- wire fullscreen state changes into `RuntimeDirector`, so fullscreen pause can suppress auto-peek and resident mode can hide/show the entity accordingly
- extend native director/config/settings tests to cover fullscreen gating and resident-mode visibility behavior

Phase 44 additions:

- stop treating `idle_invasion.enabled` as a blanket kill-switch for native legacy idle behavior
- feed `idleTimeUpdated(...)` into `RuntimeDirector` and compare it with `idle_invasion.start_delay_ms`
- allow one native legacy auto-peek path before invasion start delay is reached, while still deferring to invasion after the configured takeover point
- extend native director tests to lock both branches: pre-delay coexistence and post-delay suppression

Phase 45 additions:

- add tray-level user-facing actions for `使用指南`, `复制最近日志`, and `反馈问题`
- add `AppPaths` helpers for quick-start URL, feedback issue URL, and recent log tail extraction
- open the quick-start guide automatically on first run after persisting `firstRun=false`
- extend native path-focused tests to lock the new URL helpers and log-tail extraction behavior

Phase 46 additions:

- surface screen-commentary failures through tray notifications instead of only logging / speech fallback
- add a small user-facing runtime error summarizer that explicitly points people to `复制最近日志` and `反馈问题`
- keep cancellation quiet while still surfacing real provider/network failures to the user

Phase 47 additions:

- surface native audio degradation to the user instead of leaving `edge-tts` failures and script-audio cache misses in logs only
- emit playback warnings for missing preferred script audio, missing `edge-tts`, synthesis failure, and log-only fallback paths
- teach `ConfigRepository::save(...)` to return an error string so the application shell can explain configuration persistence failures
- route config-save failures through the same tray-notification path, with dedupe for repeated runtime audio warnings
- extend native audio/config tests to lock warning emission and save-failure error propagation

Phase 48 additions:

- fix the release deployment path so Qt `multimedia` plugins are explicitly staged instead of relying on `windeployqt` defaults
- include both `ffmpegmediaplugin` and `windowsmediaplugin` in the packaged output for the MinGW release build
- verify through smoke-check that the packaged app no longer logs `No QtMultimedia backends found` / `Failed to initialize QMediaPlayer`

Phase 49 additions:

- add `screen_commentary.auto_enabled` and `screen_commentary.auto_interval_minutes` to the native config model and legacy bootstrap path
- expose automatic screen-commentary controls in the native `SettingsDialog`
- add a native auto-commentary timer to `RuntimeDirector`, with DND/fullscreen skip behavior and rescheduling after commentary sessions end
- extend native config/settings/director tests to lock the new auto-commentary configuration and timer-triggered request flow

Phase 50 additions:

- port the Python-side `PresenceDetector` logic into native C++ with configurable thresholds and a dedicated native test target
- add native `GazeSample` to the service contract and extend the vision contract with `gazeUpdated/cameraError/cameraStateChanged`
- add vision-oriented config fields (`camera_enabled`, `eye_tracking_enabled`, `periodic_scan_enabled`, `periodic_scan_interval_minutes`) to the native config model and settings dialog
- extend `RuntimeDirector` with native gaze-update and camera-error slots plus `gazeFollowRequested(...)` so the widget/gaze contract is now explicit on the C++ side
- wire `RuntimeDirector -> EntityWidget` gaze-follow signaling in the application bootstrap, while keeping the actual camera backend out of scope for this phase

Phase 51 additions:

- add a first native `QtVisionService` implementation on top of `QtMultimedia/QVideoSink`, with configurable `camera_index` and `camera_target_fps`
- expose `camera_consent_granted`, `camera_index`, and `camera_target_fps` in the native config model, legacy bootstrap path, and `SettingsDialog`
- connect the application shell to a real native vision service lifecycle, including tray-facing camera error notifications
- add native periodic camera scan scheduling/debug flow to `RuntimeDirector`, with sample collection, DND/fullscreen skip logic, and visual state override requests
- extend native config/settings/director tests to lock camera config round-trip plus periodic camera scan debug/collection behavior

Phase 52 additions:

- finish the native `PresenceDetector -> RuntimeDirector` behavior path so `PresentPassive / Absent / PresentActive` now drive passive companion, deep-sleep, and timeout recovery on the C++ side
- feed `camera_target_fps` into the native `PresenceDetector` so presence timing uses the same sampling assumptions as the active camera backend
- extend `RuntimeDirector` tests to lock silent passive summon, passive timeout hiding, and deep-sleep entry behavior

Phase 53 additions:

- extract a dedicated native `VisionFrameAnalyzer` from `QtVisionService` so frame brightness/motion/centroid analysis is no longer buried inside the camera service
- add a native `CyberCompanionCppVisionAnalyzerTests` target to lock first-frame/no-motion behavior, moving bright-region detection, and centroid-based `faceX/faceY` output
- relax native camera format selection to prefer the closest available format instead of hard-failing when the requested FPS is not exactly exposed by the device

Phase 54 additions:

- align native camera availability semantics with real `QCamera::activeChanged` instead of treating `start()` as immediate readiness
- add user-visible runtime feedback for camera-enabled-without-consent and camera start/stop transitions so the vision path is less opaque during debugging
- extend native director coverage to lock camera state feedback behavior

Phase 55 additions:

- move native vision runtime closer to the Python resource model: fullscreen pause now suppresses camera occupancy when no explicit debug override is active
- make tray-triggered camera debug scans bring up the camera on demand and automatically release it again after debug collection completes when eye tracking / periodic scan are otherwise disabled
- remove the “already running camera debug does nothing” hole by triggering native debug scans immediately when the camera is already active

Phase 56 additions:

- add native `VoiceCommandMatcher` parity for summon / screen commentary / hide / toggle / status / no-face debug utterances
- add native `OpenAiVoiceInputService` with microphone capture, WAV packaging, OpenAI-compatible `/audio/transcriptions` upload, and transcript-to-command routing
- wire microphone / wakeup / ASR provider/model/base_url/prompt/temperature into the native config model and `SettingsDialog`
- add a minimal continuous wakeup polling loop that repeatedly captures short microphone windows and only emits transcripts when configured wake phrases are detected

Phase 57 additions:

- add native `CharacterManifestCatalog` so C++ can scan `characters/*/manifest.json`, build a role-switch submenu, and persist the active character in config
- add native tray parity for `状态 / 编辑台词 / 重载台词 / 切换角色`, including direct `scripts.json` opening and runtime script reload
- wire `sad_comfort_debug` and `no_face_debug` through native tray actions, voice command routing, and `RuntimeDirector` debug behavior

Phase 58 additions:

- add native no-face absence tracking and return greeting flow, including qualified absence marking, cooldown, and high-priority spoken greetings when the user returns

Phase 59 additions:

- replace the ad-hoc C++ idle peek timer with a Python-aligned idle threshold + jitter path, and let idle invasion fully own automatic idle behavior when enabled
- add a lightweight native `MoodSystem`, expose mood in the status summary, and wire stable expression tracking into mood deltas
- restore Python-like auto-dismiss semantics so regular idle dismissal hides quietly instead of always triggering panic flee

Phase 60 additions:

- let native periodic camera scans request camera runtime on demand instead of treating `periodic_scan_enabled` as a reason to keep the camera permanently running

Phase 61 additions:

- expose native hotkey registration state so the tray/startup layer knows whether `Ctrl+Shift+S` and `Ctrl+B` were actually registered
- replace the old phase placeholder startup notification with a Python-aligned runtime summary for camera, voice mode, hotkeys, and debug log path

Phase 62 additions:

- expand native voice commands so the C++ runtime can trigger peek, flee, scripted trajectory debug, camera scan debug, and idle invasion debug instead of falling back to a generic “not wired” toast
- make the contains-match command resolver prefer more specific phrases over shorter generic ones, so phrases like `出来看一眼` map to `peek` instead of being swallowed by `summon`
- add fail-fast validation and session-level degradation for native voice input when provider/key/base URL are invalid, and port provider-aware HTTP hints for ASR 401/403/404/429 failures

Phase 63 additions:

- add offline-mode parity across the native config model, settings dialog, startup/status summary, screen commentary gating, and voice-input gating so the C++ runtime can explicitly run in a no-remote-AI mode without silently attempting network calls
- lock offline behavior with native tests for config/settings persistence, commentary fail-fast, push-to-talk fail-fast, and Director-level manual/automatic commentary suppression

Phase 64 additions:

- promote the most frequently used runtime toggles into the tray menu so `offline mode`, `resident mode`, and `auto commentary` can be switched immediately without opening settings, while still persisting to config and updating the native runtime in place
- stop auto-starting continuous wakeup when offline mode is enabled, so the C++ runtime no longer starts a remote voice loop only to fail-fast a moment later

Phase 65 additions:

- expand runtime toggles further so tray/voice control now covers camera, microphone, wakeup, eye tracking, periodic scan, audio reactive mode, and the `push_to_talk` vs `continuous` voice mode switch
- add a dedicated native tray-controller test target to lock the growing set of checkable tray actions and signal emissions
- make the unsupported `google` ASR path explicit in the settings UI and startup/runtime warnings instead of letting it masquerade as a first-class native provider

Phase 66 additions:

- make the `ocr_fallback_enabled` commentary path materially useful by attaching foreground-window title and process-name context when image capture is unavailable, instead of falling back to a context-free no-image prompt
- extend native commentary tests to lock that no-image fallback context formatting so future provider or payload refactors do not silently regress it

Phase 67 additions:

- add a dedicated native `CharacterSwitchMatcher` so role switching can be triggered by voice using manifest-backed character names instead of only the tray submenu
- route voice transcripts through that dynamic matcher before the static voice-command table, keeping the role-package expansion path out of the core static command list

Phase 68 additions:

- add native role-catalog hot reload so `manifest.json` changes can be picked up from the tray and voice-command path without restarting the C++ app
- keep the active role stable on reload when possible, and fall back to the first valid manifest only when the previous active role disappears

Phase 69 additions:

- extend `CharacterManifestCatalog` with manifest-backed aliases so role packages can declare spoken names directly in `manifest.json`
- teach the native `CharacterSwitchMatcher` to match those aliases, not just `id/name`, so voice role switching no longer depends on the package name exactly matching what the user says

Phase 70 additions:

- expose `start_minimized` in the native settings dialog so startup behavior can be changed without editing JSON by hand
- update the commentary fallback label to describe the real native behavior now that `ocr_fallback_enabled` attaches foreground-window context instead of being a placeholder toggle

Phase 71 additions:

- extend native voice commands to cover the existing tray/user entrypoints: `settings`, `guide`, `edit scripts`, `copy recent logs`, `open config`, `open data dir`, `open logs`, `feedback`, and `about`
- route those user-facing commands directly through `ApplicationBootstrap`, removing the remaining native `recognized but not yet wired` fallback on that path
- add matcher coverage for the new tray-parity voice commands so future command-table refactors do not silently drop them

Phase 72 additions:

- split the entity right-click menu away from the full tray menu so the native window now exposes the same focused interaction set as the Python runtime instead of dumping every tray toggle into the in-scene context menu
- route that focused entity context menu through existing runtime methods (`summon`, debug triggers, guide/scripts/logs/settings, DND, quit) so interaction parity improves without introducing another state owner

Phase 73 additions:

- add a native update-check path that reads local `version.json`, resolves `update_url` with environment override support, parses GitHub-compatible release payloads, and compares versions numerically
- expose `检查更新` through both tray and voice-command entrypoints instead of keeping update metadata as documentation-only configuration
- stage `version.json` into the packaged output so installed builds can still discover their configured update source outside the source tree

Phase 79 additions:

- Native `SettingsDialog` now supports `测试 API 连接`, issuing a minimal OpenAI-compatible chat completion request with provider-aware key and base URL normalization.
- Native config/settings/runtime now support `behavior.auto_start_on_login`, synchronized to Windows `HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run`.
- Native config/settings/runtime now support `trigger.jitter_range_seconds`, so idle thresholds are no longer hardcoded to `[-30, +60]`.
- Native config/settings/runtime now apply `appearance.ascii_width` and `appearance.font_size_px` to the real `EntityWidget` card instead of only persisting them.
- Native config/settings/runtime now expose `audio.tts_provider`, keep `edge-tts` as the only supported backend, and surface fallback warnings when a different provider is configured.
- Native `ConfigRepository` now writes Python-compatible `audio / vision / wakeup / behavior` top-level sections and imports the same shape during legacy bootstrap.
- Legacy `google/google_webspeech` ASR configs now auto-migrate to `zhipu_asr` in the native runtime instead of remaining as a permanently unsupported option.
- Native autostart entries now launch with `--autostart` and can add `--start-minimized`, allowing quieter login startup semantics.

- add native config parity for `appearance.position`, `trigger.idle_threshold_seconds`, `trigger.auto_dismiss_seconds`, and `audio.volume`
- apply those settings at runtime so screen edge, idle threshold jitter base, auto-dismiss timing, and Qt audio output volume no longer stay hardcoded in the C++ path

Suggested configure commands:

```powershell
cmake --preset windows-ninja-debug
cmake --build --preset build-windows-ninja-debug
ctest --test-dir out/build/windows-ninja-debug --output-on-failure

cmake --preset windows-ninja-release
cmake --build --preset build-windows-ninja-release
ctest --preset test-windows-ninja-release --output-on-failure
```

Helper scripts:

```powershell
.\tools\cpp\doctor_cpp_env.ps1
.\tools\cpp\build_cpp.ps1 -Preset windows-ninja-release -RunTests
.\tools\cpp\deploy_cpp.ps1 -BuildDir out/build/windows-ninja-release -DeployMode Release -OutputDir out/package/windows-ninja-release
.\tools\cpp\smoke_check_cpp.ps1
```

Expected local prerequisites:

- CMake 3.24+
- Qt 6.6+ MinGW toolchain (`QT_ROOT=C:\Qt\6.6.3\mingw_64`)
- Ninja
- MinGW g++
- `windeployqt` for deployment
- Inno Setup 6 for installer generation

Phase 0 directories:

- `src_cpp/app/`: application bootstrap and composition root
- `src_cpp/runtime/`: paths, config, logging, single-instance guard
- `src_cpp/runtime/runtime_director.*`: native behavior orchestration skeleton
- `src_cpp/runtime/idle_invasion_controller.*`: native idle invasion controller and lifecycle
- `src_cpp/runtime/character_asset_catalog.*`: default character asset resolver
- `src_cpp/ui/`: tray and placeholder entity widget
- `src_cpp/ui/trajectory_player.*`: reusable trajectory animation helper
- `src_cpp/services/`: native service interfaces, provider abstraction, and Phase 5 audio/network shells
- `tests_cpp/`: native C++ tests
- `installer/`: Inno Setup packaging scripts

