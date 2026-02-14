from __future__ import annotations

from html import escape
from pathlib import Path

import numpy as np
from PIL import Image, ImageSequence


class AsciiRenderer:
    """Convert images/GIF frames into colorized HTML ASCII blocks."""

    CHARSET: str = "@%#*+=-:. "
    ASPECT_RATIO_CORRECTION: float = 0.55
    BG_THRESHOLD: int = 30
    ALPHA_THRESHOLD: int = 30
    SUPPORTED_IMAGE_SUFFIXES: set[str] = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp"}

    def __init__(self, width: int = 80, font_size_px: int = 8):
        """Initialize renderer with minimum width/font-size safeguards."""
        self._width = max(width, 8)
        self._font_size_px = max(font_size_px, 6)

    @classmethod
    def _validate_asset_path(cls, asset_path: Path | str, *, gif_only: bool = False) -> Path:
        """Validate path existence and supported suffix for image rendering."""
        path = Path(asset_path)
        if not path.exists():
            prefix = "GIF" if gif_only else "Image"
            raise FileNotFoundError(f"{prefix} not found: {path}")
        suffix = path.suffix.lower()
        if gif_only:
            if suffix != ".gif":
                raise ValueError(f"Not a GIF file: {path}")
            return path
        if suffix not in cls.SUPPORTED_IMAGE_SUFFIXES:
            raise ValueError(f"Unsupported image format: {path.suffix}")
        return path

    def render_image(self, image_path: Path | str) -> str:
        """Render an image into HTML-wrapped ASCII art."""
        path = self._validate_asset_path(image_path)
        with Image.open(path) as image:
            frame = self._preprocess_image(image)
            body = self._render_pixels(np.array(frame, dtype=np.uint8))
        return self._wrap_pre(body)

    def render_gif_frames(self, gif_path: Path | str) -> list[str]:
        """Render each GIF frame into one HTML-wrapped ASCII frame."""
        path = self._validate_asset_path(gif_path, gif_only=True)
        frames: list[str] = []
        with Image.open(path) as gif:
            for frame in ImageSequence.Iterator(gif):
                rgba = self._preprocess_image(frame)
                body = self._render_pixels(np.array(rgba, dtype=np.uint8))
                frames.append(self._wrap_pre(body))
        return frames

    def _preprocess_image(self, image: Image.Image) -> Image.Image:
        """Resize while preserving aspect ratio, then convert to RGBA."""
        if image.width <= 0 or image.height <= 0:
            return Image.new("RGBA", (self._width, 1), (0, 0, 0, 0))
        aspect_ratio = image.height / image.width
        target_height = max(1, int(self._width * aspect_ratio * self.ASPECT_RATIO_CORRECTION))
        resized = image.resize((self._width, target_height), Image.Resampling.LANCZOS)
        return resized.convert("RGBA")

    def _render_pixels(self, pixels: np.ndarray) -> str:
        """Convert RGBA pixels to colored HTML spans row by row."""
        if pixels.size == 0:
            return ""
        rows: list[str] = []
        for y in range(pixels.shape[0]):
            row_chars: list[str] = []
            for x in range(pixels.shape[1]):
                r, g, b, a = [int(v) for v in pixels[y, x]]
                if self._is_transparent(r, g, b, a):
                    row_chars.append("&nbsp;")
                    continue
                gray = 0.299 * r + 0.587 * g + 0.114 * b
                char = self._gray_to_char(gray)
                safe_char = "&nbsp;" if char == " " else escape(char)
                row_chars.append(f'<span style="color:rgb({r},{g},{b});">{safe_char}</span>')
            rows.append("".join(row_chars))
        return "<br>".join(rows)

    def _is_transparent(self, r: int, g: int, b: int, a: int) -> bool:
        """Treat low-alpha or near-black pixels as transparent background."""
        if a < self.ALPHA_THRESHOLD:
            return True
        if (r + g + b) < self.BG_THRESHOLD:
            return True
        return False

    def _gray_to_char(self, gray: float) -> str:
        """Map a grayscale value (0-255) to one ASCII character."""
        index = int(gray / 255.0 * (len(self.CHARSET) - 1))
        index = min(max(index, 0), len(self.CHARSET) - 1)
        return self.CHARSET[index]

    def _wrap_pre(self, content: str) -> str:
        """Wrap ASCII HTML in a styled `<pre>` block used by the UI label."""
        return (
            '<pre style="'
            "font-family: Consolas, 'Courier New', monospace; "
            f"font-size: {self._font_size_px}px; "
            "line-height: 1.0; "
            "letter-spacing: 0px; "
            "margin: 0; "
            "padding: 0; "
            "white-space: pre; "
            "background: transparent;"
            '">'
            f"{content}"
            "</pre>"
        )

    def apply_eye_tracking(
        self,
        ascii_html: str,
        face_x: float,
        eye_width: int = 5,
    ) -> str:
        """
        Replace eye placeholders ({EYE_L}/{EYE_R}) according to gaze X.
        """
        if "{EYE_L}" not in ascii_html and "{EYE_R}" not in ascii_html:
            return ascii_html

        width = max(3, eye_width)
        offset = int(max(-1.0, min(1.0, face_x)) * (width // 2))
        eye_pos = max(0, min(width - 1, (width // 2) + offset))
        cells = ["&nbsp;"] * width
        cells[eye_pos] = "O"
        eye = "".join(cells)

        result = ascii_html.replace("{EYE_L}", eye)
        result = result.replace("{EYE_R}", eye)
        return result
