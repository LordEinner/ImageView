"""Dialogs for image operations."""

from PySide6.QtCore import QSize
from PySide6.QtWidgets import (
    QCheckBox, QDialog, QDialogButtonBox, QDoubleSpinBox, QFormLayout, QLabel, QSpinBox,
)


class ResizeDialog(QDialog):
    def __init__(self, size, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Resize Image")
        self._orig = size
        self._updating = False

        self.width_box = QSpinBox(minimum=1, maximum=65535, value=size.width(), suffix=" px")
        self.height_box = QSpinBox(minimum=1, maximum=65535, value=size.height(), suffix=" px")
        self.percent_box = QDoubleSpinBox(minimum=0.1, maximum=10000, value=100, decimals=1, suffix=" %")
        self.keep_aspect = QCheckBox("Keep aspect ratio", checked=True)

        self.width_box.valueChanged.connect(self._width_changed)
        self.height_box.valueChanged.connect(self._height_changed)
        self.percent_box.valueChanged.connect(self._percent_changed)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)

        form = QFormLayout(self)
        form.addRow("Original:", QLabel(f"{size.width()} × {size.height()} px"))
        form.addRow("Width:", self.width_box)
        form.addRow("Height:", self.height_box)
        form.addRow("Scale:", self.percent_box)
        form.addRow(self.keep_aspect)
        form.addRow(buttons)

    def new_size(self):
        return QSize(self.width_box.value(), self.height_box.value())

    def _set(self, w=None, h=None, pct=None):
        self._updating = True
        if w is not None:
            self.width_box.setValue(max(1, round(w)))
        if h is not None:
            self.height_box.setValue(max(1, round(h)))
        if pct is not None:
            self.percent_box.setValue(pct)
        self._updating = False

    def _width_changed(self, w):
        if self._updating:
            return
        ratio = w / self._orig.width()
        if self.keep_aspect.isChecked():
            self._set(h=self._orig.height() * ratio, pct=ratio * 100)

    def _height_changed(self, h):
        if self._updating:
            return
        ratio = h / self._orig.height()
        if self.keep_aspect.isChecked():
            self._set(w=self._orig.width() * ratio, pct=ratio * 100)

    def _percent_changed(self, pct):
        if self._updating:
            return
        self._set(w=self._orig.width() * pct / 100, h=self._orig.height() * pct / 100)
