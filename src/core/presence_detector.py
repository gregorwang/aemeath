from __future__ import annotations

import time
from enum import Enum, auto

try:
    from ai.gaze_tracker import GazeData
except ModuleNotFoundError:
    from ..ai.gaze_tracker import GazeData


class PresenceState(Enum):
    PRESENT_ACTIVE = auto()
    PRESENT_PASSIVE = auto()
    ABSENT = auto()
    UNKNOWN = auto()


class PresenceDetector:
    """
    Fuse idle time and camera face detection to infer presence.

    Absence windows are normalized by time (using target FPS + elapsed samples),
    so behavior is less sensitive to camera runtime FPS fluctuations.
    """

    REFERENCE_FPS = 15
    IDLE_THRESHOLD_MS = 300_000
    FACE_ABSENT_FRAMES = 30
    STILLNESS_MOTION_THRESHOLD = 5.0
    STILL_NO_FACE_FRAMES = 20
    DARK_BRIGHTNESS_THRESHOLD = 30.0
    DARK_NO_FACE_FRAMES = 24

    def __init__(self, *, target_fps: int = REFERENCE_FPS):
        self._target_fps = max(1, min(60, int(target_fps)))
        self._face_absent_seconds = 0.0
        self._still_no_face_seconds = 0.0
        self._dark_no_face_seconds = 0.0
        self._last_sample_ts: float | None = None

    def set_target_fps(self, target_fps: int) -> None:
        self._target_fps = max(1, min(60, int(target_fps)))
        # Reset sample anchor to avoid large jump immediately after FPS change.
        self._last_sample_ts = None

    def _frame_window_seconds(self, frames: int) -> float:
        reference_fps = max(1, int(self.REFERENCE_FPS))
        return max(0.0, float(frames) / float(reference_fps))

    def _sample_seconds(self) -> float:
        baseline = 1.0 / float(max(1, self._target_fps))
        now = time.monotonic()
        if self._last_sample_ts is None:
            self._last_sample_ts = now
            return baseline
        elapsed = max(0.0, now - self._last_sample_ts)
        self._last_sample_ts = now
        # Clamp extremely long gaps; require at least one baseline tick.
        return max(baseline, min(elapsed, 1.0))

    def determine_presence(self, idle_time_ms: int, gaze_data: GazeData | None) -> PresenceState:
        if idle_time_ms < 60_000:
            self._last_sample_ts = None
            return PresenceState.PRESENT_ACTIVE

        if gaze_data is None:
            self._last_sample_ts = None
            return PresenceState.UNKNOWN

        sample_seconds = self._sample_seconds()
        motion_score = max(0.0, float(getattr(gaze_data, "motion_score", 0.0)))
        brightness = max(0.0, float(getattr(gaze_data, "brightness", 0.0)))

        if not gaze_data.face_detected:
            self._face_absent_seconds += sample_seconds
            if motion_score <= self.STILLNESS_MOTION_THRESHOLD:
                self._still_no_face_seconds += sample_seconds
            else:
                self._still_no_face_seconds = 0.0
            if brightness <= self.DARK_BRIGHTNESS_THRESHOLD:
                self._dark_no_face_seconds += sample_seconds
            else:
                self._dark_no_face_seconds = 0.0
        else:
            self._face_absent_seconds = 0.0
            self._still_no_face_seconds = 0.0
            self._dark_no_face_seconds = 0.0

        if idle_time_ms >= self.IDLE_THRESHOLD_MS:
            if self._face_absent_seconds >= self._frame_window_seconds(self.FACE_ABSENT_FRAMES):
                return PresenceState.ABSENT
            if self._still_no_face_seconds >= self._frame_window_seconds(self.STILL_NO_FACE_FRAMES):
                return PresenceState.ABSENT
            if self._dark_no_face_seconds >= self._frame_window_seconds(self.DARK_NO_FACE_FRAMES):
                return PresenceState.ABSENT
            if gaze_data.face_detected:
                return PresenceState.PRESENT_PASSIVE

        return PresenceState.UNKNOWN
