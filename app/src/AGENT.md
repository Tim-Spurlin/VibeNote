# AGENT.md

## Purpose
C++20 sources for the GUI application including entry point, overlay controller, API client, settings store, and metrics view.

## Key files
- **main.cpp** – sets up application, registers singletons, loads QML.
- **overlay_controller.cpp** – toggles overlay, handles user input.
- **api_client.cpp** – asynchronous HTTP client to the daemon.
- **settings_store.cpp** – persists user preferences and notifies daemon.
- **metrics_view.cpp** – displays daemon metrics.

## Integration
Links against Qt6 and KF6 modules. Communicates with daemon endpoints defined in `daemon/openapi.yaml`.
