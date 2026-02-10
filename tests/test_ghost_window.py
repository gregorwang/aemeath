from __future__ import annotations

import sys
from pathlib import Path

from PySide6.QtWidgets import QApplication

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT / "src") not in sys.path:
    sys.path.insert(0, str(ROOT / "src"))

from ui.ascii_renderer import AsciiRenderer
from ui.entity_window import EntityWindow


def fallback_html() -> str:
    return (
        '<pre style="font-family: Consolas, \'Courier New\', monospace; font-size: 9px; line-height: 1.0; margin: 0;">'
        '<span style="color: rgb(120,255,230);">   /\\_/\\</span><br/>'
        '<span style="color: rgb(120,255,230);">  ( o.o )</span><br/>'
        '<span style="color: rgb(120,255,230);">   &gt; ^ &lt;</span><br/>'
        '<span style="color: rgb(255,220,190);">ASCII sprite not found: using fallback.</span>'
        "</pre>"
    )


if __name__ == "__main__":
    app = QApplication(sys.argv)
    renderer = AsciiRenderer(width=60)
    sample_image = ROOT / "assets" / "sprites" / "test.png"

    if sample_image.exists():
        html = renderer.render_image(sample_image)
    else:
        html = fallback_html()

    window = EntityWindow()
    window.set_ascii_content(html)
    screen = app.primaryScreen().availableGeometry()
    window.move(screen.x() + screen.width() // 2 - window.width() // 2, screen.y() + screen.height() // 2 - window.height() // 2)
    window.show()
    raise SystemExit(app.exec())

