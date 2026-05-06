cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(SplitCommander VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)
find_package(KF6 REQUIRED COMPONENTS
    I18n IconThemes ConfigWidgets XmlGui KIO
    WidgetsAddons WindowSystem Solid Archive Service
)

add_executable(splitcommander
    src/main.cpp
    src/mainwindow.cpp
    src/filetablemodel.cpp
    src/filepane.cpp
    src/previewpanel.cpp
    src/sidebar.cpp
    src/tagmanager.cpp
    src/propertiesdialog.cpp
    src/batchrenamer.cpp
    src/jobprogresswidget.cpp
    src/fileoperations.cpp
    src/settingsdialog.cpp
    src/themedialog.cpp
    src/agebadgedialog.cpp
    src/thememanager.cpp
    src/terminalutils.cpp
    src/shortcutdialog.cpp
)

target_link_libraries(splitcommander
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg
    KF6::I18n KF6::IconThemes KF6::ConfigWidgets KF6::XmlGui
    KF6::KIOCore KF6::KIOWidgets KF6::KIOFileWidgets KF6::WidgetsAddons
    KF6::WindowSystem KF6::Solid KF6::Archive KF6::Service
)

install(TARGETS splitcommander DESTINATION ${CMAKE_INSTALL_BINDIR})
