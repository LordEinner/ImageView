"""Toolbar zoom field: shows the zoom level, opens a slider popup on click,
and accepts a typed percentage on double-click."""

import math

from PySide6.QtCore import QEvent, QPoint, QSize, Qt, QTimer, Signal
from PySide6.QtWidgets import (
    QApplication, QFrame, QHBoxLayout, QLineEdit, QSlider, QStyle, QStyleOptionSlider, QToolButton,
)

from .canvas import MAX_ZOOM, MIN_ZOOM


def _to_slider(zoom):
    return round(100 * math.log2(zoom))


def _from_slider(value):
    return 2 ** (value / 100)


class JumpSlider(QSlider):
    """Slider with a large round handle that jumps to wherever it is clicked."""

    STYLE = """
        QSlider { min-height: 26px; }
        QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: palette(mid); }
        QSlider::sub-page:horizontal { border-radius: 3px; background: palette(highlight); }
        QSlider::handle:horizontal {
            width: 20px; height: 20px; margin: -8px 0; border-radius: 11px;
            background: palette(button); border: 1px solid palette(dark);
        }
        QSlider::handle:horizontal:hover { background: palette(light); }
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.setStyleSheet(self.STYLE)

    def mousePressEvent(self, e):
        if e.button() == Qt.MouseButton.LeftButton:
            opt = QStyleOptionSlider()
            self.initStyleOption(opt)
            style = self.style()
            groove = style.subControlRect(QStyle.ComplexControl.CC_Slider, opt, QStyle.SubControl.SC_SliderGroove, self)
            handle = style.subControlRect(QStyle.ComplexControl.CC_Slider, opt, QStyle.SubControl.SC_SliderHandle, self)
            if not handle.contains(e.position().toPoint()):
                pos = round(e.position().x()) - groove.x() - handle.width() // 2
                self.setValue(QStyle.sliderValueFromPosition(
                    self.minimum(), self.maximum(), pos, groove.width() - handle.width()))
        # The handle is now under the cursor, so the base class starts a drag.
        super().mousePressEvent(e)


class ZoomPopup(QFrame):
    zoomRequested = Signal(float)

    def __init__(self, reset_action, parent=None):
        super().__init__(parent, Qt.WindowType.Popup)
        self.setFrameShape(QFrame.Shape.StyledPanel)
        self.slider = JumpSlider(Qt.Orientation.Horizontal, minimum=_to_slider(MIN_ZOOM),
                              maximum=_to_slider(MAX_ZOOM), singleStep=32, pageStep=100)
        self.slider.setFixedWidth(240)
        self.slider.valueChanged.connect(lambda v: self.zoomRequested.emit(_from_slider(v)))
        reset = QToolButton(autoRaise=True)
        reset.setDefaultAction(reset_action)
        reset.setIconSize(QSize(22, 22))
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 4, 4)
        layout.addWidget(self.slider)
        layout.addWidget(reset)

    def set_zoom(self, zoom):
        self.slider.blockSignals(True)
        self.slider.setValue(_to_slider(zoom))
        self.slider.blockSignals(False)

    def keyPressEvent(self, e):
        if e.key() == Qt.Key.Key_Escape:
            self.close()
        else:
            super().keyPressEvent(e)


class ZoomField(QLineEdit):
    zoomRequested = Signal(float)
    editingDone = Signal()

    def __init__(self, reset_action, parent=None):
        super().__init__(parent)
        self.setFixedWidth(68)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setToolTip("Zoom: click for slider, double-click to type a percentage")
        self._zoom = 1.0
        self._editing = False
        self._end_edit()

        self._popup = ZoomPopup(reset_action, self)
        self._popup.zoomRequested.connect(self.zoomRequested)
        # Wait out the double-click interval so a double-click doesn't also open the popup.
        self._click_timer = QTimer(self, singleShot=True, interval=QApplication.doubleClickInterval())
        self._click_timer.timeout.connect(self._show_popup)

    def set_zoom(self, zoom):
        self._zoom = zoom
        self._popup.set_zoom(zoom)
        if not self._editing:
            self.setText(f"{zoom * 100:.0f}%")

    def _show_popup(self):
        self._popup.set_zoom(self._zoom)
        self._popup.adjustSize()
        pos = self.mapToGlobal(QPoint(0, self.height()))
        self._popup.move(pos.x() + (self.width() - self._popup.width()) // 2, pos.y() + 2)
        self._popup.show()

    # --- editing -----------------------------------------------------------

    def _begin_edit(self):
        self._editing = True
        self.setReadOnly(False)
        self.setFocusPolicy(Qt.FocusPolicy.ClickFocus)
        self.setCursor(Qt.CursorShape.IBeamCursor)
        self.setText(f"{self._zoom * 100:.0f}")
        self.setFocus()
        self.selectAll()
        QApplication.instance().installEventFilter(self)

    def _end_edit(self):
        if self._editing:
            QApplication.instance().removeEventFilter(self)
        self._editing = False
        self.setReadOnly(True)
        self.setFocusPolicy(Qt.FocusPolicy.NoFocus)
        self.setCursor(Qt.CursorShape.PointingHandCursor)
        self.deselect()
        self.setText(f"{self._zoom * 100:.0f}%")
        self.editingDone.emit()

    def _commit(self):
        text = self.text().strip().rstrip("%").strip().replace(",", ".")
        self._end_edit()
        try:
            percent = float(text)
        except ValueError:
            return
        if percent > 0 and math.isfinite(percent):
            self.zoomRequested.emit(min(MAX_ZOOM, max(MIN_ZOOM, percent / 100)))

    def eventFilter(self, obj, e):
        # A click anywhere outside the field accepts the typed value.
        if self._editing and e.type() == QEvent.Type.MouseButtonPress:
            if not self.rect().contains(self.mapFromGlobal(e.globalPosition().toPoint())):
                self._commit()
        return False

    # --- events ------------------------------------------------------------

    def mousePressEvent(self, e):
        if self._editing:
            super().mousePressEvent(e)
        elif e.button() == Qt.MouseButton.LeftButton:
            self._click_timer.start()

    def mouseDoubleClickEvent(self, e):
        if self._editing:
            super().mouseDoubleClickEvent(e)
        elif e.button() == Qt.MouseButton.LeftButton:
            self._click_timer.stop()
            self._begin_edit()

    def keyPressEvent(self, e):
        if not self._editing:
            return super().keyPressEvent(e)
        if e.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            self._commit()
        elif e.key() == Qt.Key.Key_Escape:
            self._end_edit()
        else:
            super().keyPressEvent(e)

    def focusOutEvent(self, e):
        super().focusOutEvent(e)
        if self._editing:
            self._commit()
