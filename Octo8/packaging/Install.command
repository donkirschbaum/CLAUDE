#!/bin/bash
# Octo8 Installer - kopiert die Plugins in deinen Benutzer-Plugin-Ordner
cd "$(dirname "$0")" || exit 1
DEST="$HOME/Library/Audio/Plug-Ins"
mkdir -p "$DEST/VST3" "$DEST/Components"

echo "Installiere Octo8 ..."
rm -rf "$DEST/VST3/Octo8.vst3" "$DEST/Components/Octo8.component"
cp -R "Octo8.vst3" "$DEST/VST3/"
cp -R "Octo8.component" "$DEST/Components/"
xattr -dr com.apple.quarantine "$DEST/VST3/Octo8.vst3" "$DEST/Components/Octo8.component" 2>/dev/null

if [ -d "Octo8.app" ]; then
  rm -rf "/Applications/Octo8.app"
  cp -R "Octo8.app" "/Applications/" && xattr -dr com.apple.quarantine "/Applications/Octo8.app" 2>/dev/null
fi

# Audio-Unit-Cache auffrischen, damit Ableton das AU sofort findet
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Fertig!"
echo "  VST3: $DEST/VST3/Octo8.vst3"
echo "  AU:   $DEST/Components/Octo8.component"
echo ""
echo "Ableton neu starten. In Einstellungen > Plug-Ins ggf. 'VST3 Plug-In Custom Folder'"
echo "bzw. 'Plug-In-Quellen verwenden' aktivieren und 'Neu scannen' klicken."
echo "Octo8 findest du dann unter Plug-Ins > donkirschbaum > Octo8."
