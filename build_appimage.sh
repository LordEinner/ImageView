#!/usr/bin/env bash
# Builds ImageView-x86_64.AppImage. Requires CMake, a C++20 compiler, the Qt 6
# Widgets/Svg development files and appimagetool on PATH. Install KImageFormats
# (kf6-kimageformats / kimageformats6) first for AVIF, HEIC, JPEG XL, PSD etc.
set -euo pipefail
cd "$(dirname "$0")"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

QT_PLUGINS=$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null || qtpaths --query QT_INSTALL_PLUGINS)
PLUGIN_DIRS=(platforms platformthemes platforminputcontexts imageformats iconengines xcbglintegrations
             wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client)
# Libraries every desktop provides, or that must match the host's drivers.
EXCLUDE='^(ld-linux|libc|libm|libdl|libpthread|librt|libresolv|libutil|libnsl|libanl|libmvec|libstdc\+\+|libgcc_s|libGL|libGLX|libGLdispatch|libEGL|libOpenGL|libgbm|libdrm|libX11|libX11-xcb|libxcb|libxcb-.*|libfontconfig|libfreetype|libharfbuzz|libz|libexpat|libdbus-1|libsystemd|libudev|libwayland-.*)\.so'

rm -rf AppDir
mkdir -p AppDir/usr/bin AppDir/usr/lib AppDir/usr/plugins \
    AppDir/usr/share/applications AppDir/usr/share/icons/hicolor/scalable/apps
cp build/imageview AppDir/usr/bin/
strip AppDir/usr/bin/imageview
printf '[Paths]\nPrefix = ..\n' > AppDir/usr/bin/qt.conf

for dir in "${PLUGIN_DIRS[@]}"; do
    [ -d "$QT_PLUGINS/$dir" ] && cp -r "$QT_PLUGINS/$dir" AppDir/usr/plugins/
done
# The KDE and GTK themes and the PDF "image" plugin drag in large frameworks,
# and these platforms aren't for desktops.
rm -f AppDir/usr/plugins/platformthemes/{KDEPlasmaPlatformTheme*,libqgtk3}.so \
    AppDir/usr/plugins/imageformats/libqpdf.so \
    AppDir/usr/plugins/platforminputcontexts/libqtvirtualkeyboardplugin.so \
    AppDir/usr/plugins/platforms/lib{qeglfs,qlinuxfb,qminimalegl,qvkkhrdisplay,qvnc}.so

find AppDir/usr/bin/imageview AppDir/usr/plugins -type f -name '*' -print0 \
    | xargs -0 ldd 2>/dev/null | awk '$2 == "=>" && $3 ~ /^\// { print $3 }' | sort -u \
    | while read -r lib; do
        if ! basename "$lib" | grep -Eq "$EXCLUDE"; then
            cp -L "$lib" AppDir/usr/lib/
        fi
    done

cp packaging/imageview.desktop AppDir/imageview.desktop
cp packaging/imageview.desktop AppDir/usr/share/applications/
cp src/icon.svg AppDir/imageview.svg
cp src/icon.svg AppDir/usr/share/icons/hicolor/scalable/apps/imageview.svg
ln -sf imageview.svg AppDir/.DirIcon
cat > AppDir/AppRun <<'RUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/imageview" "$@"
RUN
chmod +x AppDir/AppRun

ARCH=x86_64 appimagetool AppDir ImageView-x86_64.AppImage
echo "Built ImageView-x86_64.AppImage"
