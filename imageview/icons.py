"""Toolbar icons: simple 24×24 line drawings, tinted to the palette's text colors."""

from PySide6.QtCore import QByteArray, QRectF, Qt
from PySide6.QtGui import QColor, QIcon, QPainter, QPalette, QPixmap
from PySide6.QtSvg import QSvgRenderer

_FLOPPY = '<path d="M5 3h11l5 5v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z"/><path d="M7 3v5h8V3"/>'
_LENS = '<circle cx="10.5" cy="10.5" r="7"/><path d="M21 21l-5.5-5.5"/>'

_SHAPES = {
    "open": '<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
    "save": _FLOPPY + '<path d="M7 21v-7h10v7"/>',
    "save-as": '<path d="M12 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v3"/><path d="M7 3v5h8V3"/>'
                '<path d="M7 21v-7h5"/><path d="M14 22v-2.5l5.5-5.5 2.5 2.5-5.5 5.5z"/>',
    "previous": '<path d="M19 12H5"/><path d="M12 19l-7-7 7-7"/>',
    "next": '<path d="M5 12h14"/><path d="M12 5l7 7-7 7"/>',
    "copy": '<rect x="9" y="9" width="12" height="12" rx="2"/>'
            '<path d="M5 15H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v1"/>',
    "crop": '<path d="M6 2v14a2 2 0 0 0 2 2h14"/><path d="M18 22V8a2 2 0 0 0-2-2H2"/>',
    "resize": '<path d="M15 3h6v6"/><path d="M9 21H3v-6"/><path d="M21 3l-7 7"/><path d="M3 21l7-7"/>',
    "rotate-left": '<path d="M3 12a9 9 0 1 0 2.6-6.4L3 8"/><path d="M3 3v5h5"/>',
    "rotate-right": '<path d="M21 12a9 9 0 1 1-2.6-6.4L21 8"/><path d="M21 3v5h-5"/>',
    "flip-horizontal": '<path d="M12 2v2M12 7v3M12 13v3M12 19v3"/><path d="M9 6L3 18h6z"/><path d="M15 6l6 12h-6z"/>',
    "flip-vertical": '<path d="M2 12h2M7 12h3M13 12h3M19 12h3"/><path d="M6 9L18 3v6z"/><path d="M6 15l12 6v-6z"/>',
    "zoom-in": _LENS + '<path d="M10.5 7.5v6M7.5 10.5h6"/>',
    "zoom-out": _LENS + '<path d="M7.5 10.5h6"/>',
    "zoom-reset": _LENS + '<rect x="8" y="8" width="5" height="5" rx="0.5"/>',
    "fullscreen": '<path d="M8 3H5a2 2 0 0 0-2 2v3"/><path d="M21 8V5a2 2 0 0 0-2-2h-3"/>'
                  '<path d="M3 16v3a2 2 0 0 0 2 2h3"/><path d="M16 21h3a2 2 0 0 0 2-2v-3"/>',
    "exit": '<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><path d="M16 17l5-5-5-5"/><path d="M21 12H9"/>',
}


def _render(name, color, size):
    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="{color.name()}" '
           f'stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">{_SHAPES[name]}</svg>')
    pixmap = QPixmap(size, size)
    pixmap.fill(Qt.GlobalColor.transparent)
    painter = QPainter(pixmap)
    QSvgRenderer(QByteArray(svg.encode())).render(painter, QRectF(0, 0, size, size))
    painter.end()
    return pixmap


def icon(name, palette):
    normal = palette.color(QPalette.ColorGroup.Active, QPalette.ColorRole.WindowText)
    disabled = palette.color(QPalette.ColorGroup.Disabled, QPalette.ColorRole.WindowText)
    if abs(disabled.lightness() - normal.lightness()) < 80:
        # Theme's disabled color is too close to normal; blend toward the background.
        bg = palette.color(QPalette.ColorRole.Window)
        disabled = QColor(*(round(n * 0.4 + b * 0.6) for n, b in
                            zip(normal.getRgb()[:3], bg.getRgb()[:3])))
    result = QIcon()
    for size in (22, 44):  # toolbar size, and 2× for HiDPI
        result.addPixmap(_render(name, normal, size), QIcon.Mode.Normal)
        result.addPixmap(_render(name, disabled, size), QIcon.Mode.Disabled)
    return result
