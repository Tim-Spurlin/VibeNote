cmake_minimum_required(VERSION 3.26)

project(VibeNoteApp LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Qml)
find_package(KF6 REQUIRED COMPONENTS I18n ConfigCore ConfigGui Kirigami GlobalAccel)

qt_policy(SET QTP0001 NEW)
qt_standard_project_setup()

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS src/*.cpp)
qt_add_executable(vibenote_app
    MANUAL_FINALIZATION
    ${APP_SOURCES}
)

if(WIN32)
    set_target_properties(vibenote_app PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(APPLE)
    set_target_properties(vibenote_app PROPERTIES MACOSX_BUNDLE TRUE)
endif()

qt_add_resources(vibenote_app "resources"
    PREFIX "/"
    FILES app.qrc
)

file(GLOB_RECURSE QML_FILES CONFIGURE_DEPENDS qml/*.qml)
qt_add_qml_module(vibenote_app
    URI org.saphyre.vibenote
    VERSION 1.0
    QML_FILES ${QML_FILES}
)

target_link_libraries(vibenote_app
    PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Qml
        KF6::I18n KF6::ConfigCore KF6::ConfigGui
        KF6::Kirigami KF6::GlobalAccel
)

vibenote_set_warnings(vibenote_app)

install(TARGETS vibenote_app
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/
)

install(FILES org.saphyre.VibeNote.desktop
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications/
)

install(DIRECTORY icons/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/
)

qt_finalize_executable(vibenote_app)
