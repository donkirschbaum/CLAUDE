#!/bin/bash
# Packt AU, VST3, Standalone und Installer in dist/PolyRhythm32-macOS.zip
set -euo pipefail
ART="build/PolyRhythm32_artefacts/Release"
rm -rf dist && mkdir -p dist/PolyRhythm32
cp -R "$ART/VST3/PolyRhythm32.vst3" dist/PolyRhythm32/
cp -R "$ART/AU/PolyRhythm32.component" dist/PolyRhythm32/
cp -R "$ART/Standalone/PolyRhythm32.app" dist/PolyRhythm32/
# Ad-hoc-Signatur (nötig, damit Apple-Silicon-Macs das Plugin laden)
for b in dist/PolyRhythm32/PolyRhythm32.vst3 dist/PolyRhythm32/PolyRhythm32.component dist/PolyRhythm32/PolyRhythm32.app; do
  codesign --force --deep --sign - "$b"
done
cp scripts/Installieren.command dist/PolyRhythm32/
chmod +x dist/PolyRhythm32/Installieren.command
cp scripts/LIESMICH.txt dist/PolyRhythm32/
lipo -info dist/PolyRhythm32/PolyRhythm32.vst3/Contents/MacOS/PolyRhythm32 || true
cd dist && ditto -c -k --sequesterRsrc --keepParent PolyRhythm32 PolyRhythm32-macOS.zip
ls -la
