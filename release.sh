#!/bin/bash
set -e

VERSION="$1"
BESCHREIBUNG="${2:-Release $VERSION}"

if [ -z "$VERSION" ]; then
    echo "Verwendung: sc-release v0.2.0 [\"Beschreibung\"]"
    exit 1
fi

# Version muss mit v beginnen
if [[ "$VERSION" != v* ]]; then
    echo -e "\e[31m  Fehler: Version muss mit 'v' beginnen, z.B. v0.2.0\e[0m"
    exit 1
fi

cd ~/SplitCommander

# Prüfen ob Tag schon existiert
if git tag | grep -q "^${VERSION}$"; then
    echo -e "\e[31m  Fehler: Tag $VERSION existiert bereits.\e[0m"
    echo -e "\e[33m  Vorhandene Tags:\e[0m"
    git tag | sort -V | tail -5
    exit 1
fi

echo -e "\e[36m  Baue SplitCommander $VERSION...\e[0m"
make -C build -j$(nproc)

echo -e "\e[36m  Committe und pushe...\e[0m"
git add .
git commit -m "release: $VERSION — $BESCHREIBUNG"
git push origin main

echo -e "\e[36m  Erstelle GitHub Release $VERSION...\e[0m"
if command -v gh &>/dev/null; then
    gh release create "$VERSION" \
        --title "SplitCommander $VERSION" \
        --notes "$BESCHREIBUNG" \
        --latest
    echo -e "\e[32m  GitHub Release $VERSION erstellt.\e[0m"
else
    # Fallback: nur Tag pushen
    git tag -a "$VERSION" -m "$BESCHREIBUNG"
    git push origin "$VERSION"
    echo -e "\e[33m  'gh' nicht gefunden — nur Tag gepusht.\e[0m"
    echo -e "\e[33m  Release manuell auf GitHub anlegen: https://github.com/daniell1904/SplitCommander/releases/new\e[0m"
fi

echo -e "\e[32m  Release $VERSION fertig.\e[0m"
