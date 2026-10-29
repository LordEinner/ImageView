# ImageView

A simple, fast image viewer for Linux in the spirit of IrfanView, packaged as an AppImage.
Opens JPEG, PNG, GIF and WebP (including animations), AVIF, HEIC, TIFF, BMP, PSD and more.
Can crop, resize, rotate, flip and save to the common formats.

## Build and run

    ./build_appimage.sh                      # needs appimagetool on PATH
    ./ImageView-x86_64.AppImage [image]

From source: `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt && .venv/bin/python run.py [image]`

## Controls

| | |
|---|---|
| Enter / double-click | Toggle fullscreen |
| Esc | Leave fullscreen, or quit |
| Left / Right | Previous / next image in folder (wraps around) |
| Wheel, pinch, + / - / 0 | Zoom in / out / fit (the zoom % field opens a slider; double-click it to type) |
| Left-drag | Select area · right-drag: pan |
| Ctrl+C | Copy selection, or the whole image |
| Ctrl+Y / Ctrl+R | Crop to selection / resize |
| L / R, H / V | Rotate left / right, flip horizontal / vertical |
| Ctrl+O / Ctrl+S / Ctrl+Shift+S | Open / save (asks to confirm) / save as |
