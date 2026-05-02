<div align="center">
  <img src="logo.svg" width="600" alt="SplitCommander"/>
  <br/><br/>

  [![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)
  [![Qt6](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io/)
  [![KF6](https://img.shields.io/badge/KDE%20Frameworks-6-blue.svg)](https://api.kde.org/frameworks/)
  [![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)]()

</div>

---

<details open>
<summary><b>🇩🇪 Deutsch</b></summary>

## SplitCommander

Ein nativer KDE-Dateimanager mit Dual-Pane-Layout, inspiriert von OneCommander.  
Gebaut mit Qt6 und KDE Frameworks 6.

### Funktionen

- **Dual-Pane-Layout** — zwei Verzeichnisse gleichzeitig im Blick
- **Miller-Columns-Ansicht** — schnelle Navigation durch Verzeichnisbäume
- **Sidebar** mit Geräten, Lesezeichen, benutzerdefinierten Gruppen und Tags
- **Detailansicht** mit konfigurierbaren Spalten (Name, Typ, Größe, Datum, Rechte u.v.m.)
- **Icon-Ansicht** für medienreiche Verzeichnisse
- **Tag-System** — Dateien mit farbigen Tags markieren und filtern
- **Batch-Umbenenner** — mehrere Dateien gleichzeitig umbenennen
- **Themes** — Nord, Catppuccin Mocha, Gruvbox Dark sowie KDE Global Themes
- **KIO-Integration** — Netzlaufwerke, SSH, SFTP direkt nutzbar

### Installation

```bash
git clone https://github.com/daniell1904/SplitCommander.git
cd SplitCommander
chmod +x install.sh
./install.sh
```

Das Script erkennt automatisch die Distribution und installiert alle Abhängigkeiten.

| Distribution | Paketmanager |
|---|---|
| Arch / CachyOS / Manjaro | pacman |
| Fedora | dnf |
| Ubuntu / Debian / Mint | apt |
| openSUSE | zypper |

**Optionale Flags:**
```bash
./install.sh --no-deps     # Abhängigkeiten überspringen
./install.sh --no-install  # Nur bauen, nicht systemweit installieren
```

### Manueller Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --parallel $(nproc)
sudo cmake --install build
```

### Abhängigkeiten

- Qt 6 — Core, Gui, Widgets, Svg
- KDE Frameworks 6 — KIO, Solid, IconThemes, ConfigWidgets, XmlGui, WidgetsAddons, WindowSystem, Archive, Service, I18n
- CMake ≥ 3.18, Ninja, GCC/Clang mit C++20

### Lizenz

GPL-3.0 — siehe [LICENSE](LICENSE)

</details>

---

<details>
<summary><b>🇬🇧 English</b></summary>

## SplitCommander

A native KDE file manager with dual-pane layout, inspired by OneCommander.  
Built with Qt6 and KDE Frameworks 6.

### Features

- **Dual-pane layout** — browse two directories side by side
- **Miller columns view** — fast directory tree navigation
- **Sidebar** with devices, bookmarks, custom groups and tags
- **Detail view** with configurable columns (name, type, size, date, permissions, and more)
- **Icon view** for media-rich directories
- **Tag system** — mark and filter files with colored tags
- **Batch renamer** — rename multiple files at once
- **Themes** — Nord, Catppuccin Mocha, Gruvbox Dark and KDE Global Theme support
- **KIO integration** — network drives, SSH, SFTP out of the box

### Installation

```bash
git clone https://github.com/daniell1904/SplitCommander.git
cd SplitCommander
chmod +x install.sh
./install.sh
```

The script automatically detects your distribution and installs all required dependencies.

| Distribution | Package manager |
|---|---|
| Arch / CachyOS / Manjaro | pacman |
| Fedora | dnf |
| Ubuntu / Debian / Mint | apt |
| openSUSE | zypper |

**Optional flags:**
```bash
./install.sh --no-deps     # skip dependency installation
./install.sh --no-install  # build only, don't install system-wide
```

### Manual build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --parallel $(nproc)
sudo cmake --install build
```

### Dependencies

- Qt 6 — Core, Gui, Widgets, Svg
- KDE Frameworks 6 — KIO, Solid, IconThemes, ConfigWidgets, XmlGui, WidgetsAddons, WindowSystem, Archive, Service, I18n
- CMake ≥ 3.18, Ninja, GCC/Clang with C++20

### License

GPL-3.0 — see [LICENSE](LICENSE)

</details>
