#!/usr/bin/env bash
# ==============================================================================
# SplitCommander AppImage Build Script
# Packages SplitCommander into a self-contained, portable .AppImage executable.
# ==============================================================================

set -euo pipefail

# Verzeichnisse definieren
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build-appimage-tmp"
APPDIR="${PROJECT_DIR}/AppDir"
OUTPUT_DIR="${PROJECT_DIR}/dist"

echo "=== 1. Bereite Build-Umgebung vor ==="
rm -rf "${BUILD_DIR}" "${APPDIR}"
mkdir -p "${BUILD_DIR}"
mkdir -p "${OUTPUT_DIR}"

echo "=== 2. Lade linuxdeploy herunter ==="
mkdir -p "${PROJECT_DIR}/tools"
cd "${PROJECT_DIR}/tools"

# Lade linuxdeploy falls nicht vorhanden
if [ ! -f "linuxdeploy-x86_64.AppImage" ]; then
    echo "Lade linuxdeploy herunter..."
    curl -sLo linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x linuxdeploy-x86_64.AppImage
fi

# Lade linuxdeploy Qt-Plugin falls nicht vorhanden
if [ ! -f "linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
    echo "Lade linuxdeploy-plugin-qt herunter..."
    curl -sLo linuxdeploy-plugin-qt-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
fi

export PATH="${PROJECT_DIR}/tools:${PATH}"

echo "=== 3. Kompiliere SplitCommander ==="
cd "${PROJECT_DIR}"
cmake -B "${BUILD_DIR}" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSC_PLUGIN_GIT=ON \
    -DSC_PLUGIN_PAPERLESS=ON \
    -DSC_PLUGIN_MOUNTISO=ON \
    -DSC_PLUGIN_MAKEFILEACTIONS=ON \
    -G Ninja

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo "=== 4. Installiere in temporäres AppDir ==="
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

echo "=== 5. Erstelle AppImage ==="
# Exportiere Umgebungsvariablen für linuxdeploy-plugin-qt (wichtig für Qt6)
export EXTRA_QT_PLUGINS="platformthemes,styles,imageformats,iconengines"
# Bei Qt6 müssen wir mitteilen, dass das Qt6-Plugin geladen werden soll
export QMAKE="qmake6"
# Verhindert Fehler beim Strippen von Bibliotheken mit neuartigen RELR-Relozierungen auf Fedora
export NO_STRIP=1

# Führe linuxdeploy aus
./tools/linuxdeploy-x86_64.AppImage \
    --appdir "${APPDIR}" \
    --plugin qt \
    --output appimage

# Verschiebe das fertige AppImage in den Ausgabe-Ordner
mv SplitCommander-*.AppImage "${OUTPUT_DIR}/"

echo "=== FERTIG! ==="
echo "Das portable AppImage wurde erfolgreich unter '${OUTPUT_DIR}/' erstellt!"
