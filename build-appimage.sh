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

# Lade appimagetool falls nicht vorhanden
if [ ! -f "appimagetool-x86_64.AppImage" ]; then
    echo "Lade appimagetool herunter..."
    curl -sLo appimagetool-x86_64.AppImage https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
    chmod +x appimagetool-x86_64.AppImage
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
# Erstelle das Zielverzeichnis für Übersetzungen, damit linuxdeploy-plugin-qt Symlinks ohne Fehler anlegen kann
mkdir -p "${APPDIR}/usr/translations"

echo "=== 5. Bereite AppDir mit linuxdeploy vor ==="
# Exportiere Umgebungsvariablen für linuxdeploy-plugin-qt (wichtig für Qt6)
export EXTRA_QT_PLUGINS="platformthemes,styles,imageformats,iconengines"
# Bei Qt6 müssen wir mitteilen, dass das Qt6-Plugin geladen werden soll
export QMAKE="qmake6"
# Verhindert Fehler beim Strippen von Bibliotheken mit neuartigen RELR-Relozierungen auf Fedora
export NO_STRIP=1

# Führe linuxdeploy aus (nur Bereitstellung, kein direktes Packen)
./tools/linuxdeploy-x86_64.AppImage \
    --appdir "${APPDIR}" \
    --plugin qt

echo "=== 6. Bereine systemnahe Bibliotheken (um Segfaults auf Fedora/Ubuntu zu verhindern) ==="
# Diese Bibliotheken werden vom Host-System bereitgestellt und führen beim Bundling zu Segfaults
SYSTEM_LIBS=(
    "libcbor" "libfido2" "libcrypt" "libevent" "libunistring" "libogg" "libpsl"
    "libssh" "libidn2" "libssl" "libcrypto" "libsystemd" "libdbus-1" "libglib-2.0"
    "libgobject-2.0" "libgio-2.0" "libgmodule-2.0" "libudev" "libacl" "libattr"
    "libmount" "libblkid" "libselinux" "libffi" "libgssapi_krb5" "libkrb5"
    "libk5crypto" "libkrb5support" "libkeyutils" "libxml2" "libzstd"
    "libdouble-conversion" "libicu" "libcanberra" "libvorbis" "libtdb" "libltdl"
    "libsasl2" "libgomp" "libcurl" "libnghttp" "libngtcp" "libbrotli" "libldap"
    "liblber" "libproxy" "libpxbackend" "libxcb" "libxkbcommon" "libz"
)

for lib in "${SYSTEM_LIBS[@]}"; do
    find "${APPDIR}/usr/lib" -name "${lib}*.so*" -delete -print || true
done

echo "=== 7. Baue finales AppImage mit appimagetool ==="
./tools/appimagetool-x86_64.AppImage "${APPDIR}"

# Verschiebe das fertige AppImage in den Ausgabe-Ordner
mv SplitCommander-*.AppImage "${OUTPUT_DIR}/"

echo "=== FERTIG! ==="
echo "Das portable, hochkompatible AppImage wurde erfolgreich unter '${OUTPUT_DIR}/' erstellt!"
