"""
GIF State Mapper & Reactive Trigger System.

Maps the state1–state7 GIFs to specific events/behaviors and coordinates
the GIF particle system with audio output detection, idle states, and
other triggers.

GIF Semantic Assignments:
    state1.gif  – 好奇/探头  (Curious peek – triggered on peeking state)
    state2.gif  – 兴奋/活跃  (Excited – triggered on user interaction)
    state3.gif  – 律动/跟随音乐  (Vibing to music – triggered by audio detection)
    state4.gif  – 害羞/逃跑  (Shy retreat – triggered on flee state)
    state5.gif  – 思考/发呆  (Thinking/spacing out – triggered on prolonged idle)
    state6.gif  – 开心/打招呼  (Happy greeting – triggered on summon/wakeup)
    state7.gif  – 微型装饰  (Small ambient particle – always available for waves)

The mapper is event-driven: other modules emit signals, and this mapper
reacts by spawning appropriate GIF particles.
"""

from __future__ import annotations

import random
from pathlib import Path
from typing import Optional

from PySide6.QtCore import QObject, QTimer, Signal, Slot

try:
    from ui.gif_particle import GifParticleManager, ParticleConfig
except ModuleNotFoundError:
    from ..ui.gif_particle import GifParticleManager, ParticleConfig


