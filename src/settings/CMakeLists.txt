# SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>
# SPDX-License-Identifier: BSD-2-Clause

qt_add_library(vibenote_settings STATIC)
ecm_add_qml_module(vibenote_settings
    GENERATE_PLUGIN_SOURCE
    URI org.kde.vibenote.settings
    QML_FILES
        VibeNoteConfigurationView.qml
        GeneralPage.qml
)
