#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# SplitCommander — Installer
# Unterstützte Distros: Arch Linux, Fedora, Ubuntu/Debian
# ─────────────────────────────────────────────────────────────────────────────

set -e

INSTALL_PREFIX="/usr/local"
BUILD_DIR="build-release"
BOLD="\033[1m"
GREEN="\033[1;32m"
RED="\033[1;31m"
YELLOW="\033[1;33m"
RESET="\033[0m"

print_step() { echo -e "${BOLD}==> $1${RESET}"; }
print_ok()   { echo -e "${GREEN}    ✓ $1${RESET}"; }
print_warn() { echo -e "${YELLOW}    ! $1${RESET}"; }
print_err()  { echo -e "${RED}    ✗ $1${RESET}"; }

# ── Distro erkennen ───────────────────────────────────────────────────────────
detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        echo "$ID"
    else
        echo "unknown"
    fi
}

# ── Dependencies installieren ─────────────────────────────────────────────────
install_deps() {
    local distro=$1
    print_step "Installiere Abhängigkeiten für: $distro"

    case "$distro" in
        arch|cachyos|endeavouros|manjaro)
            sudo pacman -Sy --needed --noconfirm \
                base-devel cmake ninja \
                qt6-base qt6-svg \
                extra-cmake-modules \
                kf6-ki18n kf6-kiconthemes kf6-kconfigwidgets kf6-kxmlgui \
                kf6-kio kf6-kwidgetsaddons kf6-kwindowsystem kf6-solid \
                kf6-karchive kf6-kservice
            ;;
        fedora)
            sudo dnf install -y \
                cmake ninja-build gcc-c++ \
                qt6-qtbase-devel qt6-qtsvg-devel \
                extra-cmake-modules \
                kf6-ki18n-devel kf6-kiconthemes-devel kf6-kconfigwidgets-devel \
                kf6-kxmlgui-devel kf6-kio-devel kf6-kwidgetsaddons-devel \
                kf6-kwindowsystem-devel kf6-solid-devel kf6-karchive-devel \
                kf6-kservice-devel
            ;;
        ubuntu|debian|linuxmint|pop)
            sudo apt-get update
            sudo apt-get install -y \
                cmake ninja-build build-essential \
                qt6-base-dev libqt6svg6-dev \
                extra-cmake-modules \
                libkf6i18n-dev libkf6iconthemes-dev libkf6configwidgets-dev \
                libkf6xmlgui-dev libkf6kio-dev libkf6widgetsaddons-dev \
                libkf6windowsystem-dev libkf6solid-dev libkf6archive-dev \
                libkf6service-dev
            ;;
        opensuse*|suse)
            sudo zypper install -y \
                cmake ninja gcc-c++ \
                qt6-base-devel qt6-svg-devel \
                extra-cmake-modules \
                kf6-ki18n-devel kf6-kiconthemes-devel kf6-kconfigwidgets-devel \
                kf6-kxmlgui-devel kf6-kio-devel kf6-kwidgetsaddons-devel \
                kf6-kwindowsystem-devel kf6-solid-devel kf6-karchive-devel \
                kf6-kservice-devel
            ;;
        *)
            print_warn "Unbekannte Distro '$distro' — überspringe automatische Dep-Installation."
            print_warn "Bitte manuell installieren: cmake, ninja, Qt6, KF6 (siehe README.md)"
            ;;
    esac

    print_ok "Abhängigkeiten installiert"
}

# ── Build ─────────────────────────────────────────────────────────────────────
build() {
    print_step "Konfiguriere Build"
    cmake -B "$BUILD_DIR" -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -G Ninja
    print_ok "Konfiguration abgeschlossen"

    print_step "Kompiliere SplitCommander"
    cmake --build "$BUILD_DIR" --parallel "$(nproc)"
    print_ok "Build erfolgreich"
}

# ── Install ───────────────────────────────────────────────────────────────────
install() {
    print_step "Installiere nach $INSTALL_PREFIX"
    sudo cmake --install "$BUILD_DIR"
    print_ok "SplitCommander installiert nach $INSTALL_PREFIX/bin/splitcommander"
}

