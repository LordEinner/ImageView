"""Widget that displays an image with zoom, panning and rectangular selection."""

from PySide6.QtCore import QEvent, QPoint, QPointF, QRect, QRectF, QSizeF, Qt, QTimer, Signal
from PySide6.QtGui import QColor, QPainter, QPainterPath, QPen, QPixmap
from PySide6.QtWidgets import QApplication, QWidget

MIN_ZOOM = 0.01
MAX_ZOOM = 64.0
# Above this many pixels the downscaled pixmap cache is skipped.
MAX_CACHE_PIXELS = 50_000_000
CURSOR_HIDE_MS = 1500


class ImageCanvas(QWidget):
    viewChanged = Signal()
    doubleClicked = Signal()
    contextMenuRequested = Signal(QPoint)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.FocusPolicy.StrongFocus)
        self.setMouseTracking(True)
        self._image = None
        self._movie = None
        self._message = ""
        self._fullscreen = False
        # Zoom is image pixels -> device pixels, so 1.0 is a true 1:1 on HiDPI too.
        self._zoom = 1.0
        self._auto = True
        self._offset = QPointF()
        self._selection = QRect()
        self._sel_start = None
        self._pan_start = None
        self._pan_offset = QPointF()
        self._pan_button = None
        self._panned = False
        self._cache = None
        self._cache_key = None
        self._cursor_timer = QTimer(self, singleShot=True, interval=CURSOR_HIDE_MS)
        self._cursor_timer.timeout.connect(lambda: self.setCursor(Qt.CursorShape.BlankCursor))

    # --- content -----------------------------------------------------------

    def image(self):
        return self._image

    def is_animated(self):
        return self._movie is not None

    def set_image(self, image):
        self._stop_movie()
        self._image = image
        self._message = ""
        self._reset()

    def set_movie(self, movie):
        self._stop_movie()
        self._movie = movie
        movie.setParent(self)
        movie.frameChanged.connect(self._on_frame)
        movie.jumpToFrame(0)
        self._image = movie.currentImage()
        self._message = ""
        self._reset()
        movie.start()

    def clear(self, message=""):
        self._stop_movie()
        self._image = None
        self._message = message
        self._reset()

    def _stop_movie(self):
        if self._movie is not None:
            self._movie.stop()
            self._movie.frameChanged.disconnect(self._on_frame)
            self._movie.deleteLater()
            self._movie = None

    def _on_frame(self, _):
        self._image = self._movie.currentImage()
        self.update()

    def _reset(self):
        self._selection = QRect()
        self._sel_start = None
        self._cache = None
        self._cache_key = None
        self.reset_zoom()

    # --- view --------------------------------------------------------------

    def zoom(self):
        return self._zoom

    def selection(self):
        return QRect(self._selection)

    def set_fullscreen(self, fullscreen):
        self._fullscreen = fullscreen
        if fullscreen:
            self._cursor_timer.start()
        else:
            self._cursor_timer.stop()
            self.unsetCursor()
        self.reset_zoom()

    def reset_zoom(self):
        self._auto = True
        self._apply_auto()
        self.update()
        self.viewChanged.emit()

    def zoom_by(self, factor, anchor=None):
        if self._image is None:
            return
        if anchor is None:
            anchor = QPointF(self.width() / 2, self.height() / 2)
        image_pt = (anchor - self._offset) / self._scale()
        self._zoom = min(MAX_ZOOM, max(MIN_ZOOM, self._zoom * factor))
        self._auto = False
        self._offset = anchor - image_pt * self._scale()
        self._clamp_offset()
        self.update()
        self.viewChanged.emit()

    def set_zoom(self, zoom, anchor=None):
        self.zoom_by(zoom / self._zoom, anchor)

    def _scale(self):
        """Logical widget pixels per image pixel."""
        return self._zoom / self.devicePixelRatioF()

    def _apply_auto(self):
        if self._image is None or self._image.isNull():
            return
        dpr = self.devicePixelRatioF()
        fit = min(self.width() * dpr / self._image.width(), self.height() * dpr / self._image.height())
        self._zoom = fit if self._fullscreen else min(1.0, fit)
        self._clamp_offset()

    def _clamp_offset(self):
        if self._image is None:
            return
        s = self._scale()
        x, y = self._offset.x(), self._offset.y()
        w, h = self._image.width() * s, self._image.height() * s
        x = (self.width() - w) / 2 if w <= self.width() else min(0, max(self.width() - w, x))
        y = (self.height() - h) / 2 if h <= self.height() else min(0, max(self.height() - h, y))
        self._offset = QPointF(x, y)

    def _to_image(self, pos):
        p = (pos - self._offset) / self._scale()
        x = min(self._image.width(), max(0, round(p.x())))
        y = min(self._image.height(), max(0, round(p.y())))
        return QPoint(x, y)

    # --- painting ----------------------------------------------------------

    def paintEvent(self, _):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(0, 0, 0) if self._fullscreen else QColor(40, 40, 40))
        if self._image is None or self._image.isNull():
            p.setPen(QColor(170, 170, 170))
            p.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, self._message)
            return
        img = self._image
        s = self._scale()
        target = QRectF(self._offset, QSizeF(img.width() * s, img.height() * s))
        scaled_w, scaled_h = round(img.width() * self._zoom), round(img.height() * self._zoom)
        if self._zoom < 1 and self._movie is None and scaled_w * scaled_h <= MAX_CACHE_PIXELS:
            dpr = self.devicePixelRatioF()
            key = (img.cacheKey(), scaled_w, scaled_h, dpr)
            if key != self._cache_key:
                scaled = img.scaled(max(1, scaled_w), max(1, scaled_h),
                                    Qt.AspectRatioMode.IgnoreAspectRatio,
                                    Qt.TransformationMode.SmoothTransformation)
                self._cache = QPixmap.fromImage(scaled)
                self._cache.setDevicePixelRatio(dpr)
                self._cache_key = key
            p.drawPixmap(target.topLeft(), self._cache)
        else:
            visible = QRectF(self.rect()).intersected(target)
            source = QRectF((visible.topLeft() - self._offset) / s, visible.size() / s)
            p.setRenderHint(QPainter.RenderHint.SmoothPixmapTransform, self._zoom < 4)
            p.drawImage(visible, img, source)
        if not self._selection.isEmpty():
            self._paint_selection(p, target, s)

    def _paint_selection(self, p, target, s):
        sel = self._selection
        r = QRectF(self._offset + QPointF(sel.topLeft()) * s, QSizeF(sel.size()) * s)
        shade = QPainterPath()
        shade.addRect(target)
        shade.addRect(r)
        p.fillPath(shade, QColor(0, 0, 0, 110))
        p.setRenderHint(QPainter.RenderHint.Antialiasing, False)
        p.setPen(QPen(QColor(0, 0, 0), 1))
        p.drawRect(r)
        dash = QPen(QColor(255, 255, 255), 1, Qt.PenStyle.DashLine)
        p.setPen(dash)
        p.drawRect(r)

    # --- input -------------------------------------------------------------

    def resizeEvent(self, _):
        if self._auto:
            self._apply_auto()
        else:
            self._clamp_offset()
        self.viewChanged.emit()

    def wheelEvent(self, e):
        delta = e.angleDelta().y()
        if delta:
            self.zoom_by(1.25 ** (delta / 120), e.position())

    def event(self, e):
        # Touchpad pinch (Wayland pointer gestures / X11 XInput 2.4).
        if (e.type() == QEvent.Type.NativeGesture
                and e.gestureType() == Qt.NativeGestureType.ZoomNativeGesture):
            self.zoom_by(1 + e.value(), e.position())
            return True
        return super().event(e)

    def mousePressEvent(self, e):
        if e.button() in (Qt.MouseButton.RightButton, Qt.MouseButton.MiddleButton):
            # Becomes a pan once the mouse moves; a right click without moving opens the menu.
            self._pan_start = e.position()
            self._pan_offset = QPointF(self._offset)
            self._pan_button = e.button()
            self._panned = False
        elif e.button() == Qt.MouseButton.LeftButton and self._image is not None:
            self._sel_start = self._to_image(e.position())
            self._selection = QRect()
            self.update()
            self.viewChanged.emit()

    def mouseMoveEvent(self, e):
        if self._fullscreen and self._pan_start is None:
            self.unsetCursor()
            self._cursor_timer.start()
        if self._image is None:
            return
        if self._sel_start is not None:
            a, b = self._sel_start, self._to_image(e.position())
            self._selection = QRect(min(a.x(), b.x()), min(a.y(), b.y()), abs(a.x() - b.x()), abs(a.y() - b.y()))
            self.update()
            self.viewChanged.emit()
        elif self._pan_start is not None:
            delta = e.position() - self._pan_start
            if not self._panned:
                if delta.manhattanLength() < QApplication.startDragDistance():
                    return
                self._panned = True
                self.setCursor(Qt.CursorShape.ClosedHandCursor)
            self._offset = self._pan_offset + delta
            self._clamp_offset()
            self.update()

    def mouseReleaseEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton and self._sel_start is not None:
            self._sel_start = None
            if self._selection.width() < 2 or self._selection.height() < 2:
                self._selection = QRect()
                self.update()
                self.viewChanged.emit()
        elif e.button() == self._pan_button:
            if e.button() == Qt.MouseButton.RightButton and not self._panned:
                self.contextMenuRequested.emit(e.globalPosition().toPoint())
            self._pan_start = None
            self._pan_button = None
            self.unsetCursor()

    def mouseDoubleClickEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton:
            self.doubleClicked.emit()
        else:
            super().mouseDoubleClickEvent(e)
