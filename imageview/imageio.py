"""Image loading and saving: Qt codecs first, Pillow as a fallback."""

import os
from dataclasses import dataclass
from functools import cache

from PySide6.QtCore import Qt
from PySide6.QtGui import QImage, QImageReader, QImageWriter, QMovie, QPainter

# Extensions Qt can't read but Pillow / pillow-heif can. Listed statically so
# scanning a folder doesn't have to import Pillow, which slows down startup.
_PIL_EXTENSIONS = {
    ".avif", ".avifs", ".heic", ".heics", ".heif", ".heifs", ".hif", ".psd",
    ".jp2", ".j2k", ".j2c", ".jpc", ".jpf", ".jpx", ".qoi", ".dds", ".pcx",
    ".sgi", ".rgb", ".rgba", ".bw", ".ras", ".dib", ".pnm", ".pfm", ".apng",
    ".mpo", ".jpe", ".dcx", ".blp", ".msp",
}

# (filter name, extensions) offered in the Save As dialog, in display order.
_SAVE_CHOICES = [
    ("PNG", ["png"]),
    ("JPEG", ["jpg", "jpeg"]),
    ("WebP", ["webp"]),
    ("AVIF", ["avif"]),
    ("HEIC", ["heic"]),
    ("TIFF", ["tif", "tiff"]),
    ("BMP", ["bmp"]),
    ("GIF", ["gif"]),
    ("ICO", ["ico"]),
    ("TGA", ["tga"]),
    ("QOI", ["qoi"]),
    ("PPM", ["ppm"]),
]

LOSSY_EXTENSIONS = {"jpg", "jpeg", "webp", "avif", "heic"}

# Standard IJG luminance quantization table (quality 50), natural order.
_IJG_LUMA = [
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99,
]


@cache
def _pil():
    """Import and initialize Pillow on first use."""
    from PIL import Image

    try:
        import pillow_heif

        pillow_heif.register_heif_opener()
    except ImportError:
        pass
    Image.init()
    return Image


class LoadError(Exception):
    pass


@dataclass
class Loaded:
    image: QImage | None = None
    movie: QMovie | None = None
    format: str = ""


def _names(formats):
    return {bytes(f).decode().lower() for f in formats}


@cache
def _qt_writable():
    return _names(QImageWriter.supportedImageFormats())


@cache
def readable_extensions():
    exts = {"." + f for f in _names(QImageReader.supportedImageFormats())}
    return frozenset(exts | _PIL_EXTENSIONS)


def _pil_save_format(ext):
    Image = _pil()
    fmt = Image.registered_extensions().get("." + ext)
    return fmt if fmt in Image.SAVE else None


def _can_write(ext):
    return ext in _qt_writable() or _pil_save_format(ext) is not None


@cache
def save_filters():
    """Return a list of (filter string, default extension) for writable formats."""
    filters = []
    for name, exts in _SAVE_CHOICES:
        exts = [e for e in exts if _can_write(e)]
        if exts:
            filters.append((f"{name} ({' '.join('*.' + e for e in exts)})", exts[0]))
    return filters


def can_save(ext):
    return _can_write(ext.lower().lstrip("."))


def pil_to_qimage(pil):
    from PIL import ImageOps

    pil = ImageOps.exif_transpose(pil)
    if pil.mode != "RGBA":
        pil = pil.convert("RGBA")
    data = pil.tobytes("raw", "RGBA")
    w, h = pil.size
    return QImage(data, w, h, 4 * w, QImage.Format.Format_RGBA8888).copy()


def qimage_to_pil(img):
    has_alpha = img.hasAlphaChannel()
    img = img.convertToFormat(QImage.Format.Format_RGBA8888)
    data = bytes(img.constBits())[: img.sizeInBytes()]
    pil = _pil().frombuffer("RGBA", (img.width(), img.height()), data, "raw", "RGBA", img.bytesPerLine(), 1)
    return pil if has_alpha else pil.convert("RGB")


def load(path):
    """Load a file as a static QImage or, for animations, a QMovie."""
    reader = QImageReader(path)
    reader.setAutoTransform(True)
    reader.setDecideFormatFromContent(True)
    error = ""
    if reader.canRead():
        fmt = bytes(reader.format()).decode().upper()
        if reader.supportsAnimation() and reader.imageCount() != 1:
            movie = QMovie(path)
            if movie.isValid() and movie.frameCount() != 1:
                return Loaded(movie=movie, format=fmt)
        image = reader.read()
        if not image.isNull():
            return Loaded(image=image, format=fmt)
        error = reader.errorString()
    try:
        with _pil().open(path) as pil:
            pil.load()
            return Loaded(image=pil_to_qimage(pil), format=pil.format or "")
    except Exception as e:
        raise LoadError(error or str(e)) from e


def estimate_quality(path):
    """Best guess of the quality a file was saved with, or None if unknown.

    JPEG quality is derived from the luminance quantization table, inverting
    the IJG scaling formula. Lossless WebP maps to 100 (Qt's lossless setting).
    Other formats don't record their quality.
    """
    try:
        with _pil().open(path) as pil:
            if pil.format == "JPEG" and pil.quantization:
                table = pil.quantization.get(0) or next(iter(pil.quantization.values()))
                if max(table) == 1:
                    return 100
                scale = sum(table) * 100 / sum(_IJG_LUMA)
                quality = (200 - scale) / 2 if scale <= 100 else 5000 / scale
                return min(100, max(1, round(quality)))
            if pil.format == "WEBP":
                with open(path, "rb") as f:
                    if f.read(16)[12:16] == b"VP8L":
                        return 100
    except Exception:
        pass
    return None


def _flatten(img):
    """Composite onto white, for formats without alpha support."""
    if not img.hasAlphaChannel():
        return img
    out = QImage(img.size(), QImage.Format.Format_RGB32)
    out.fill(Qt.GlobalColor.white)
    painter = QPainter(out)
    painter.drawImage(0, 0, img)
    painter.end()
    return out


def save(img, path, quality=90):
    ext = os.path.splitext(path)[1].lower().lstrip(".")
    if ext in ("jpg", "jpeg"):
        img = _flatten(img)
    if ext in _qt_writable():
        writer = QImageWriter(path, ext.encode())
        writer.setQuality(quality)
        if writer.write(img):
            return
        if _pil_save_format(ext) is None:
            raise OSError(writer.errorString())
    fmt = _pil_save_format(ext)
    if fmt is None:
        raise OSError(f"Unsupported format: .{ext}")
    pil = qimage_to_pil(img)
    try:
        pil.save(path, format=fmt, quality=quality)
    except OSError:
        # Format can't store this mode (e.g. alpha); retry flattened.
        qimage_to_pil(_flatten(img)).save(path, format=fmt, quality=quality)
