# ImageView

A simple, fast image viewer for Linux and Windows in the spirit of IrfanView, written in C++ with Qt 6,
packaged as an AppImage on Linux and as a self-contained folder / zip on Windows.
Opens JPEG, PNG, GIF and WebP (including animations), TIFF, BMP and more; AVIF, JPEG XL, PSD, QOI etc.
come from [KImageFormats](https://invent.kde.org/frameworks/kimageformats) when it is installed at build time.
Can crop, resize, rotate and flip (with undo) and save to the common formats, including AVIF and JPEG XL.

## Build and run on Windows

Needs Visual Studio 2022 or its Build Tools with the "Desktop development with C++" workload
(includes CMake and Ninja), and Qt 6.9+ for MSVC 2022 64-bit with the Qt Image Formats module
(for WebP and TIFF), e.g. in `C:\Qt\6.11.3\msvc2022_64`. KImageFormats isn't bundled on Windows,
so AVIF, JPEG XL, PSD and QOI aren't available there.

    .\build_windows.ps1                      # finds C:\Qt\<newest>\msvc2022_64, or pass -QtDir
    dist\ImageView\ImageView.exe [image]     # also zipped as ImageView-windows-x64.zip
    .\install_windows.ps1                    # optional: per-user install, Start menu, "Open with"
    .\install_windows.ps1 -Uninstall

The window follows Windows' light/dark app mode (Settings > Personalization > Colors), switching live.

If scripts are blocked, run them with `powershell -ExecutionPolicy Bypass -File .\build_windows.ps1`.

## Build and run on Linux

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
| Left / Right | Previous / next picture in folder (wraps around); steps through JPEG, PNG, GIF, WebP, BMP, TIFF, AVIF and JPEG XL only — other formats (PSD, SVG, ICO, TGA...) open directly but are skipped |
| Delete | Delete the current file (asks first) and show the next one; uses the Recycle Bin / Trash when there is one, otherwise deletes permanently |
| Wheel, pinch, + / - / 0 | Zoom in / out / fit (the zoom % field opens a slider; double-click it to type) |
| Left-drag / right-drag | Select area / pan · right-click: menu |
| Ctrl+C | Copy selection, or the whole image |
| Ctrl+V | Paste an image from the clipboard as a new, unsaved file |
| Ctrl+Y / Ctrl+R | Crop to selection / resize |
| Ctrl+Z | Undo the last crop, resize, rotation or flip |
| L / R, H / V | Rotate left / right, flip horizontal / vertical |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / save (asks to confirm) / save as |
