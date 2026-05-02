# SplitCommander

Nativer KDE-Dateimanager mit Dual-Pane-Layout, inspiriert von OneCommander.

## Build

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
make -C build -j$(nproc)
./build/splitcommander
```

## Abhängigkeiten

- Qt6 (Core, Gui, Widgets, Svg)
- KF6 (KIO, Solid, IconThemes, ConfigWidgets, XmlGui, WidgetsAddons, WindowSystem, Archive, Service, I18n)

## Lizenz

GPL-3.0
