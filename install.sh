#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# SplitCommander — Installer
# Unterstützte Distros: Arch Linux, Fedora, Ubuntu/Debian
# ─────────────────────────────────────────────────────────────────────────────

set -e

# Wechsel in das Verzeichnis des Skripts, um relative Pfade (z. B. zu CMakeLists.txt)
# auch unter pkexec/sudo korrekt aufzulösen.
cd "$(dirname "$0")"

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
                base-devel cmake ninja librsvg \
                qt6-base qt6-svg \
                extra-cmake-modules \
                kf6-ki18n kf6-kiconthemes kf6-kconfigwidgets kf6-kxmlgui \
                kf6-kio kf6-kwidgetsaddons kf6-kwindowsystem kf6-solid \
                kf6-karchive kf6-kservice
            # Optional: Google Drive, MTP
            sudo pacman -Sy --needed --noconfirm --asdeps \
                kio-gdrive kio-extras 2>/dev/null || true
            ;;
        fedora)
            sudo dnf install -y \
                cmake ninja-build gcc-c++ librsvg2-tools \
                qt6-qtbase-devel qt6-qtsvg-devel \
                extra-cmake-modules \
                kf6-ki18n-devel kf6-kiconthemes-devel kf6-kconfigwidgets-devel \
                kf6-kxmlgui-devel kf6-kio-devel kf6-kwidgetsaddons-devel \
                kf6-kwindowsystem-devel kf6-solid-devel kf6-karchive-devel \
                kf6-kservice-devel
            # Optional: Google Drive, MTP
            sudo dnf install -y kio-gdrive kio-extras 2>/dev/null || true
            ;;
        ubuntu|debian|linuxmint|pop)
            sudo apt-get update
            sudo apt-get install -y \
                cmake ninja-build build-essential librsvg2-bin \
                qt6-base-dev libqt6svg6-dev \
                extra-cmake-modules \
                libkf6i18n-dev libkf6iconthemes-dev libkf6configwidgets-dev \
                libkf6xmlgui-dev libkf6kio-dev libkf6widgetsaddons-dev \
                libkf6windowsystem-dev libkf6solid-dev libkf6archive-dev \
                libkf6service-dev
            # Optional: Google Drive, MTP
            sudo apt-get install -y kio-gdrive kio-extras 2>/dev/null || true
            ;;
        opensuse*|suse)
            sudo zypper install -y \
                cmake ninja gcc-c++ rsvg-view \
                qt6-base-devel qt6-svg-devel \
                extra-cmake-modules \
                kf6-ki18n-devel kf6-kiconthemes-devel kf6-kconfigwidgets-devel \
                kf6-kxmlgui-devel kf6-kio-devel kf6-kwidgetsaddons-devel \
                kf6-kwindowsystem-devel kf6-solid-devel kf6-karchive-devel \
                kf6-kservice-devel
            # Optional: Google Drive, MTP
            sudo zypper install -y kio-gdrive kio-extras 2>/dev/null || true
            ;;
        *)
            print_warn "Unbekannte Distro '$distro' — überspringe automatische Dep-Installation."
            print_warn "Bitte manuell installieren: cmake, ninja, Qt6, KF6 (siehe README.md)"
            ;;
    esac

    print_ok "Abhängigkeiten installiert"
}

# ── Hilfsfunktionen & Terminal-Prompt ─────────────────────────────────────────
prompt_yn() {
    local prompt_text="$1"
    local yn
    while true; do
        read -r -p "    $prompt_text [y/N]: " yn < /dev/tty
        case "$yn" in
            [Yy]* ) return 0 ;;
            [Nn]* | "" ) return 1 ;;
            * ) echo -e "${YELLOW}    Bitte antworte mit 'y' (ja) oder 'n' (nein).${RESET}" ;;
        esac
    done
}

