#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# SplitCommander — Release Script
# Verwendung: ./release.sh v0.2.0 "Kurze Beschreibung"
# ─────────────────────────────────────────────────────────────────────────────

set -e

BOLD="\033[1m"
GREEN="\033[1;32m"
RED="\033[1;31m"
RESET="\033[0m"

print_step() { echo -e "${BOLD}==> $1${RESET}"; }
print_ok()   { echo -e "${GREEN}    ✓ $1${RESET}"; }
print_err()  { echo -e "${RED}    ✗ $1${RESET}"; exit 1; }

# ── Argumente prüfen ──────────────────────────────────────────────────────────
VERSION=$1
DESCRIPTION=$2

if [ -z "$VERSION" ]; then
    echo -e "${BOLD}Verwendung:${RESET} ./release.sh <version> [beschreibung]"
    echo -e "Beispiel:   ./release.sh v0.2.0 \"Bugfixes und neue Features\""
    exit 1
fi

if [ -z "$DESCRIPTION" ]; then
    DESCRIPTION="Release $VERSION"
fi

# ── Im Projektverzeichnis? ────────────────────────────────────────────────────
if [ ! -f "CMakeLists.txt" ]; then
    print_err "Bitte aus dem SplitCommander-Verzeichnis ausführen."
fi

echo ""
echo -e "${BOLD}SplitCommander Release: $VERSION${RESET}"
echo "─────────────────────────────────────────"
echo ""

# ── Build ─────────────────────────────────────────────────────────────────────
print_step "Build starten"
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local > /dev/null
cmake --build build --parallel "$(nproc)"
print_ok "Build erfolgreich"

# ── Uncommitted changes? ──────────────────────────────────────────────────────
print_step "Git-Status prüfen"
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo -e "${BOLD}Ungespeicherte Änderungen gefunden:${RESET}"
    git status --short
    echo ""
    read -rp "Alle Änderungen jetzt committen? [j/N] " answer
    if [[ "$answer" =~ ^[jJyY]$ ]]; then
        git add -A
        git commit -m "chore: prepare release $VERSION"
        print_ok "Änderungen committet"
    else
        print_err "Bitte zuerst alle Änderungen committen."
    fi
fi

# ── Tag setzen ────────────────────────────────────────────────────────────────
print_step "Git-Tag $VERSION setzen"
if git rev-parse "$VERSION" >/dev/null 2>&1; then
    print_err "Tag $VERSION existiert bereits."
fi
git tag -a "$VERSION" -m "Release $VERSION"
print_ok "Tag gesetzt"

# ── Pushen ────────────────────────────────────────────────────────────────────
print_step "Pushen nach GitHub"
git push origin main
git push origin "$VERSION"
print_ok "Gepusht"

# ── GitHub Release erstellen ──────────────────────────────────────────────────
print_step "GitHub Release erstellen"
gh release create "$VERSION" \
    --title "SplitCommander $VERSION" \
    --notes "## $VERSION

$DESCRIPTION

### Installation

\`\`\`bash
git clone https://github.com/daniell1904/SplitCommander.git
cd SplitCommander
chmod +x install.sh
./install.sh
\`\`\`"

print_ok "Release $VERSION erstellt"

echo ""
echo -e "${GREEN}${BOLD}Release $VERSION erfolgreich veröffentlicht.${RESET}"
echo -e "https://github.com/daniell1904/SplitCommander/releases/tag/$VERSION"
echo ""