class GifStateMapper(QObject):
    """
    Coordinates GIF particle spawning based on application events.

    Connect this to Director state changes, AudioOutputMonitor signals,
    and other event sources.
    """

    # Semantic GIF role names -> filename
    GIF_ROLES = {
        "curious":    "state1.gif",
        "excited":    "state2.gif",
        "music_vibe": "state3.gif",
        "shy":        "state4.gif",
        "thinking":   "state5.gif",
        "greeting":   "state6.gif",
        "ambient":    "state7.gif",
        "main":       "aemeath.gif",
    }

    def __init__(
        self,
        characters_dir: Path,
        particle_manager: GifParticleManager,
        parent: QObject | None = None,
    ):
        super().__init__(parent)
        self._characters_dir = characters_dir
        self._particle_manager = particle_manager
        self._gif_paths: dict[str, str] = {}
        self._audio_reaction_active = False
        self._audio_pulse_timer = QTimer(self)
        self._audio_pulse_timer.setInterval(4000)  # Spawn a new music particle every 4s
        self._audio_pulse_timer.timeout.connect(self._on_audio_pulse)
        self._enabled = True

        self._resolve_gif_paths()

    def _resolve_gif_paths(self) -> None:
        """Resolve all GIF paths, checking existence."""
        for role, filename in self.GIF_ROLES.items():
            path = self._characters_dir / filename
            if path.exists():
                self._gif_paths[role] = str(path)
            else:
                print(f"[GifStateMapper] GIF 未找到: {path} (role={role})")

    def set_enabled(self, enabled: bool) -> None:
        """Enable or disable particle effects."""
        self._enabled = enabled
        if not enabled:
            self._particle_manager.dismiss_all()
            self._stop_audio_reaction()

    # ─── Event Handlers ────────────────────────────────────────

    @Slot()
    def on_peeking(self) -> None:
        """Called when entity enters PEEKING state."""
        if not self._enabled:
            return
        curious_path = self._gif_paths.get("curious")
        if curious_path:
            config = ParticleConfig(
                gif_path=curious_path,
                scale=0.6,
                duration_ms=3000,
                enter_duration_ms=1000,
                exit_duration_ms=600,
                edge="random",
                target="random_inner",
                opacity=0.85,
            )
            self._particle_manager.spawn_particle(config)

    @Slot()
    def on_engaged(self) -> None:
        """Called when entity enters ENGAGED state – show excitement."""
        if not self._enabled:
            return
        excited_path = self._gif_paths.get("excited")
        if excited_path:
            # Spawn 2 excited particles from different edges
            self._particle_manager.spawn_wave(
                [excited_path],
                count=2,
                scale=0.5,
                duration_ms=4000,
                stagger_ms=400,
            )

    @Slot()
    def on_fleeing(self) -> None:
        """Called when entity enters FLEEING state – shy scattering."""
        if not self._enabled:
            return
        shy_path = self._gif_paths.get("shy")
        if shy_path:
            self._particle_manager.spawn_wave(
                [shy_path],
                count=3,
                scale=0.4,
                duration_ms=2000,
                stagger_ms=150,
                edges="random",
            )

    @Slot()
    def on_hidden(self) -> None:
        """Called when entity enters HIDDEN – dismiss all particles."""
        self._particle_manager.dismiss_all()

    @Slot()
    def on_summoned(self) -> None:
        """Called on force-summon (wakeup/tray) – greeting effect."""
        if not self._enabled:
            return
        greeting_path = self._gif_paths.get("greeting")
        if greeting_path:
            self._particle_manager.spawn_wave(
                [greeting_path],
                count=3,
                scale=0.6,
                duration_ms=5000,
                stagger_ms=500,
                edges="random",
            )

    @Slot()
    def on_prolonged_idle(self) -> None:
        """Called when user is idle for extended period – thinking particles."""
        if not self._enabled:
            return
        thinking_path = self._gif_paths.get("thinking")
        ambient_path = self._gif_paths.get("ambient")
        paths = [p for p in [thinking_path, ambient_path] if p]
        if paths:
            self._particle_manager.spawn_wave(
                paths,
                count=2,
                scale=0.5,
                duration_ms=6000,
                stagger_ms=800,
            )

    # ─── Audio Reaction ────────────────────────────────────────

    @Slot()
    def on_audio_started(self) -> None:
        """Called when system audio output is detected."""
        if not self._enabled:
            return
        if self._audio_reaction_active:
            return

        self._audio_reaction_active = True
        print("[GifStateMapper] 🎵 音频检测到，开始音乐律动效果")

        # Spawn initial music vibe particles
        music_path = self._gif_paths.get("music_vibe")
        ambient_path = self._gif_paths.get("ambient")
        if music_path:
            self._particle_manager.spawn_wave(
                [music_path],
                count=2,
                scale=0.7,
                duration_ms=8000,
                stagger_ms=600,
                edges="random",
            )

        # Start periodic pulse for ongoing music
        self._audio_pulse_timer.start()

    @Slot()
    def on_audio_stopped(self) -> None:
        """Called when system audio output stops."""
        self._stop_audio_reaction()
        print("[GifStateMapper] 🔇 音频停止，结束律动效果")

    def _stop_audio_reaction(self) -> None:
        """Stop the periodic music pulse."""
        self._audio_reaction_active = False
        if self._audio_pulse_timer.isActive():
            self._audio_pulse_timer.stop()

    def _on_audio_pulse(self) -> None:
        """Periodically spawn music particles while audio is playing."""
        if not self._audio_reaction_active or not self._enabled:
            self._audio_pulse_timer.stop()
            return

        music_path = self._gif_paths.get("music_vibe")
        ambient_path = self._gif_paths.get("ambient")
        paths = [p for p in [music_path, ambient_path] if p]
        if not paths:
            return

        # Don't overcrowd – only spawn if we're below threshold
        if self._particle_manager.active_count < 4:
            config = ParticleConfig(
                gif_path=random.choice(paths),
                scale=random.uniform(0.4, 0.8),
                duration_ms=random.randint(4000, 7000),
                enter_duration_ms=random.randint(800, 1500),
                exit_duration_ms=random.randint(600, 1000),
                edge="random",
                target="random_inner",
                opacity=random.uniform(0.6, 0.95),
            )
            self._particle_manager.spawn_particle(config)

    # ─── Ambient / Random ──────────────────────────────────────

    def spawn_random_ambient(self, count: int = 1) -> None:
        """Spawn small random ambient particles."""
        if not self._enabled:
            return
        ambient_path = self._gif_paths.get("ambient")
        if not ambient_path:
            return
        for _ in range(count):
            config = ParticleConfig(
                gif_path=ambient_path,
                scale=random.uniform(0.3, 0.5),
                duration_ms=random.randint(3000, 6000),
                enter_duration_ms=random.randint(600, 1200),
                exit_duration_ms=random.randint(400, 800),
                edge="random",
                target="random_inner",
                opacity=random.uniform(0.4, 0.7),
            )
            self._particle_manager.spawn_particle(config)

    def shutdown(self) -> None:
        """Cleanup on application exit."""
        self._stop_audio_reaction()
        self._particle_manager.dismiss_all()
