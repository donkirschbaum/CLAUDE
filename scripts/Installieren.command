#!/bin/bash
# Grids 8 – installiert AU + VST3 in deinen Benutzer-Plugin-Ordner
cd "$(dirname "$0")"
set -e
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/Grids 8.vst3" "$AU/Grids 8.component"
cp -R "Grids 8.vst3" "$VST3/"
cp -R "Grids 8.component" "$AU/"
# Download-Sperre (Quarantäne) entfernen, sonst blockiert macOS das unsignierte Plugin
xattr -dr com.apple.quarantine "$VST3/Grids 8.vst3" "$AU/Grids 8.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "Grids 8.app" 2>/dev/null || true
# AU-Cache auffrischen
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "Grids 8 ist installiert:"
echo "  VST3: $VST3/Grids 8.vst3"
echo "  AU:   $AU/Grids 8.component"
echo ""
echo "Ableton Live neu starten. Unter Einstellungen > Plug-Ins ggf. 'VST3 Plug-In Custom Folder'"
echo "bzw. 'Audio Units verwenden' aktivieren und 'Neu scannen' klicken."
