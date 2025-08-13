# AGENT.md

## Purpose
QML files defining all visual components: Main window, overlay, dashboard pages, and settings dialogs.

## Interaction
Components bind to C++ singletons (SettingsStore, ApiClient, OverlayController) via `qmlRegisterSingletonInstance`. Signals and slots propagate user actions to the daemon and render responses.

## Notes
Keep QML accessible and Wayland compliant. Use Kirigami style components and avoid blocking operations.
