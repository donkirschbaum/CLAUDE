#!/bin/bash
# Packt AU, VST3, Standalone, Samples und Installer in dist/Grids8-macOS.zip
set -euo pipefail
ART="build/Grids8_artefacts/Release"
rm -rf dist && mkdir -p dist/Grids8
cp -R "$ART/VST3/Grids 8.vst3" dist/Grids8/
cp -R "$ART/AU/Grids 8.component" dist/Grids8/
cp -R "$ART/Standalone/Grids 8.app" dist/Grids8/
# Ad-hoc-Signatur (nötig, damit Apple-Silicon-Macs das Plugin laden)
for b in "dist/Grids8/Grids 8.vst3" "dist/Grids8/Grids 8.component" "dist/Grids8/Grids 8.app"; do
  codesign --force --deep --sign - "$b"
done
mkdir -p dist/Grids8/Samples
cp Resources/samples/*.wav dist/Grids8/Samples/
cp scripts/Installieren.command dist/Grids8/
chmod +x dist/Grids8/Installieren.command
cp scripts/LIESMICH.txt dist/Grids8/
lipo -info "dist/Grids8/Grids 8.vst3/Contents/MacOS/Grids 8" || true
cd dist && ditto -c -k --sequesterRsrc --keepParent Grids8 Grids8-macOS.zip
ls -la
