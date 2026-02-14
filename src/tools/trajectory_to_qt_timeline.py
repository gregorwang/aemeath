from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def _extract_points(payload: dict[str, Any]) -> list[dict[str, float | int]]:
    points = payload.get("points")
    if isinstance(points, list):
        extracted: list[dict[str, float | int]] = []
        for item in points:
            if not isinstance(item, dict):
                continue
            try:
                extracted.append(
                    {
                        "x": float(item["x"]),
                        "y": float(item["y"]),
                        "t": float(item["t"]),
                        "s": int(item.get("s", 1)),
                    }
                )
            except Exception:
                continue
        return extracted

    keyframes = payload.get("keyframes")
    if not isinstance(keyframes, list):
        return []

    extracted = []
    for frame in keyframes:
        if not isinstance(frame, dict):
            continue
        try:
            if "time_ms" in frame:
                t_value = float(frame["time_ms"]) / 1000.0
            else:
                t_value = float(frame["t"])
            extracted.append(
                {
                    "x": float(frame["x"]),
                    "y": float(frame["y"]),
                    "t": t_value,
                    "s": int(frame.get("state", frame.get("s", 1))),
                }
            )
        except Exception:
            continue
    return extracted


def _sanitize_points(raw_points: list[dict[str, float | int]]) -> list[dict[str, float | int]]:
    sanitized: list[dict[str, float | int]] = []
    last_t = -1.0
    for item in raw_points:
        t = float(item["t"])
        if t < 0:
            continue
        if t <= last_t:
            t = last_t + 0.0001
        sanitized.append(
            {
                "x": float(item["x"]),
                "y": float(item["y"]),
                "t": t,
                "s": int(item.get("s", 1)),
            }
        )
        last_t = t
    return sanitized


def _resolve_total_duration_s(payload: dict[str, Any], points: list[dict[str, float | int]]) -> float:
    duration_ms = payload.get("duration_ms")
    if duration_ms is not None:
        try:
            duration = float(duration_ms) / 1000.0
            if duration > 0:
                if points:
                    return max(duration, float(points[-1]["t"]))
                return duration
        except Exception:
            pass

    total_duration = float(payload.get("total_duration", 0.0) or 0.0)
    if points:
        total_duration = max(total_duration, float(points[-1]["t"]))
    return max(0.0, total_duration)


def _interpolate_points(
    points: list[dict[str, float | int]], duration_s: float, fps: int
) -> list[dict[str, int | float]]:
    if not points:
        return []
    if duration_s <= 0:
        duration_s = float(points[-1]["t"])
    if duration_s <= 0:
        duration_s = 0.001

    frame_count = max(2, int(round(duration_s * fps)) + 1)
    frame_step_s = duration_s / float(frame_count - 1)
    point_idx = 0
    keyframes: list[dict[str, int | float]] = []
    last_time_ms = -1

    for frame_idx in range(frame_count):
        elapsed = frame_idx * frame_step_s
        if frame_idx == frame_count - 1:
            elapsed = duration_s

        while point_idx < len(points) - 1 and float(points[point_idx + 1]["t"]) <= elapsed:
            point_idx += 1

        p0 = points[point_idx]
        p1 = points[point_idx + 1] if point_idx < len(points) - 1 else p0
        t0 = float(p0["t"])
        t1 = float(p1["t"])
        if t1 > t0:
            alpha = max(0.0, min(1.0, (elapsed - t0) / (t1 - t0)))
        else:
            alpha = 0.0

        x = float(p0["x"]) + (float(p1["x"]) - float(p0["x"])) * alpha
        y = float(p0["y"]) + (float(p1["y"]) - float(p0["y"])) * alpha

        s0 = int(p0.get("s", 1))
        s1 = int(p1.get("s", s0))
        state = s1 if (s1 != s0 and alpha >= 0.35) else s0

        raw_time_ms = int(round(elapsed * 1000.0))
        if frame_idx == frame_count - 1:
            raw_time_ms = max(raw_time_ms, int(round(duration_s * 1000.0)))
        time_ms = max(raw_time_ms, last_time_ms + 1)
        last_time_ms = time_ms
        at = float(time_ms) / float(last_time_ms if frame_idx == frame_count - 1 else int(round(duration_s * 1000.0)) or 1)
        keyframes.append(
            {
                "time_ms": time_ms,
                "at": round(min(1.0, max(0.0, at)), 6),
                "x": int(round(x)),
                "y": int(round(y)),
                "state": int(state),
            }
        )

    if keyframes:
        duration_ms = keyframes[-1]["time_ms"]
        if isinstance(duration_ms, int) and duration_ms > 0:
            for frame in keyframes:
                frame["at"] = round(float(frame["time_ms"]) / float(duration_ms), 6)
            keyframes[-1]["at"] = 1.0
    return keyframes


def _build_state_events(keyframes: list[dict[str, int | float]]) -> list[dict[str, int]]:
    events: list[dict[str, int]] = []
    prev_state: int | None = None
    for frame in keyframes:
        state = int(frame["state"])
        if prev_state is None or state != prev_state:
            events.append({"time_ms": int(frame["time_ms"]), "state": state})
            prev_state = state
    return events


def convert_payload(
    payload: dict[str, Any], *, source_file: str, fps: int = 60
) -> dict[str, Any]:
    normalized_fps = max(1, min(120, int(fps)))
    points = _sanitize_points(_extract_points(payload))
    if not points:
        raise ValueError("Input trajectory has no valid points/keyframes.")

    duration_s = _resolve_total_duration_s(payload, points)
    keyframes = _interpolate_points(points, duration_s, normalized_fps)
    if not keyframes:
        raise ValueError("Failed to generate keyframes from trajectory.")
    state_events = _build_state_events(keyframes)

    return {
        "schema": "qt.animation.timeline.v1",
        "source_file": source_file,
        "target_property": "pos",
        "duration_ms": int(keyframes[-1]["time_ms"]),
        "fps_hint": normalized_fps,
        "coordinate_type": "int",
        "default_easing": "Linear",
        "keyframes": keyframes,
        "state_events": state_events,
    }


def convert_file(input_path: Path, output_path: Path, *, fps: int = 60) -> None:
    payload = json.loads(input_path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("Input trajectory JSON must be an object.")

    converted = convert_payload(payload, source_file=input_path.name, fps=fps)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(converted, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert trajectory JSON (points/keyframes) to qt.animation.timeline.v1.",
    )
    parser.add_argument("input", help="Input trajectory json path")
    parser.add_argument(
        "-o",
        "--output",
        default="",
        help="Output file path (default: <input_stem>_qt_animation.json)",
    )
    parser.add_argument(
        "--fps",
        type=int,
        default=60,
        help="Resampling fps hint for timeline keyframes (default: 60)",
    )
    args = parser.parse_args()

    input_path = Path(args.input).expanduser().resolve()
    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")
    if args.output:
        output_path = Path(args.output).expanduser().resolve()
    else:
        output_path = input_path.with_name(f"{input_path.stem}_qt_animation.json")

    convert_file(input_path, output_path, fps=args.fps)
    print(f"Converted: {input_path} -> {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
