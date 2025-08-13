cmake_minimum_required(VERSION 3.26)

project(VibeNote LANGUAGES CXX)

include(cmake/toolchains.cmake)
include(cmake/warnings.cmake)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_PADDLE_OCR "Enable Paddle OCR support" OFF)
option(VIBENOTE_ENABLE_TESTS "Build VibeNote test suite" ON)
option(VIBENOTE_STRICT_WARNINGS "Treat warnings as errors" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network DBus Gui Quick QuickControls2 Qml)
find_package(KF6 REQUIRED COMPONENTS I18n Config Kirigami GlobalAccel)

add_subdirectory(app)
add_subdirectory(daemon)

if(VIBENOTE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_PREFIX}/share/vibenote)

include(CPack)
