cmake_minimum_required(VERSION 3.26)
project(VibeNoteApp LANGUAGES CXX)

# CRITICAL: Set AUTOMOC before any targets are created
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Qml Widgets)
find_package(KF6 REQUIRED COMPONENTS I18n Config Kirigami GlobalAccel)

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)  # Use new QML resource prefix

# Source files
set(APP_SOURCES
    src/main.cpp
    src/api_client.cpp
    src/metrics_view.cpp
    src/overlay_controller.cpp
    src/settings_store.cpp
)

# Header files (explicitly listed for MOC)
set(APP_HEADERS
    src/api_client.h
    src/metrics_view.h
    src/overlay_controller.h
    src/settings_store.h
)

# Create executable
qt_add_executable(vibenote_app
    ${APP_SOURCES}
    ${APP_HEADERS}
)

# QML module - USE RELATIVE PATHS, NOT GLOB!
qt_add_qml_module(vibenote_app
    URI org.saphyre.vibenote
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/DashboardPage.qml
        qml/OverlayWindow.qml
)

# Resources
qt_add_resources(vibenote_app "resources"
    PREFIX "/"
    FILES app.qrc
)

# Link libraries
target_link_libraries(vibenote_app PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Qml
    Qt6::Widgets
    KF6::I18n
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::Kirigami
    KF6::GlobalAccel
)

# Include directories
target_include_directories(vibenote_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Finalize executable
qt_finalize_executable(vibenote_app)

# Install targets
install(TARGETS vibenote_app
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
)