# ── Plugin-Auswahl ────────────────────────────────────────────────────────────
select_plugins() {
    # Wenn Plugins bereits per CLI übergeben wurden, überspringen wir die Abfrage
    if [ "$cli_plugins_selected" -eq 1 ]; then
        print_step "Plugins über Kommandozeilen-Flags ausgewählt:"
        [ "$PLUGIN_GIT" -eq 1 ]            && print_ok "Plugin: Git Manager aktiviert"            || print_warn "Plugin: Git Manager nicht aktiviert"
        [ "$PLUGIN_PAPERLESS" -eq 1 ]      && print_ok "Plugin: Paperless-ngx aktiviert"          || print_warn "Plugin: Paperless-ngx nicht aktiviert"
        [ "$PLUGIN_MOUNTISO" -eq 1 ]       && print_ok "Plugin: ISO einbinden aktiviert"          || print_warn "Plugin: ISO einbinden nicht aktiviert"
        [ "$PLUGIN_MAKEFILEACTIONS" -eq 1 ]&& print_ok "Plugin: Makefile-Aktionen aktiviert"      || print_warn "Plugin: Makefile-Aktionen nicht aktiviert"
        return
    fi

    local has_gui=0
    if [ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ]; then
        has_gui=1
    fi

    # Wenn wir im interaktiven Modus sind und ein GUI aktiv ist,
    # kompilieren und starten wir den Qt6-Installer!
    if [ "$has_gui" -eq 1 ]; then
        print_step "Bereite grafischen Installer vor..."
        if cmake -B build-installer -S src/installer -DCMAKE_BUILD_TYPE=Release -G Ninja &>/dev/null && \
           cmake --build build-installer &>/dev/null; then
            print_ok "Grafischer Installer gestartet."
            ./build-installer/splitcommander-installer
            exit 0
        else
            print_warn "Kompilierung des grafischen Installers fehlgeschlagen. Verwende Standard-Abfrage."
        fi
    fi

    # Falls GUI verfügbar, versuchen wir kdialog oder zenity
    if [ "$has_gui" -eq 1 ]; then
        # 1. kdialog (KDE)
        if command -v kdialog &>/dev/null; then
            local choices
            choices=$(kdialog --separate-output --checklist "Wähle die optionalen Plugins für SplitCommander aus:" \
                "1" "Git Manager (Git-Repository-Verwaltung in der Sidebar)" off \
                "2" "Paperless-ngx (Dokumente hochladen und durchsuchen)" off \
                "3" "ISO einbinden (ISO/IMG-Dateien per Klick mounten)" off \
                "4" "Makefile-Aktionen (Make-Targets direkt ausführen)" off 2>/dev/null)
            
            if [ $? -eq 0 ]; then
                PLUGIN_GIT=0
                PLUGIN_PAPERLESS=0
                PLUGIN_MOUNTISO=0
                PLUGIN_MAKEFILEACTIONS=0
                for choice in $choices; do
                    case $choice in
                        1) PLUGIN_GIT=1 ;;
                        2) PLUGIN_PAPERLESS=1 ;;
                        3) PLUGIN_MOUNTISO=1 ;;
                        4) PLUGIN_MAKEFILEACTIONS=1 ;;
                    esac
                done
                print_step "Erfolgreich über grafische Oberfläche ausgewählt:"
                [ "$PLUGIN_GIT" -eq 1 ]            && print_ok "Plugin: Git Manager aktiviert"            || print_warn "Plugin: Git Manager nicht aktiviert"
                [ "$PLUGIN_PAPERLESS" -eq 1 ]      && print_ok "Plugin: Paperless-ngx aktiviert"          || print_warn "Plugin: Paperless-ngx nicht aktiviert"
                [ "$PLUGIN_MOUNTISO" -eq 1 ]       && print_ok "Plugin: ISO einbinden aktiviert"          || print_warn "Plugin: ISO einbinden nicht aktiviert"
                [ "$PLUGIN_MAKEFILEACTIONS" -eq 1 ]&& print_ok "Plugin: Makefile-Aktionen aktiviert"      || print_warn "Plugin: Makefile-Aktionen nicht aktiviert"
                return
            else
                # Wenn abgebrochen, machen wir im Terminal weiter
                print_warn "Grafischer Dialog abgebrochen oder geschlossen — Fallback auf Terminal-Abfrage."
            fi
        # 2. zenity (GTK fallback)
        elif command -v zenity &>/dev/null; then
            local choices
            choices=$(zenity --list --checklist \
                --title="SplitCommander Plugin-Auswahl" \
                --text="Wähle die optionalen Plugins aus, die installiert werden sollen:" \
                --column="Aktiv" --column="ID" --column="Plugin" --column="Beschreibung" \
                --hide-column=2 --separator=" " \
                FALSE "1" "Git Manager" "Sidebar Git-Gruppe & Repository-Verwaltung" \
                FALSE "2" "Paperless-ngx" "Dokumente hochladen und durchsuchen" \
                FALSE "3" "ISO einbinden" "ISO/IMG-Dateien per Klick mounten" \
                FALSE "4" "Makefile-Aktionen" "Make-Targets direkt ausführen" 2>/dev/null)
            
            if [ $? -eq 0 ]; then
                PLUGIN_GIT=0
                PLUGIN_PAPERLESS=0
                PLUGIN_MOUNTISO=0
                PLUGIN_MAKEFILEACTIONS=0
                for choice in $choices; do
                    case $choice in
                        1) PLUGIN_GIT=1 ;;
                        2) PLUGIN_PAPERLESS=1 ;;
                        3) PLUGIN_MOUNTISO=1 ;;
                        4) PLUGIN_MAKEFILEACTIONS=1 ;;
                    esac
                done
                print_step "Erfolgreich über grafische Oberfläche ausgewählt:"
                [ "$PLUGIN_GIT" -eq 1 ]            && print_ok "Plugin: Git Manager aktiviert"            || print_warn "Plugin: Git Manager nicht aktiviert"
                [ "$PLUGIN_PAPERLESS" -eq 1 ]      && print_ok "Plugin: Paperless-ngx aktiviert"          || print_warn "Plugin: Paperless-ngx nicht aktiviert"
                [ "$PLUGIN_MOUNTISO" -eq 1 ]       && print_ok "Plugin: ISO einbinden aktiviert"          || print_warn "Plugin: ISO einbinden nicht aktiviert"
                [ "$PLUGIN_MAKEFILEACTIONS" -eq 1 ]&& print_ok "Plugin: Makefile-Aktionen aktiviert"      || print_warn "Plugin: Makefile-Aktionen nicht aktiviert"
                return
            else
                # Wenn abgebrochen, machen wir im Terminal weiter
                print_warn "Grafischer Dialog abgebrochen oder geschlossen — Fallback auf Terminal-Abfrage."
            fi
        fi
    fi

    # Fallback: Terminal-interaktive Ja/Nein Abfrage
    echo ""
    echo -e "${BOLD}Optionale Plugins auswählen (Ja/Nein):${RESET}"
    echo "─────────────────────────────────────────"
    
    PLUGIN_GIT=0
    PLUGIN_PAPERLESS=0
    PLUGIN_MOUNTISO=0
    PLUGIN_MAKEFILEACTIONS=0

    if prompt_yn "Git Manager installieren? (Repository-Verwaltung in der Sidebar)"; then
        PLUGIN_GIT=1
        print_ok "Git Manager ausgewählt"
    else
        print_warn "Git Manager nicht ausgewählt"
    fi

    if prompt_yn "Paperless-ngx installieren? (Dokumente hochladen und durchsuchen)"; then
        PLUGIN_PAPERLESS=1
        print_ok "Paperless-ngx ausgewählt"
    else
        print_warn "Paperless-ngx nicht ausgewählt"
    fi

    if prompt_yn "ISO einbinden installieren? (ISO/IMG-Dateien mounten)"; then
        PLUGIN_MOUNTISO=1
        print_ok "ISO einbinden ausgewählt"
    else
        print_warn "ISO einbinden nicht ausgewählt"
    fi

    if prompt_yn "Makefile-Aktionen installieren? (Make-Targets direkt ausführen)"; then
        PLUGIN_MAKEFILEACTIONS=1
        print_ok "Makefile-Aktionen ausgewählt"
    else
        print_warn "Makefile-Aktionen nicht ausgewählt"
    fi
    echo ""
}


