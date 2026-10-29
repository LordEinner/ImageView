#!/usr/bin/env bash
# Installs ImageView for the current user, unpacked for faster startup than
# running the AppImage directly, with a Graphics menu entry and icon.
# Run ./build_appimage.sh first.
set -euo pipefail
cd "$(dirname "$0")"

DEST="$HOME/.local/opt/imageview"
APPS="$HOME/.local/share/applications"
ICONS="$HOME/.local/share/icons/hicolor/scalable/apps"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
(cd "$tmp" && "$OLDPWD/ImageView-x86_64.AppImage" --appimage-extract >/dev/null)
rm -rf "$DEST"
mkdir -p "$(dirname "$DEST")" "$APPS" "$ICONS"
mv "$tmp/squashfs-root" "$DEST"

cp imageview/icon.svg "$ICONS/imageview.svg"
sed "s|^Exec=.*|Exec=$DEST/AppRun %F|" packaging/imageview.desktop > "$APPS/imageview.desktop"
update-desktop-database "$APPS" 2>/dev/null || true
command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 >/dev/null 2>&1 || true
echo "Installed to $DEST"
