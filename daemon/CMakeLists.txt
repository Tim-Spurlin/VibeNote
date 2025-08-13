cmake_minimum_required(VERSION 3.26)

project(vibenote_daemon LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Network DBus)
qt_standard_project_setup()

include(${CMAKE_SOURCE_DIR}/cmake/warnings.cmake)

file(GLOB_RECURSE VIBENOTE_DAEMON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(vibenote_daemon ${VIBENOTE_DAEMON_SOURCES})

set_target_properties(vibenote_daemon PROPERTIES
    AUTOMOC ON
    BUILD_RPATH "$ORIGIN/../lib"
    INSTALL_RPATH "$ORIGIN/../lib"
)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

target_include_directories(vibenote_daemon
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_library(NVML_LIBRARY nvidia-ml)
if(NOT NVML_LIBRARY)
    message(FATAL_ERROR "nvidia-ml library not found")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PipeWire REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(Portal REQUIRED IMPORTED_TARGET libportal)
pkg_check_modules(Tesseract REQUIRED IMPORTED_TARGET tesseract)
pkg_check_modules(Leptonica REQUIRED IMPORTED_TARGET lept)

target_link_libraries(vibenote_daemon
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::DBus
        SQLite::SQLite3
        Threads::Threads
        ${NVML_LIBRARY}
        PkgConfig::PipeWire
        PkgConfig::Portal
        PkgConfig::Tesseract
        PkgConfig::Leptonica
)

if(ENABLE_PADDLE_OCR)
    find_package(ONNXRuntime REQUIRED)
    target_link_libraries(vibenote_daemon PRIVATE onnxruntime::onnxruntime)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/llama.cpp/CMakeLists.txt")
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/llama.cpp third_party/llama.cpp)
    target_link_libraries(vibenote_daemon PRIVATE llama)
endif()

vibenote_set_warnings(vibenote_daemon)

install(TARGETS vibenote_daemon
    RUNTIME DESTINATION bin
)

install(FILES openapi.yaml
    DESTINATION share/vibenote
)