# ── Icon installieren ─────────────────────────────────────────────────────────
install_icon() {
    print_step "Installiere Icon"

    local png="src/splitcommander_128.png"
    local svg="logo.svg"

    if [ ! -f "$png" ]; then
        print_warn "$png nicht gefunden — überspringe Icon-Installation"
        return
    fi

    # PNG in alle Standardgrößen skalieren
    local sizes=(16 32 48 64 128 256)
    local converter=""

    if command -v convert &>/dev/null; then
        converter="imagemagick"
    elif command -v inkscape &>/dev/null; then
        converter="inkscape"
    else
        print_warn "imagemagick nicht gefunden — installiere es"
        local distro
        distro=$(detect_distro)
        case "$distro" in
            arch|cachyos|endeavouros|manjaro) sudo pacman -S --needed --noconfirm imagemagick ;;
            fedora)                           sudo dnf install -y imagemagick ;;
            ubuntu|debian|linuxmint|pop)      sudo apt-get install -y imagemagick ;;
            opensuse*|suse)                   sudo zypper install -y ImageMagick ;;
        esac
        converter="imagemagick"
    fi

    for size in "${sizes[@]}"; do
        local dir="/usr/share/icons/hicolor/${size}x${size}/apps"
        sudo mkdir -p "$dir"
        convert -background none -resize "${size}x${size}" "$png" "/tmp/sc_${size}.png" 2>/dev/null
        sudo cp "/tmp/sc_${size}.png" "$dir/splitcommander.png"
        rm -f "/tmp/sc_${size}.png"
    done

    # SVG zusätzlich installieren falls vorhanden
    if [ -f "$svg" ]; then
        sudo mkdir -p /usr/share/icons/hicolor/scalable/apps
        sudo cp "$svg" /usr/share/icons/hicolor/scalable/apps/splitcommander.svg
    fi

    sudo gtk-update-icon-cache /usr/share/icons/hicolor 2>/dev/null || true
    print_ok "Icon installiert (${sizes[*]} px)"
}

# ── Desktop-Eintrag ───────────────────────────────────────────────────────────
install_desktop() {
    local desktop_file="/usr/share/applications/splitcommander.desktop"
    print_step "Erstelle Desktop-Eintrag"
    sudo tee "$desktop_file" > /dev/null <<EOF
[Desktop Entry]
Name=SplitCommander
Comment=Nativer KDE Dual-Pane Dateimanager
Exec=splitcommander
Icon=splitcommander
Terminal=false
Type=Application
Categories=System;FileTools;FileManager;
MimeType=inode/directory;
EOF
    sudo update-desktop-database 2>/dev/null || true
    print_ok "Desktop-Eintrag erstellt: $desktop_file"
}

# ── Hauptablauf ───────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${BOLD}SplitCommander Installer${RESET}"
    echo "─────────────────────────────────────────"
    echo ""

    # Prüfen ob wir im Projektverzeichnis sind
    if [ ! -f "CMakeLists.txt" ]; then
        print_err "CMakeLists.txt nicht gefunden."
        print_err "Bitte das Script aus dem SplitCommander-Verzeichnis ausführen."
        exit 1
    fi

    local distro
    distro=$(detect_distro)
    print_ok "Erkannte Distro: $distro"

    # Optionale Flags
    local skip_deps=0
    local skip_install=0
    for arg in "$@"; do
        case $arg in
            --no-deps)    skip_deps=1 ;;
            --no-install) skip_install=1 ;;
        esac
    done

    [ "$skip_deps" -eq 0 ]    && install_deps "$distro"
    build
    [ "$skip_install" -eq 0 ] && install
    install_icon
    install_desktop

    echo ""
    echo -e "${GREEN}${BOLD}Installation abgeschlossen.${RESET}"
    echo -e "Starten mit: ${BOLD}splitcommander${RESET}"
    echo ""
}

main "$@"
