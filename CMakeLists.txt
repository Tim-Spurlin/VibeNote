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
find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 REQUIRED COMPONENTS
  Core Gui Qml Quick QuickControls2 Svg
)

find_package(KF6 REQUIRED COMPONENTS
  CoreAddons
  I18n
  Kirigami
  GuiAddons
  Config
  KIO
  IconThemes
  QQC2DesktopStyle
)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)


add_subdirectory(app)
add_subdirectory(daemon)

if(VIBENOTE_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_PREFIX}/share/vibenote)

include(CPack)
