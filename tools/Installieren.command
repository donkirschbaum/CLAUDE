#!/bin/bash
# Installiert Shaker Lab (VST3 + AU) für den aktuellen Benutzer und entfernt die Download-Quarantäne.
cd "$(dirname "$0")" || exit 1
VST3="$HOME/Library/Audio/Plug-Ins/VST3"
AU="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3" "$AU"

rm -rf "$VST3/Shaker Lab.vst3" "$AU/Shaker Lab.component"
cp -R "Shaker Lab.vst3" "$VST3/"
cp -R "Shaker Lab.component" "$AU/"
xattr -dr com.apple.quarantine "$VST3/Shaker Lab.vst3" "$AU/Shaker Lab.component" 2>/dev/null
[ -d "Shaker Lab.app" ] && xattr -dr com.apple.quarantine "Shaker Lab.app" 2>/dev/null

# AU-Cache auffrischen
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Shaker Lab ist installiert:"
echo "  VST3: $VST3/Shaker Lab.vst3"
echo "  AU:   $AU/Shaker Lab.component"
echo ""
echo "Ableton neu starten. Falls das Plugin nicht auftaucht: Einstellungen → Plug-Ins →"
echo "'Plug-In-Quellen' auf VST3 bzw. Audio Units prüfen und 'Neu scannen' klicken."
echo ""
read -n 1 -s -r -p "Taste drücken zum Schließen …"
