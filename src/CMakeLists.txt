# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>

add_subdirectory(settings)

qt_add_library(vibenote_static STATIC)
ecm_add_qml_module(vibenote_static
    URI org.kde.vibenote
    GENERATE_PLUGIN_SOURCE
    QML_FILES
        qml/Main.qml
    DEPENDENCIES
        org.kde.kirigamiaddons.formcard
        org.kde.kirigamiaddons.statefulapp
        org.kde.kirigami
        org.kde.vibenote.settings
)

target_sources(vibenote_static PRIVATE
    vibenoteapplication.cpp
    vibenoteapplication.h
)

target_link_libraries(vibenote_static PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    KF6::I18n
    KF6::I18nQml
    KF6::IconThemes
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigGui
    KirigamiAddonsStatefulApp
)
target_include_directories(vibenote_static PUBLIC ${CMAKE_BINARY_DIR})

if (ANDROID)
    kirigami_package_breeze_icons(ICONS
        list-add
        help-about
        application-exit
        applications-graphics
    )
else()
    target_link_libraries(vibenote_static PUBLIC Qt::Widgets)
endif()

kconfig_add_kcfg_files(vibenote_static GENERATE_MOC vibenoteconfig.kcfgc)

add_executable(vibenote main.cpp)
target_link_libraries(vibenote PUBLIC
    vibenote_static
    vibenote_staticplugin
    vibenote_settings
    vibenote_settingsplugin
)
install(TARGETS vibenote ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
ecm_add_android_apk(vibenote ANDROID_DIR ${CMAKE_SOURCE_DIR}/android)
