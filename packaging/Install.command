#!/bin/bash
# LoopTape – Installation fuer den aktuellen Benutzer (VST3 + Audio Unit)
cd "$(dirname "$0")" || exit 1
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"
rm -rf "$VST3/LoopTape.vst3" "$AU/LoopTape.component"
cp -R LoopTape.vst3 "$VST3/"
cp -R LoopTape.component "$AU/"
xattr -dr com.apple.quarantine "$VST3/LoopTape.vst3" "$AU/LoopTape.component" 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo ""
echo "LoopTape installiert:"
echo "  $VST3/LoopTape.vst3"
echo "  $AU/LoopTape.component"
echo ""
echo "Ableton neu starten (bzw. Einstellungen > Plug-Ins > Neu scannen)."
read -n 1 -s -r -p "Taste druecken zum Schliessen..."
