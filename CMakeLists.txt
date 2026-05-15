cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(splitcommander VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Concurrent DBus)
find_package(KF6 REQUIRED COMPONENTS
    I18n IconThemes ConfigWidgets XmlGui KIO
    WidgetsAddons WindowSystem Solid Archive Service CoreAddons Baloo
)

add_executable(splitcommander
    src/main.cpp
    src/panewidgets.cpp
    src/mainwindow.cpp
    src/panetoolbar.cpp
    src/millercolumn.cpp
    src/millerarea.cpp
    src/panewidget.cpp
    src/filepane.cpp
    src/filepanedelegate.cpp
    src/drivedelegate.cpp
    src/joboverlay.cpp
    src/previewpanel.cpp
    src/drophandler.cpp
    src/filemanager1.cpp
    src/drivemanager.cpp
    src/sidebar.cpp
    src/addnetworkdialog.cpp
    src/sidebargroups.cpp
    src/tagmanager.cpp
    src/batchrenamer.cpp
    src/config.cpp
    src/themedialog.cpp
    src/agebadgedialog.cpp
    src/thememanager.cpp
    src/dialogutils.cpp
    src/thumbnailmanager.cpp
    src/hoverfader.cpp
)

target_link_libraries(splitcommander
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Concurrent Qt6::DBus
    KF6::I18n KF6::IconThemes KF6::IconWidgets KF6::ConfigWidgets KF6::XmlGui
    KF6::KIOCore KF6::KIOWidgets KF6::KIOFileWidgets KF6::WidgetsAddons
    KF6::WindowSystem KF6::Solid KF6::Archive KF6::Service KF6::CoreAddons KF6::Baloo
)

target_compile_definitions(splitcommander PRIVATE
    SC_VERSION="${PROJECT_VERSION}"
    QT_USE_QSTRINGBUILDER
)

install(TARGETS splitcommander DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Übersetzungen ---
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

set(TS_FILES
    translations/splitcommander_de.ts
    translations/splitcommander_en.ts
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
