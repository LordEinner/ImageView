"""Main window and application entry point."""

import bisect
import os
import re
import sys

from PySide6.QtCore import QSize, QUrl, Qt
from PySide6.QtGui import QAction, QGuiApplication, QIcon, QImageReader, QKeySequence, QTransform
from PySide6.QtWidgets import (
    QApplication, QFileDialog, QInputDialog, QLabel, QMainWindow, QMenu, QMessageBox, QSizePolicy, QToolBar, QWidget,
)

from . import icons, imageio
from .canvas import ImageCanvas
from .dialogs import ResizeDialog
from .zoomfield import ZoomField

APP_NAME = "ImageView"
ZOOM_STEP = 1.25
DEFAULT_QUALITY = 90


def natural_key(name):
    return [int(t) if t.isdigit() else t for t in re.split(r"(\d+)", name.casefold())]


def list_images(directory):
    exts = imageio.readable_extensions()
    try:
        names = [n for n in os.listdir(directory)
                 if os.path.splitext(n)[1].lower() in exts and os.path.isfile(os.path.join(directory, n))]
    except OSError:
        return []
    return sorted(names, key=natural_key)


def human_size(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024


class MainWindow(QMainWindow):
    def __init__(self, path=None):
        super().__init__()
        self._path = None
        self._files = []
        self._format = ""
        self._modified = False
        self._was_maximized = False
        self._quality = DEFAULT_QUALITY

        self.canvas = ImageCanvas(self)
        self.canvas.viewChanged.connect(self._update_status)
        self.canvas.doubleClicked.connect(self.toggle_fullscreen)
        self.canvas.contextMenuRequested.connect(self._show_context_menu)
        self.setCentralWidget(self.canvas)
        self.canvas.setFocus()

        self._status = QLabel()
        self.statusBar().addWidget(self._status, 1)
        self._build_toolbar()
        self.setAcceptDrops(True)
        # Right-drag pans the image; don't pop up the toolbar context menu.
        self.setContextMenuPolicy(Qt.ContextMenuPolicy.NoContextMenu)

        screen = QGuiApplication.primaryScreen().availableGeometry()
        self.resize(min(1280, int(screen.width() * 0.8)), min(860, int(screen.height() * 0.8)))

        if path:
            self.open_path(path)
        else:
            self.canvas.clear("Open an image with Ctrl+O or drop one here")
            self._update_title()
            self._update_status()

    # --- toolbar -----------------------------------------------------------

    def _build_toolbar(self):
        bar = QToolBar(self, movable=False, floatable=False)
        bar.setIconSize(QSize(22, 22))
        bar.setStyleSheet("QToolBar::separator { background: palette(mid); width: 1px; margin: 3px 9px; }")
        bar.toggleViewAction().setVisible(False)
        self.addToolBar(bar)
        self._toolbar = bar
        self._image_actions = []
        self._actions = {}
        palette = self.palette()

        def action(icon_name, text, slot, shortcut=None, key_hint=None, needs_image=True):
            a = QAction(icons.icon(icon_name, palette), text, self)
            hint = key_hint
            if shortcut:
                a.setShortcut(QKeySequence(shortcut))
                hint = a.shortcut().toString(QKeySequence.SequenceFormat.NativeText)
            a.setToolTip(f"{text} ({hint})" if hint else text)
            a.triggered.connect(slot)
            bar.addAction(a)
            # Keep shortcuts working while the toolbar is hidden in fullscreen.
            self.addAction(a)
            if needs_image:
                self._image_actions.append(a)
            self._actions[icon_name] = a
            return a

        # Keys handled in keyPressEvent get only a tooltip hint, not a real
        # shortcut, so both main-keyboard and numpad variants work.
        action("open", "Open", self.open_dialog, QKeySequence.StandardKey.Open, needs_image=False)
        action("save", "Save", self.save, QKeySequence.StandardKey.Save)
        action("save-as", "Save As", self.save_as, QKeySequence.StandardKey.SaveAs)
        bar.addSeparator()
        action("previous", "Previous Image", lambda: self.navigate(-1), key_hint="Left", needs_image=False)
        action("next", "Next Image", lambda: self.navigate(1), key_hint="Right", needs_image=False)
        bar.addSeparator()
        action("copy", "Copy", self.copy, QKeySequence.StandardKey.Copy)
        self._crop_action = action("crop", "Crop to Selection", self.crop, "Ctrl+Y")
        action("resize", "Resize", self.resize_image, "Ctrl+R")
        bar.addSeparator()
        action("rotate-left", "Rotate Left", lambda: self._edit(lambda img: img.transformed(QTransform().rotate(-90))), "L")
        action("rotate-right", "Rotate Right", lambda: self._edit(lambda img: img.transformed(QTransform().rotate(90))), "R")
        action("flip-horizontal", "Flip Horizontal", lambda: self._edit(lambda img: img.flipped(Qt.Orientation.Horizontal)), "H")
        action("flip-vertical", "Flip Vertical", lambda: self._edit(lambda img: img.flipped(Qt.Orientation.Vertical)), "V")
        bar.addSeparator()
        action("zoom-out", "Zoom Out", lambda: self.canvas.zoom_by(1 / ZOOM_STEP), key_hint="-")
        reset = QAction(icons.icon("zoom-reset", palette), "Reset Zoom", self, toolTip="Reset Zoom (0)")
        reset.triggered.connect(self.canvas.reset_zoom)
        self._zoom_field = ZoomField(reset, self)
        self._zoom_field.zoomRequested.connect(self.canvas.set_zoom)
        self._zoom_field.editingDone.connect(self.canvas.setFocus)
        bar.addWidget(self._zoom_field)
        action("zoom-in", "Zoom In", lambda: self.canvas.zoom_by(ZOOM_STEP), key_hint="+")
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        bar.addWidget(spacer)
        action("fullscreen", "Fullscreen", self.toggle_fullscreen, key_hint="Enter", needs_image=False)

    def _show_context_menu(self, pos):
        menu = QMenu(self)
        menu.setStyleSheet("QMenu::separator { height: 1px; background: palette(mid); margin: 5px 8px; }")
        menu.addAction(self._actions["copy"])
        if not self.canvas.selection().isEmpty():
            menu.addAction(self._actions["crop"])
        menu.addSeparator()
        for name in ("rotate-left", "rotate-right", "flip-horizontal", "flip-vertical"):
            menu.addAction(self._actions[name])
        menu.addSeparator()
        palette = self.palette()
        if self.isFullScreen():
            menu.addAction(icons.icon("fullscreen", palette), "Exit Fullscreen\tEsc", self.toggle_fullscreen)
        else:
            menu.addAction(icons.icon("exit", palette), "Exit\tEsc", self.close)
        menu.exec(pos)

    def _update_actions(self):
        has_image = self.canvas.image() is not None
        for a in self._image_actions:
            a.setEnabled(has_image)
        self._crop_action.setEnabled(has_image and not self.canvas.selection().isEmpty())
        self._zoom_field.setEnabled(has_image)
        self._zoom_field.set_zoom(self.canvas.zoom())

    # --- keyboard ----------------------------------------------------------

    def keyPressEvent(self, e):
        key = e.key()
        if key in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            self.toggle_fullscreen()
        elif key == Qt.Key.Key_Escape:
            if self.isFullScreen():
                self.toggle_fullscreen()
            else:
                self.close()
        elif key in (Qt.Key.Key_Right, Qt.Key.Key_Space, Qt.Key.Key_PageDown):
            self.navigate(1)
        elif key in (Qt.Key.Key_Left, Qt.Key.Key_Backspace, Qt.Key.Key_PageUp):
            self.navigate(-1)
        elif key in (Qt.Key.Key_Plus, Qt.Key.Key_Equal):
            self.canvas.zoom_by(ZOOM_STEP)
        elif key in (Qt.Key.Key_Minus, Qt.Key.Key_Underscore):
            self.canvas.zoom_by(1 / ZOOM_STEP)
        elif key == Qt.Key.Key_0:
            self.canvas.reset_zoom()
        else:
            super().keyPressEvent(e)

    def toggle_fullscreen(self):
        if self.isFullScreen():
            self._toolbar.show()
            self.statusBar().show()
            self.canvas.set_fullscreen(False)
            self.showMaximized() if self._was_maximized else self.showNormal()
        else:
            self._was_maximized = self.isMaximized()
            self._toolbar.hide()
            self.statusBar().hide()
            self.canvas.set_fullscreen(True)
            self.showFullScreen()

    # --- files -------------------------------------------------------------

    def open_path(self, path, files=None):
        self._path = os.path.abspath(path)
        self._files = files if files is not None else list_images(os.path.dirname(self._path))
        self._modified = False
        self._format = ""
        try:
            loaded = imageio.load(self._path)
        except imageio.LoadError as e:
            self.canvas.clear(f"Cannot open {os.path.basename(self._path)}\n\n{e}")
        else:
            self._format = loaded.format
            self._quality = DEFAULT_QUALITY
            if loaded.movie is not None:
                self.canvas.set_movie(loaded.movie)
            else:
                self.canvas.set_image(loaded.image)
        self._update_title()
        self._update_status()

    def navigate(self, step):
        if not self._path:
            return
        directory, name = os.path.split(self._path)
        files = list_images(directory)
        if not files:
            return
        keys = [natural_key(f) for f in files]
        key = natural_key(name)
        i = bisect.bisect_left(keys, key)
        if i < len(files) and files[i] == name:
            i += step
        elif step < 0:
            i -= 1  # current file is gone; bisect already points at the next one
        self.open_path(os.path.join(directory, files[i % len(files)]), files)

    def open_dialog(self):
        exts = " ".join("*" + e for e in sorted(imageio.readable_extensions()))
        start = os.path.dirname(self._path) if self._path else os.path.expanduser("~")
        path, _ = QFileDialog.getOpenFileName(self, "Open Image", start, f"Images ({exts});;All files (*)")
        if path:
            self.open_path(path)

    def save(self):
        img = self.canvas.image()
        if img is None:
            return
        ext = os.path.splitext(self._path)[1].lower().lstrip(".")
        if not imageio.can_save(ext):
            self.save_as()
            return
        msg = f"Overwrite {os.path.basename(self._path)}?"
        if self.canvas.is_animated():
            msg += "\n\nOnly the current frame of the animation will be saved."
        if QMessageBox.question(self, "Save", msg) == QMessageBox.StandardButton.Yes:
            self._write(img, self._path, self._file_quality())

    def save_as(self):
        img = self.canvas.image()
        if img is None:
            return
        filters = imageio.save_filters()
        base, ext = os.path.splitext(self._path)
        ext = ext.lower().lstrip(".")
        if not imageio.can_save(ext):
            ext = "png"
        selected = next((f for f, e in filters if e == ext or f"*.{ext}" in f), filters[0][0])
        path, chosen = QFileDialog.getSaveFileName(
            self, "Save As", f"{base}.{ext}", ";;".join(f for f, _ in filters), selected)
        if not path:
            return
        ext = os.path.splitext(path)[1].lower().lstrip(".")
        if not ext:
            ext = dict(filters).get(chosen, "png")
            path = f"{path}.{ext}"
        quality = self._file_quality()
        if ext in imageio.LOSSY_EXTENSIONS:
            quality, ok = QInputDialog.getInt(self, "Save As", "Quality (1–100):", quality, 1, 100)
            if not ok:
                return
            self._quality = quality
        self._write(img, path, quality)

    def _file_quality(self):
        """Quality of the file on disk, else the last one chosen in Save As."""
        return imageio.estimate_quality(self._path) or self._quality

    def _write(self, img, path, quality):
        ext = os.path.splitext(path)[1].lower().lstrip(".")
        try:
            imageio.save(img, path, quality)
        except Exception as e:
            QMessageBox.critical(self, "Save failed", f"Could not save {path}:\n{e}")
            return
        self._path = path
        self._files = list_images(os.path.dirname(path))
        self._format = ext.upper()
        self._modified = False
        self._update_title()
        self._update_status()
        self.statusBar().showMessage(f"Saved {path}", 3000)

    # --- editing -----------------------------------------------------------

    def _edit(self, transform):
        img = self.canvas.image()
        if img is None:
            return
        self.canvas.set_image(transform(img))
        self._modified = True
        self._update_title()
        self._update_status()

    def crop(self):
        sel = self.canvas.selection()
        if sel.isEmpty():
            self.statusBar().showMessage("Select an area with the mouse first", 3000)
            return
        self._edit(lambda img: img.copy(sel))

    def resize_image(self):
        img = self.canvas.image()
        if img is None:
            return
        dlg = ResizeDialog(img.size(), self)
        if dlg.exec():
            size = dlg.new_size()
            self._edit(lambda img: img.scaled(size, Qt.AspectRatioMode.IgnoreAspectRatio,
                                              Qt.TransformationMode.SmoothTransformation))

    def copy(self):
        img = self.canvas.image()
        if img is None:
            return
        sel = self.canvas.selection()
        if not sel.isEmpty():
            img = img.copy(sel)
        QApplication.clipboard().setImage(img)
        self.statusBar().showMessage(f"Copied {img.width()} × {img.height()} to clipboard", 3000)

    # --- status ------------------------------------------------------------

    def _update_title(self):
        if self._path:
            star = "*" if self._modified else ""
            self.setWindowTitle(f"{star}{os.path.basename(self._path)} — {APP_NAME}")
        else:
            self.setWindowTitle(APP_NAME)

    def _update_status(self):
        self._update_actions()
        if not self._path:
            self._status.setText("")
            return
        name = os.path.basename(self._path)
        parts = [name]
        if name in self._files:
            parts.append(f"{self._files.index(name) + 1} / {len(self._files)}")
        img = self.canvas.image()
        if img is not None:
            parts.append(f"{img.width()} × {img.height()}")
            fmt = self._format + (" (animated)" if self.canvas.is_animated() else "")
            if fmt:
                parts.append(fmt)
            try:
                parts.append(human_size(os.path.getsize(self._path)))
            except OSError:
                pass
            parts.append(f"{self.canvas.zoom() * 100:.0f}%")
            sel = self.canvas.selection()
            if not sel.isEmpty():
                parts.append(f"Selection: {sel.x()}, {sel.y()}  {sel.width()} × {sel.height()}")
        if self._modified:
            parts.append("modified")
        self._status.setText("   |   ".join(parts))

    # --- drag & drop -------------------------------------------------------

    def dragEnterEvent(self, e):
        if e.mimeData().hasUrls():
            e.acceptProposedAction()

    def dropEvent(self, e):
        for url in e.mimeData().urls():
            if url.isLocalFile():
                self.open_path(url.toLocalFile())
                break


def main():
    QImageReader.setAllocationLimit(4096)
    app = QApplication(sys.argv)
    app.setApplicationName(APP_NAME)
    app.setDesktopFileName("imageview")
    icon = os.path.join(os.path.dirname(__file__), "icon.svg")
    if os.path.exists(icon):
        app.setWindowIcon(QIcon(icon))
    path = None
    if len(sys.argv) > 1:
        arg = sys.argv[1]
        path = QUrl(arg).toLocalFile() if arg.startswith("file://") else arg
    win = MainWindow(path)
    win.show()
    sys.exit(app.exec())
