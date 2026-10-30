# ImageView

A simple, fast image viewer for Linux in the spirit of IrfanView, written in C++ with Qt 6 and packaged as an AppImage.
Opens JPEG, PNG, GIF and WebP (including animations), TIFF, BMP and more; AVIF, JPEG XL, PSD, QOI etc.
come from [KImageFormats](https://invent.kde.org/frameworks/kimageformats) when it is installed at build time.
Can crop, resize, rotate and flip (with undo) and save to the common formats, including AVIF and JPEG XL.

## Build and run

Needs CMake, a C++20 compiler and the Qt 6 Widgets and Svg development files
(Fedora: `sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtsvg-devel kf6-kimageformats`).

    ./build_appimage.sh                      # needs appimagetool on PATH
    ./ImageView-x86_64.AppImage [image]
    ./install_local.sh                       # optional: install unpacked + menu entry

From source: `cmake -S . -B build -G Ninja && cmake --build build && build/imageview [image]`

## Controls

| | |
|---|---|
| Enter / double-click | Toggle fullscreen |
| Esc | Leave fullscreen, or quit |
| Left / Right | Previous / next image in folder (wraps around) |
| Wheel, pinch, + / - / 0 | Zoom in / out / fit (the zoom % field opens a slider; double-click it to type) |
| Left-drag / right-drag | Select area / pan · right-click: menu |
| Ctrl+C | Copy selection, or the whole image |
| Ctrl+Y / Ctrl+R | Crop to selection / resize |
| Ctrl+Z | Undo the last crop, resize, rotation or flip |
| L / R, H / V | Rotate left / right, flip horizontal / vertical |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / save (asks to confirm) / save as |
