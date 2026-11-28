#!/bin/bash
# PulseForge installer – copies the plugins to your user plug-in folders
# and removes the macOS quarantine flag (the plugin is not notarized).
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"

AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU_DIR" "$VST3_DIR"

rm -rf "$AU_DIR/PulseForge.component" "$VST3_DIR/PulseForge.vst3"
cp -R "$DIR/PulseForge.component" "$AU_DIR/"
cp -R "$DIR/PulseForge.vst3" "$VST3_DIR/"

xattr -dr com.apple.quarantine "$AU_DIR/PulseForge.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "$VST3_DIR/PulseForge.vst3" 2>/dev/null || true

# make macOS rescan Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "PulseForge wurde installiert:"
echo "  AU:   $AU_DIR/PulseForge.component"
echo "  VST3: $VST3_DIR/PulseForge.vst3"
echo ""
echo "Ableton Live neu starten. Unter Audio-Effekte > Plug-ins > donkirschbaum > PulseForge."
echo "(Falls nicht sichtbar: Einstellungen > Plug-ins > 'Neu scannen' bzw. 'Audio Units verwenden' aktivieren.)"
