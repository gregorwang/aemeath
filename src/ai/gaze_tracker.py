from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Any

from PySide6.QtCore import QThread, Signal


@dataclass(slots=True)
class GazeData:
    """Normalized face position from camera frame."""

    face_detected: bool = False
    face_x: float = 0.0
    face_y: float = 0.0
    confidence: float = 0.0


class GazeTracker(QThread):
    """
    Camera-based gaze tracker.

    Privacy guarantees:
    - Frames are processed in-memory only and never persisted.
    - No network upload is performed from this module.
    """

    gaze_updated = Signal(object)  # GazeData
    camera_error = Signal(str)
    camera_state_changed = Signal(bool)  # True=start, False=stop

    MIN_DETECTION_CONFIDENCE = 0.5
    NOSE_TIP = 1

    def __init__(self, camera_index: int = 0, target_fps: int = 15, parent=None):
        super().__init__(parent)
        self._camera_index = camera_index
        self._target_fps = max(1, min(30, int(target_fps)))
        self._running = False

    def start_tracking(self) -> None:
        if self.isRunning():
            return
        self._running = True
        self.start()

    def stop_tracking(self) -> None:
        self._running = False
        if self.isRunning():
            self.wait(3000)

    def run(self) -> None:
        cv2, mp, np = self._load_deps()
        if cv2 is None or mp is None or np is None:
            self.camera_error.emit("缺少依赖: 需要安装 opencv-python、mediapipe、numpy")
            return

        cap = cv2.VideoCapture(self._camera_index)
        if not cap.isOpened():
            self.camera_error.emit("无法打开摄像头")
            return

        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)

        face_mesh = mp.solutions.face_mesh.FaceMesh(
            max_num_faces=1,
            refine_landmarks=True,
            min_detection_confidence=self.MIN_DETECTION_CONFIDENCE,
            min_tracking_confidence=0.5,
        )

        frame_interval = 1.0 / self._target_fps
        self.camera_state_changed.emit(True)
        try:
            while self._running:
                start_ts = time.perf_counter()
                ret, frame = cap.read()
                if not ret:
                    time.sleep(frame_interval)
                    continue

                rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                results = face_mesh.process(rgb_frame)

                if results.multi_face_landmarks:
                    landmarks = results.multi_face_landmarks[0]
                    gaze_data = self._calculate_gaze(landmarks)
                else:
                    gaze_data = GazeData(face_detected=False)

                self.gaze_updated.emit(gaze_data)

                elapsed = time.perf_counter() - start_ts
                sleep_s = frame_interval - elapsed
                if sleep_s > 0:
                    time.sleep(sleep_s)
        finally:
            cap.release()
            face_mesh.close()
            self.camera_state_changed.emit(False)

    def _calculate_gaze(self, landmarks: Any) -> GazeData:
        import numpy as np

        nose = landmarks.landmark[self.NOSE_TIP]
        face_x = -(nose.x * 2.0 - 1.0)
        face_y = nose.y * 2.0 - 1.0
        confidence = getattr(nose, "visibility", 1.0) or 1.0
        return GazeData(
            face_detected=True,
            face_x=float(np.clip(face_x, -1.0, 1.0)),
            face_y=float(np.clip(face_y, -1.0, 1.0)),
            confidence=float(np.clip(confidence, 0.0, 1.0)),
        )

    @staticmethod
    def _load_deps():
        try:
            import cv2  # type: ignore
            import mediapipe as mp  # type: ignore
            import numpy as np  # type: ignore
        except Exception:
            return None, None, None
        return cv2, mp, np

