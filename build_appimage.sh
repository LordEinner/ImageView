#!/usr/bin/env bash
# Builds ImageView-x86_64.AppImage. Requires appimagetool on PATH.
set -euo pipefail
cd "$(dirname "$0")"

if [ ! -x .venv/bin/python ]; then
    python3 -m venv .venv
fi
.venv/bin/pip install -q -r requirements.txt pyinstaller

rm -rf build dist AppDir
.venv/bin/pyinstaller --noconfirm --clean --windowed --onedir --name imageview \
    --add-data imageview/icon.svg:imageview \
    --collect-all pillow_heif \
    --exclude-module tkinter \
    run.py

mkdir -p AppDir/usr/lib AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/scalable/apps
cp -r dist/imageview AppDir/usr/lib/imageview
cp packaging/imageview.desktop AppDir/imageview.desktop
cp packaging/imageview.desktop AppDir/usr/share/applications/
cp imageview/icon.svg AppDir/imageview.svg
cp imageview/icon.svg AppDir/usr/share/icons/hicolor/scalable/apps/imageview.svg
ln -sf imageview.svg AppDir/.DirIcon
cat > AppDir/AppRun <<'RUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/lib/imageview/imageview" "$@"
RUN
chmod +x AppDir/AppRun

ARCH=x86_64 appimagetool AppDir ImageView-x86_64.AppImage
echo "Built ImageView-x86_64.AppImage"
