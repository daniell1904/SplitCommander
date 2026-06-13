<div align="center">
  <img src="logo.svg" width="600" alt="SplitCommander"/>
  <br/><br/>

  [![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](LICENSE)
  [![Qt6](https://img.shields.io/badge/Qt-6-green.svg)](https://www.qt.io/)
  [![KF6](https://img.shields.io/badge/KDE%20Frameworks-6-blue.svg)](https://api.kde.org/frameworks/)
  [![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)]()
</div>

---

### 📸 Screenshots & Gallery / Galerie

<details open>
<summary><b>Click to expand / Zum Aufklappen anklicken</b></summary>
<br/>

| 🔵 Nord Theme & Miller Columns | 🎨 Theme Creator & Tagging |
| :---: | :---: |
| <img src="screenshots/Bildschirmfoto_20260521_201438.png" width="450" alt="Main Window"/> <br/> *SplitCommander main window layout* | <img src="screenshots/Bildschirmfoto_20260521_201615.png" width="450" alt="Themes and Tags"/> <br/> *Miller Columns & custom file tagging* |

| 📦 Native Qt6 Installer Wizard | 📂 Custom Sidebar Groups & Casing |
| :---: | :---: |
| <img src="screenshots/Bildschirmfoto_20260521_201513.png" width="450" alt="Installer Wizard"/> <br/> *Graphical setup wizard with custom plugin choices* | <img src="screenshots/Bildschirmfoto_20260521_201631.png" width="450" alt="New Group Dialog"/> <br/> *Wider, readable dialogs & centered sidebar elements* |

</details>

---

## 🇬🇧 English

A native KDE file manager with a dual-pane layout, inspired by OneCommander. Built with Qt6 and KDE Frameworks 6.

### Key Features

* **Dual-Pane & Miller Columns** — Side-by-side browsing and fast tree-navigation.
* **Smart Sidebar** — Devices, bookmarks, custom groups, and a color-coded file tagging system.
* **Cloud & Network Integration** — Native Google Drive (kio-gdrive) and auto-detected network mounts (SMB, SFTP, NFS, WebDAV, MTP).
* **Power Tools** — Batch Renamer, built-in Terminal integration, and fully configurable shortcuts.
* **Optional Plugins** — Git Manager (repository status/actions in sidebar), Paperless-ngx document upload, ISO mounting, and Makefile actions.
* **Custom Themes** — Modern built-in themes (Nord, Catppuccin, Gruvbox), custom theme creator, and KDE Global Theme sync.

### 🚀 Installation & Build

**Using the Installer Script (Recommended):**
Automatically installs dependencies (on Arch, Fedora, openSUSE, Debian/Ubuntu) and runs the graphical setup wizard:
```bash
git clone https://github.com/daniell1904/SplitCommander.git
cd SplitCommander
chmod +x install.sh && ./install.sh
```

**Manual Build:**
Make sure you have Qt6 & KF6 (KIO, Solid, etc.) development packages installed:
```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build-release --parallel $(nproc)
sudo cmake --install build-release
```
*Note: To run locally without global installation, execute `./build-release/splitcommander`.*

### 📦 Alternative Packages

* **Flatpak:** `flatpak-builder --force-clean --user --install build-flatpak org.github.daniell1904.SplitCommander.yml`
* **AppImage:** `./build-appimage.sh`
* **Arch Linux (AUR):** `yay -S splitcommander-git`

---

## 🇩🇪 Deutsch

Ein nativer KDE-Dateimanager mit Dual-Pane-Layout, inspiriert von OneCommander. Entwickelt mit Qt6 und KDE Frameworks 6.

### Hauptmerkmale

* **Dual-Pane & Miller Columns** — Paralleles Browsen und schnelle Navigation im Verzeichnisbaum.
* **Intelligente Sidebar** — Geräte, Lesezeichen, eigene Gruppen und ein farbiges Datei-Tag-System.
* **Cloud- & Netzwerk-Integration** — Direktes Google Drive (kio-gdrive) sowie automatische Netzwerklaufwerke (SMB, SFTP, NFS, WebDAV, MTP).
* **Power-Tools** — Batch-Umbenenner, integriertes Terminal und frei konfigurierbare Tastenkürzel.
* **Optionale Plugins** — Git Manager (Sidebar-Repository-Status & Aktionen), Paperless-ngx Dokumenten-Upload, ISO einbinden und Makefile-Aktionen.
* **Eigene Themes** — Moderne integrierte Themes (Nord, Catppuccin, Gruvbox), Theme-Creator und Synchronisation mit KDE-System-Themes.

### 🚀 Installation & Build

**Über das Installations-Skript (Empfohlen):**
Installiert automatisch alle Abhängigkeiten (für Arch, Fedora, openSUSE, Debian/Ubuntu) und startet den grafischen Installer:
```bash
git clone https://github.com/daniell1904/SplitCommander.git
cd SplitCommander
chmod +x install.sh && ./install.sh
```

**Manueller Build:**
Stelle sicher, dass die Qt6- & KF6-Entwicklungspakete (KIO, Solid, etc.) installiert sind:
```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build-release --parallel $(nproc)
sudo cmake --install build-release
```
*Hinweis: Um das Programm lokal ohne globale Installation zu starten, `./build-release/splitcommander` ausführen.*

### 📦 Alternative Pakete

* **Flatpak:** `flatpak-builder --force-clean --user --install build-flatpak org.github.daniell1904.SplitCommander.yml`
* **AppImage:** `./build-appimage.sh`
* **Arch Linux (AUR):** `yay -S splitcommander-git`
