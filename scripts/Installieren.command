#!/bin/bash
# PolyRhythm32 – installiert AU + VST3 in deinen Benutzer-Plugin-Ordner
cd "$(dirname "$0")"
set -e
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/PolyRhythm32.vst3" "$AU/PolyRhythm32.component"
cp -R "PolyRhythm32.vst3" "$VST3/"
cp -R "PolyRhythm32.component" "$AU/"
# Download-Sperre (Quarantäne) entfernen, sonst blockiert macOS das unsignierte Plugin
xattr -dr com.apple.quarantine "$VST3/PolyRhythm32.vst3" "$AU/PolyRhythm32.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "PolyRhythm32.app" 2>/dev/null || true
# AU-Cache auffrischen
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo ""
echo "PolyRhythm32 ist installiert:"
echo "  VST3: $VST3/PolyRhythm32.vst3"
echo "  AU:   $AU/PolyRhythm32.component"
echo ""
echo "DAW neu starten. In Ableton unter Einstellungen > Plug-Ins ggf."
echo "'VST3 System-Ordner' bzw. 'Audio Units' aktivieren und 'Neu scannen' klicken."
