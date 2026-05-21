# SplitCommander AUR-Paketierung (Arch Linux)

Dieses Verzeichnis enthält die offizielle Paket-Rezeptur (`PKGBUILD`) für das Arch User Repository (AUR). Mit diesem Paket können Arch Linux-, Manjaro- und EndeavourOS-Nutzer SplitCommander direkt über ihren Paketmanager (z.B. `yay`) installieren.

## Wie du das Paket im AUR veröffentlichst (Schritt für Schritt)

### 1. Vorbereitung
1. Erstelle einen Account auf [aur.archlinux.org](https://aur.archlinux.org/) (falls du noch keinen hast).
2. Hinterlege deinen SSH-Key in deinem AUR-Profil (unter "My Account").

### 2. AUR-Repository klonen
Erstelle auf deinem lokalen PC ein leeres Verzeichnis und klone das offizielle (noch leere) AUR-Repository für SplitCommander:
```bash
git clone ssh://aur@aur.archlinux.org/splitcommander-git.git
cd splitcommander-git
```

### 3. PKGBUILD kopieren & `.SRCINFO` generieren
Kopiere das `PKGBUILD` aus diesem Ordner in das neu geklonte Verzeichnis. 
Generiere danach die `.SRCINFO` (diese Datei enthält Metadaten, die das AUR zum Parsen braucht):
```bash
# Kopiere das PKGBUILD (Pfade ggf. anpassen)
cp ../SplitCommander/packaging/aur/PKGBUILD .

# Generiere die Metadaten
makepkg --printsrcinfo > .SRCINFO
```

### 4. Testen (Optional aber empfohlen)
Du kannst testen, ob das Paket sauber baut und alle Abhängigkeiten passen:
```bash
makepkg -s
```

### 5. Pushen zum AUR
Füge beide Dateien hinzu, committe sie und pushe sie ins offizielle AUR:
```bash
git add PKGBUILD .SRCINFO
git commit -m "Initial release of splitcommander-git"
git push
```

**Fertig!** Ab diesem Moment kann jeder Arch-Nutzer auf der Welt dein Programm mit folgendem einfachen Befehl installieren:
```bash
yay -S splitcommander-git
```
