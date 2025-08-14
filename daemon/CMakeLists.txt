find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Gui HttpServer)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
pkg_check_modules(PORTAL libportal)
pkg_check_modules(TESSERACT tesseract)
pkg_check_modules(LEPTONICA lept)

set(CMAKE_AUTOMOC ON)

add_executable(vibenote_daemon
    src/main.cpp
    src/config.cpp
    src/http_server.cpp
    src/llama_client.cpp
    src/gpu_guard.cpp
    src/logging.cpp
    src/metrics.cpp
    src/queue.cpp
    src/capture/screencast_portal.cpp
    src/capture/frame_diff.cpp
    src/enrich/enrich_none.cpp
    src/enrich/enrich_openai.cpp
    src/enrich/enrich_secondary.cpp
    src/exporters/export_csv.cpp
    src/exporters/export_json.cpp
    src/exporters/export_raw.cpp
    src/ocr/ocr_paddle.cpp
    src/ocr/ocr_tesseract.cpp
    src/store/sqlite_store.cpp
    src/windows/kwin_watcher.cpp
)

target_include_directories(vibenote_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PIPEWIRE_INCLUDE_DIRS}
    ${PORTAL_INCLUDE_DIRS}
    ${TESSERACT_INCLUDE_DIRS}
    ${LEPTONICA_INCLUDE_DIRS}
)

target_link_libraries(vibenote_daemon
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Gui
    Qt6::HttpServer
    SQLite::SQLite3
    ${PIPEWIRE_LIBRARIES}
    ${PORTAL_LIBRARIES}
    ${TESSERACT_LIBRARIES}
    ${LEPTONICA_LIBRARIES}
)

install(TARGETS vibenote_daemon DESTINATION bin)
