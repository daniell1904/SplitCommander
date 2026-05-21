cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(splitcommander VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Optionale Plugins ──────────────────────────────────────────────────────────
option(SC_PLUGIN_GIT "Git Manager Plugin einbauen" OFF)
option(SC_PLUGIN_PAPERLESS "Paperless-ngx Plugin einbauen" OFF)
option(SC_PLUGIN_MOUNTISO "ISO-Einbinden Plugin einbauen" OFF)
option(SC_PLUGIN_MAKEFILEACTIONS "Makefile-Aktionen Plugin einbauen" OFF)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Concurrent DBus Network)
find_package(KF6 REQUIRED COMPONENTS
    I18n IconThemes ConfigWidgets XmlGui KIO
    WidgetsAddons WindowSystem Solid Archive Service CoreAddons Baloo
)

set(SC_SOURCES
    src/main.cpp
    src/panecomponents.cpp
    src/mainwindow.cpp
    src/mainwindow_shortcuts.cpp
    src/mainwindow_session.cpp
    src/panetoolbar.cpp
    src/millercolumn.cpp
    src/millerarea.cpp
    src/panewidget.cpp
    src/panewidget_hamburger.cpp
    src/panewidget_search.cpp
    src/panewidget_footer.cpp
    src/filepane.cpp
    src/filepane_menus.cpp
    src/filepaneproxy.cpp
    src/scremoveaction.cpp
    src/filepanedelegate.cpp
    src/drivedelegate.cpp
    src/joboverlay.cpp
    src/drophandler.cpp
    src/filemanager1.cpp
    src/drivemanager.cpp
    src/sidebar.cpp
    src/addnetworkdialog.cpp
    src/sidebargroups.cpp
    src/tagmanager.cpp
    src/settingsdialog.cpp
    src/batchrenamer.cpp
    src/config.cpp
    src/thememanager.cpp
    src/thememanager_themes.cpp
    src/dialogutils.cpp
    src/thumbnailmanager.cpp
    src/themecreatordialog.cpp
    src/themepreviewwidget.cpp
    src/hoverfader.cpp
)

if(SC_PLUGIN_GIT)
    list(APPEND SC_SOURCES
        src/plugins/git/gitmanagerdialog.cpp
        src/plugins/git/gitstatusmanager.cpp
    )
endif()

if(SC_PLUGIN_PAPERLESS)
    list(APPEND SC_SOURCES
        src/plugins/paperless/paperlessmanager.cpp
        src/plugins/paperless/paperlessmanager.h
    )
endif()

if(SC_PLUGIN_MOUNTISO)
    list(APPEND SC_SOURCES
        src/plugins/mountiso/mountiso.cpp
    )
endif()

if(SC_PLUGIN_MAKEFILEACTIONS)
    list(APPEND SC_SOURCES
        src/plugins/makefileactions/makefileactions.cpp
    )
endif()

add_executable(splitcommander ${SC_SOURCES})

target_link_libraries(splitcommander
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Concurrent Qt6::DBus Qt6::Network
    KF6::I18n KF6::IconThemes KF6::IconWidgets KF6::ConfigWidgets KF6::XmlGui
    KF6::KIOCore KF6::KIOWidgets KF6::KIOFileWidgets KF6::WidgetsAddons
    KF6::WindowSystem KF6::Solid KF6::Archive KF6::Service KF6::CoreAddons KF6::Baloo
)

target_compile_definitions(splitcommander PRIVATE
    SC_VERSION="${PROJECT_VERSION}"
    QT_USE_QSTRINGBUILDER
    $<$<BOOL:${SC_PLUGIN_GIT}>:SC_PLUGIN_GIT>
    $<$<BOOL:${SC_PLUGIN_PAPERLESS}>:SC_PLUGIN_PAPERLESS>
    $<$<BOOL:${SC_PLUGIN_MOUNTISO}>:SC_PLUGIN_MOUNTISO>
    $<$<BOOL:${SC_PLUGIN_MAKEFILEACTIONS}>:SC_PLUGIN_MAKEFILEACTIONS>
)

install(TARGETS splitcommander DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Übersetzungen ---
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

set(TS_FILES
    translations/splitcommander_de.ts
    translations/splitcommander_en.ts
    translations/splitcommander_fr.ts
    translations/splitcommander_es.ts
    translations/splitcommander_it.ts
    translations/splitcommander_pt.ts
    translations/splitcommander_ru.ts
    translations/splitcommander_pl.ts
    translations/splitcommander_nl.ts
    translations/splitcommander_zh_CN.ts
    translations/splitcommander_tr.ts
    translations/splitcommander_ja.ts
    translations/splitcommander_ko.ts
    translations/splitcommander_ar.ts
    translations/splitcommander_sv.ts
    translations/splitcommander_da.ts
    translations/splitcommander_fi.ts
    translations/splitcommander_nb.ts
    translations/splitcommander_cs.ts
    translations/splitcommander_sk.ts
    translations/splitcommander_hu.ts
    translations/splitcommander_ro.ts
)

qt6_add_translations(splitcommander
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)

install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/splitcommander/translations)

# --- Desktop-Datei & Icons ---
install(FILES splitcommander.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES src/splitcommander.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES src/splitcommander_32.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps RENAME splitcommander.png)
install(FILES src/splitcommander_64.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/64x64/apps RENAME splitcommander.png)
install(FILES src/splitcommander_128.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps RENAME splitcommander.png)