# ── Build ─────────────────────────────────────────────────────────────────────
build() {
    print_step "Konfiguriere Build"
    echo -e "  Aktivierte Plugins für diesen Build:"
    [ "${PLUGIN_GIT:-0}" -eq 1 ]            && echo -e "    ${GREEN}✓${RESET} ${BOLD}Git Manager${RESET}" || echo -e "    ${RED}✗${RESET} Git Manager"
    [ "${PLUGIN_PAPERLESS:-0}" -eq 1 ]      && echo -e "    ${GREEN}✓${RESET} ${BOLD}Paperless-ngx${RESET}" || echo -e "    ${RED}✗${RESET} Paperless-ngx"
    [ "${PLUGIN_MOUNTISO:-0}" -eq 1 ]       && echo -e "    ${GREEN}✓${RESET} ${BOLD}ISO einbinden${RESET}" || echo -e "    ${RED}✗${RESET} ISO einbinden"
    [ "${PLUGIN_MAKEFILEACTIONS:-0}" -eq 1 ]&& echo -e "    ${GREEN}✓${RESET} ${BOLD}Makefile-Aktionen${RESET}" || echo -e "    ${RED}✗${RESET} Makefile-Aktionen"
    echo ""

    local cmake_extra=""
    [ "${PLUGIN_GIT:-0}" -eq 1 ]            && cmake_extra="$cmake_extra -DSC_PLUGIN_GIT=ON"
    [ "${PLUGIN_PAPERLESS:-0}" -eq 1 ]      && cmake_extra="$cmake_extra -DSC_PLUGIN_PAPERLESS=ON"
    [ "${PLUGIN_MOUNTISO:-0}" -eq 1 ]       && cmake_extra="$cmake_extra -DSC_PLUGIN_MOUNTISO=ON"
    [ "${PLUGIN_MAKEFILEACTIONS:-0}" -eq 1 ]&& cmake_extra="$cmake_extra -DSC_PLUGIN_MAKEFILEACTIONS=ON"

    cmake -B "$BUILD_DIR" -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -G Ninja \
        $cmake_extra
    print_ok "Konfiguration abgeschlossen"

    print_step "Kompiliere SplitCommander (mit ausgewählten Plugins)"
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
    local svg="src/org.github.daniell1904.SplitCommander.svg"

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
        if command -v rsvg-convert &>/dev/null; then
            rsvg-convert -w "$size" -h "$size" "$svg" -o "/tmp/sc_${size}.png" 2>/dev/null
        elif command -v convert &>/dev/null; then
            convert -background none -resize "${size}x${size}" "$png" "/tmp/sc_${size}.png" 2>/dev/null
        else
            cp "$png" "/tmp/sc_${size}.png"
        fi
        sudo cp "/tmp/sc_${size}.png" "$dir/org.github.daniell1904.SplitCommander.png"
        rm -f "/tmp/sc_${size}.png"
    done

    # SVG zusätzlich installieren falls vorhanden
    if [ -f "$svg" ]; then
        sudo mkdir -p /usr/share/icons/hicolor/scalable/apps
        sudo cp "$svg" /usr/share/icons/hicolor/scalable/apps/org.github.daniell1904.SplitCommander.svg
    fi

    sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor 2>/dev/null || true
    kbuildsycoca6 --noincremental 2>/dev/null || true
    print_ok "Icon installiert (${sizes[*]} px)"
}

