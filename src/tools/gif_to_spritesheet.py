"""
Convert one or more GIF files into sprite sheet PNG files with JSON metadata.

Usage:
    python src/tools/gif_to_spritesheet.py characters/state1.gif
    python src/tools/gif_to_spritesheet.py characters/state*.gif --output-dir characters/sprites/
    python src/tools/gif_to_spritesheet.py characters/state2.gif --columns 4 --scale 2.0
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

from PIL import Image


def _safe_duration_ms(raw: Any) -> int:
    """Return a usable frame duration in milliseconds."""
    try:
        value = int(raw)
    except (TypeError, ValueError):
        value = 100
    if value <= 0:
        return 100
    return value


def _extract_gif_frames(gif_path: Path, scale: float) -> tuple[list[Image.Image], list[int]]:
    """
    Extract RGBA full frames and durations from a GIF.

    Handles incremental GIF frames by compositing with the previous frame.
    """
    frames: list[Image.Image] = []
    durations: list[int] = []

    with Image.open(gif_path) as img:
        frame_index = 0
        previous_full: Image.Image | None = None

        while True:
            durations.append(_safe_duration_ms(img.info.get("duration", 100)))

            current = img.convert("RGBA")
            disposal = img.info.get("disposal", 0)

            # Most GIFs rely on compositing onto previous full frame.
            if previous_full is not None and disposal != 2:
                composed = previous_full.copy()
                composed.paste(current, (0, 0), current)
                current = composed

            if abs(scale - 1.0) > 1e-6:
                width = max(1, int(round(current.width * scale)))
                height = max(1, int(round(current.height * scale)))
                current = current.resize((width, height), Image.Resampling.LANCZOS)

            frames.append(current.copy())
            previous_full = current.copy()
            frame_index += 1

            try:
                img.seek(frame_index)
            except EOFError:
                break

    return frames, durations


def _compute_columns(frame_count: int, explicit_columns: int) -> int:
    if explicit_columns > 0:
        return explicit_columns
    if frame_count <= 16:
        return frame_count
    return int(math.ceil(math.sqrt(frame_count)))


def gif_to_spritesheet(
    gif_path: Path,
    output_dir: Path,
    *,
    columns: int = 0,
    scale: float = 1.0,
) -> tuple[Path, Path]:
    """Convert a single GIF to sprite sheet + metadata JSON."""
    frames, durations = _extract_gif_frames(gif_path, scale)
    if not frames:
        raise ValueError(f"No valid frames found in GIF: {gif_path}")

    frame_width = frames[0].width
    frame_height = frames[0].height
    frame_count = len(frames)

    cols = _compute_columns(frame_count, columns)
    rows = int(math.ceil(frame_count / cols))

    sheet_width = cols * frame_width
    sheet_height = rows * frame_height

    sheet = Image.new("RGBA", (sheet_width, sheet_height), (0, 0, 0, 0))
    try:
        for idx, frame in enumerate(frames):
            col = idx % cols
            row = idx // cols
            x = col * frame_width
            y = row * frame_height
            sheet.paste(frame, (x, y))

        output_dir.mkdir(parents=True, exist_ok=True)
        stem = gif_path.stem
        sheet_path = output_dir / f"{stem}_sheet.png"
        meta_path = output_dir / f"{stem}_sheet.json"

        sheet.save(sheet_path, "PNG", optimize=True)
    finally:
        sheet.close()
        for frame in frames:
            frame.close()

    avg_duration = sum(durations) / len(durations) if durations else 100.0
    default_fps = round(1000.0 / max(1.0, avg_duration), 1)

    metadata = {
        "image": sheet_path.name,
        "source_gif": gif_path.name,
        "frame_width": frame_width,
        "frame_height": frame_height,
        "frame_count": frame_count,
        "columns": cols,
        "rows": rows,
        "sheet_width": sheet_width,
        "sheet_height": sheet_height,
        "default_fps": default_fps,
        "frame_durations_ms": durations,
        "loop": True,
        "scale": scale,
    }
    meta_path.write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    return sheet_path, meta_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert GIF files to sprite sheet PNG + JSON metadata.",
    )
    parser.add_argument("gif_files", nargs="+", help="One or more GIF file paths.")
    parser.add_argument(
        "--output-dir",
        "-o",
        default="",
        help="Output directory. Defaults to <gif_dir>/sprites.",
    )
    parser.add_argument(
        "--columns",
        "-c",
        type=int,
        default=0,
        help="Frames per row. 0 means auto layout.",
    )
    parser.add_argument(
        "--scale",
        type=float,
        default=1.0,
        help="Scale factor applied to each frame (for example: 1.5).",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.scale <= 0:
        raise SystemExit("--scale must be > 0")
    if args.columns < 0:
        raise SystemExit("--columns must be >= 0")

    converted = 0
    for input_path in args.gif_files:
        gif_path = Path(input_path).resolve()
        if not gif_path.exists():
            print(f"Skip missing file: {gif_path}")
            continue
        if gif_path.suffix.lower() != ".gif":
            print(f"Skip non-gif file: {gif_path}")
            continue

        if args.output_dir:
            output_dir = Path(args.output_dir).resolve()
        else:
            output_dir = gif_path.parent / "sprites"

        try:
            sheet_path, meta_path = gif_to_spritesheet(
                gif_path,
                output_dir,
                columns=args.columns,
                scale=args.scale,
            )
            converted += 1
            print(f"OK: {gif_path.name} -> {sheet_path.name}, {meta_path.name}")
        except Exception as exc:
            print(f"Failed: {gif_path} ({exc})")

    print(f"Done. Converted {converted}/{len(args.gif_files)} file(s).")


if __name__ == "__main__":
    main()
