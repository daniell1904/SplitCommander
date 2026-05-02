#!/bin/bash
set -e

VERSION="$1"
BESCHREIBUNG="${2:-Release $VERSION}"

if [ -z "$VERSION" ]; then
    echo "Verwendung: sc-release v0.2.0 [\"Beschreibung\"]"
    exit 1
fi

cd ~/SplitCommander

echo -e "\e[36m  Baue SplitCommander $VERSION...\e[0m"
make -C build -j$(nproc)

echo -e "\e[36m  Committe und pushe...\e[0m"
git add .
git commit -m "release: $VERSION — $BESCHREIBUNG"
git tag "$VERSION"
git push origin main
git push origin "$VERSION"

echo -e "\e[32m  Release $VERSION fertig.\e[0m"
