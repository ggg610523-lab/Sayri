#!/usr/bin/env bash
# Install the Sayri GNOME extension into the user's
# extensions directory and reload GNOME Shell.
#
# Usage: ./install.sh          (copy + reload + enable)
#        ./install.sh --copy   (copy only, no reload)

set -uo pipefail

cd "$(dirname "$0")"

UUID="sayri@pulsar.tools"
DEST="${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/$UUID"

echo "Installing Sayri extension to: $DEST"

mkdir -p "$DEST"
cp metadata.json "$DEST/"
cp extension.js  "$DEST/"
cp orb.js        "$DEST/"
cp glyph.png     "$DEST/"
cp stylesheet.css "$DEST/"

if [[ "${1:-}" == "--copy" ]]; then
    echo "Copied files (skipping shell reload)."
    exit 0
fi

# On X11 we can soft-restart the shell; on Wayland a logout/login
# is required before the new extension is even scanned, so `enable`
# right now fails with "does not exist". Handle both cleanly.
if [[ "${WAYLAND_DISPLAY:-}" != "" ]]; then
    echo "Wayland session detected."
    echo "Log out and back in so GNOME Shell picks up the new extension, then run:"
    echo "    gnome-extensions enable $UUID"
    echo "(until then 'does not exist' is expected and nothing is wrong)"
    exit 0
fi

echo "Restarting GNOME Shell…"
busctl --user call org.gnome.Shell /org/gnome/Shell \
    org.gnome.Shell Eval s 'Meta.restart("Installing Sayri extension")' \
    2>/dev/null || true
sleep 2

echo "Enabling extension…"
if gnome-extensions enable "$UUID"; then
    echo "Done. Look for the 🙂 Sayri button in the top-right."
else
    echo "Could not enable now. The shell may not have rescanned yet —"
    echo "run: gnome-extensions enable $UUID   (or restart the session)."
fi