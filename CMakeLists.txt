cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(splitcommander VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Concurrent)
find_package(KF6 REQUIRED COMPONENTS
    I18n IconThemes ConfigWidgets XmlGui KIO
    WidgetsAddons WindowSystem Solid Archive Service CoreAddons
)

add_executable(splitcommander
    src/main.cpp
    src/panewidgets.cpp
    src/mainwindow.cpp
    src/filetablemodel.cpp
    src/filepane.cpp
    src/joboverlay.cpp
    src/previewpanel.cpp
    src/sidebar.cpp
    src/tagmanager.cpp
    src/batchrenamer.cpp
    src/settingsdialog.cpp
    src/themedialog.cpp
    src/agebadgedialog.cpp
    src/thememanager.cpp
    src/terminalutils.cpp
    src/shortcutdialog.cpp
    src/dialogutils.cpp
)

target_link_libraries(splitcommander
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg Qt6::Concurrent
    KF6::I18n KF6::IconThemes KF6::ConfigWidgets KF6::XmlGui
    KF6::KIOCore KF6::KIOWidgets KF6::KIOFileWidgets KF6::WidgetsAddons
    KF6::WindowSystem KF6::Solid KF6::Archive KF6::Service KF6::CoreAddons
)

target_compile_definitions(splitcommander PRIVATE
    SC_VERSION="${PROJECT_VERSION}"
    QT_USE_QSTRINGBUILDER
)

install(TARGETS splitcommander DESTINATION ${CMAKE_INSTALL_BINDIR})
