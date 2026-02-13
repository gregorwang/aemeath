from __future__ import annotations

import logging
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
    emotion_label: str = "neutral"  # happy / neutral / angry / unknown
    emotion_score: float = 0.0


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
    LOGGER_NAME = "CyberCompanion"

    def __init__(self, camera_index: int = 0, target_fps: int = 15, parent=None):
        super().__init__(parent)
        self._camera_index = camera_index
        self._target_fps = max(1, min(30, int(target_fps)))
        self._running = False

    def set_camera_config(self, camera_index: int, target_fps: int) -> None:
        self._camera_index = max(0, int(camera_index))
        self._target_fps = max(1, min(30, int(target_fps)))

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
        logger = logging.getLogger(self.LOGGER_NAME)
        cv2, mp, np, missing_deps, dep_error = self._load_deps()
        if cv2 is None or mp is None or np is None:
            missing = missing_deps or "opencv-python、mediapipe、matplotlib、numpy"
            detail = f" ({dep_error})" if dep_error else ""
            logger.error("[Vision] Dependency load failed: %s%s", missing, detail)
            self.camera_error.emit(f"缺少依赖或加载失败: {missing}{detail}")
            return

        dshow_backend = getattr(cv2, "CAP_DSHOW", None)
        cap = cv2.VideoCapture(self._camera_index, dshow_backend) if dshow_backend is not None else cv2.VideoCapture(self._camera_index)
        if not cap.isOpened():
            cap.release()
            cap = cv2.VideoCapture(self._camera_index)
        if not cap.isOpened():
            logger.error("[Vision] Cannot open camera index=%s", self._camera_index)
            self.camera_error.emit(
                f"无法打开摄像头(index={self._camera_index})；请检查设备占用或系统相机权限。"
            )
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
        logger.info("[Vision] Camera tracking started (index=%s, fps=%s)", self._camera_index, self._target_fps)
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
                    gaze_data = GazeData(face_detected=False, emotion_label="unknown", emotion_score=0.0)

                self.gaze_updated.emit(gaze_data)

                elapsed = time.perf_counter() - start_ts
                sleep_s = frame_interval - elapsed
                if sleep_s > 0:
                    time.sleep(sleep_s)
        finally:
            cap.release()
            face_mesh.close()
            logger.info("[Vision] Camera tracking stopped")
            self.camera_state_changed.emit(False)

    def _calculate_gaze(self, landmarks: Any) -> GazeData:
        import numpy as np

        nose = landmarks.landmark[self.NOSE_TIP]
        face_x = -(nose.x * 2.0 - 1.0)
        face_y = nose.y * 2.0 - 1.0
        confidence = getattr(nose, "visibility", 1.0) or 1.0
        emotion_label, emotion_score = self._estimate_expression(landmarks)
        return GazeData(
            face_detected=True,
            face_x=float(np.clip(face_x, -1.0, 1.0)),
            face_y=float(np.clip(face_y, -1.0, 1.0)),
            confidence=float(np.clip(confidence, 0.0, 1.0)),
            emotion_label=emotion_label,
            emotion_score=emotion_score,
        )

    @staticmethod
    def _estimate_expression(landmarks: Any) -> tuple[str, float]:
        # Landmark indices from MediaPipe face mesh.
        left_corner = landmarks.landmark[61]
        right_corner = landmarks.landmark[291]
        upper_lip = landmarks.landmark[13]
        lower_lip = landmarks.landmark[14]
        left_cheek = landmarks.landmark[234]
        right_cheek = landmarks.landmark[454]
        left_eye = landmarks.landmark[33]
        right_eye = landmarks.landmark[263]
        left_brow = landmarks.landmark[105]
        right_brow = landmarks.landmark[334]

        face_width = max(1e-6, abs(right_cheek.x - left_cheek.x))
        mouth_width = abs(right_corner.x - left_corner.x)
        mouth_open = abs(lower_lip.y - upper_lip.y)
        smile_ratio = mouth_width / face_width
        brow_drop = ((left_brow.y + right_brow.y) * 0.5) - ((left_eye.y + right_eye.y) * 0.5)

        if smile_ratio >= 0.38 and mouth_open >= 0.008:
            score = min(1.0, max(0.0, (smile_ratio - 0.38) / 0.16 + mouth_open / 0.03))
            return "happy", float(score)
        if brow_drop >= 0.038 and smile_ratio < 0.37:
            score = min(1.0, max(0.0, (brow_drop - 0.038) / 0.08))
            return "angry", float(score)
        neutral_score = 1.0 - min(1.0, abs(smile_ratio - 0.34) / 0.2)
        return "neutral", float(max(0.0, neutral_score))

    @staticmethod
    def _load_deps():
        errors: dict[str, str] = {}
        cv2 = mp = np = mpl = None
        try:
            import cv2  # type: ignore
        except Exception as exc:
            errors["opencv-python"] = str(exc)
        try:
            import mediapipe as mp  # type: ignore
        except Exception as exc:
            errors["mediapipe"] = str(exc)
        try:
            import matplotlib as mpl  # type: ignore
        except Exception as exc:
            errors["matplotlib"] = str(exc)
        try:
            import numpy as np  # type: ignore
        except Exception as exc:
            errors["numpy"] = str(exc)

        if cv2 is None or mp is None or np is None or mpl is None:
            missing = "、".join(errors.keys())
            detail = "; ".join(f"{name}: {message}" for name, message in errors.items())
            return None, None, None, missing, detail
        return cv2, mp, np, "", ""
