from __future__ import annotations

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
    """Fuse idle time and camera face detection to infer presence."""

    IDLE_THRESHOLD_MS = 300_000
    FACE_ABSENT_FRAMES = 30
    STILLNESS_MOTION_THRESHOLD = 5.0
    STILL_NO_FACE_FRAMES = 20
    DARK_BRIGHTNESS_THRESHOLD = 30.0
    DARK_NO_FACE_FRAMES = 24

    def __init__(self):
        self._face_absent_count = 0
        self._still_no_face_count = 0
        self._dark_no_face_count = 0

    def determine_presence(self, idle_time_ms: int, gaze_data: GazeData | None) -> PresenceState:
        if idle_time_ms < 60_000:
            return PresenceState.PRESENT_ACTIVE

        if gaze_data is None:
            return PresenceState.UNKNOWN

        motion_score = max(0.0, float(getattr(gaze_data, "motion_score", 0.0)))
        brightness = max(0.0, float(getattr(gaze_data, "brightness", 0.0)))

        if not gaze_data.face_detected:
            self._face_absent_count += 1
            if motion_score <= self.STILLNESS_MOTION_THRESHOLD:
                self._still_no_face_count += 1
            else:
                self._still_no_face_count = 0
            if brightness <= self.DARK_BRIGHTNESS_THRESHOLD:
                self._dark_no_face_count += 1
            else:
                self._dark_no_face_count = 0
        else:
            self._face_absent_count = 0
            self._still_no_face_count = 0
            self._dark_no_face_count = 0

        if idle_time_ms >= self.IDLE_THRESHOLD_MS:
            if self._face_absent_count >= self.FACE_ABSENT_FRAMES:
                return PresenceState.ABSENT
            if self._still_no_face_count >= self.STILL_NO_FACE_FRAMES:
                return PresenceState.ABSENT
            if self._dark_no_face_count >= self.DARK_NO_FACE_FRAMES:
                return PresenceState.ABSENT
            if gaze_data.face_detected:
                return PresenceState.PRESENT_PASSIVE

        return PresenceState.UNKNOWN