# ── Desktop-Eintrag ───────────────────────────────────────────────────────────
install_desktop() {
    local desktop_file="/usr/share/applications/org.github.daniell1904.SplitCommander.desktop"
    print_step "Erstelle Desktop-Eintrag"
    sudo tee "$desktop_file" > /dev/null <<EOF
[Desktop Entry]
Name=SplitCommander
Comment=Native KDE Dual-Pane File Manager
Comment[de]=Nativer KDE Dual-Pane Dateimanager
Exec=splitcommander
Icon=org.github.daniell1904.SplitCommander
Terminal=false
Type=Application
Categories=System;FileTools;FileManager;
MimeType=inode/directory;
EOF
    sudo update-desktop-database 2>/dev/null || true
    print_ok "Desktop-Eintrag erstellt: $desktop_file"
}

# ── Hilfe / Usage ─────────────────────────────────────────────────────────────
print_usage() {
    echo -e "${BOLD}SplitCommander Installer${RESET}"
    echo "Nutzung: ./install.sh [OPTIONEN]"
    echo ""
    echo -e "${BOLD}Optionen:${RESET}"
    echo "  --help, -h       Diese Hilfe anzeigen"
    echo "  --no-deps        Installation von System-Abhängigkeiten überspringen"
    echo "  --no-install     Nur kompilieren, nicht systemweit installieren"
    echo "  --plugins-only   Nur Plugins neu konfigurieren und bauen (überspringt Deps)"
    echo ""
    echo -e "${BOLD}Plugins direkt aktivieren (überspringt interaktive Abfrage):${RESET}"
    echo "  --git            Git Manager Plugin aktivieren"
    echo "  --paperless      Paperless-ngx Plugin aktivieren"
    echo "  --iso            ISO einbinden Plugin aktivieren"
    echo "  --makefile       Makefile-Aktionen Plugin aktivieren"
    echo "  --all            Alle Plugins aktivieren"
    echo ""
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

    # Standardwerte
    PLUGIN_GIT=0
    PLUGIN_PAPERLESS=0
    PLUGIN_MOUNTISO=0
    PLUGIN_MAKEFILEACTIONS=0
    local skip_deps=0
    local skip_install=0
    local plugins_only=0
    cli_plugins_selected=0

    # Argumente parsen
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                print_usage
                exit 0
                ;;
            --no-deps)
                skip_deps=1
                shift
                ;;
            --no-install)
                skip_install=1
                shift
                ;;
            --plugins-only)
                plugins_only=1
                skip_deps=1
                shift
                ;;
            --git)
                PLUGIN_GIT=1
                cli_plugins_selected=1
                shift
                ;;
            --paperless)
                PLUGIN_PAPERLESS=1
                cli_plugins_selected=1
                shift
                ;;
            --iso)
                PLUGIN_MOUNTISO=1
                cli_plugins_selected=1
                shift
                ;;
            --makefile)
                PLUGIN_MAKEFILEACTIONS=1
                cli_plugins_selected=1
                shift
                ;;
            --all)
                PLUGIN_GIT=1
                PLUGIN_PAPERLESS=1
                PLUGIN_MOUNTISO=1
                PLUGIN_MAKEFILEACTIONS=1
                cli_plugins_selected=1
                shift
                ;;
            *)
                print_err "Unbekannter Parameter: $1"
                print_usage
                exit 1
                ;;
        esac
    done

    if [ "$plugins_only" -eq 1 ]; then
        print_step "Plugin-Nachinstallation"
    fi

    select_plugins

    [ "$skip_deps" -eq 0 ] && install_deps "$distro"
    build
    if [ "$skip_install" -eq 0 ]; then
        install
        install_icon
        install_desktop
    fi

    echo ""
    echo -e "${GREEN}${BOLD}Installation abgeschlossen.${RESET}"
    echo -e "Starten mit: ${BOLD}splitcommander${RESET}"
    echo ""
}

main "$@"
