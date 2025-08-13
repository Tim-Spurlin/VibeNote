# AGENT.md

## Purpose
Contains the Kirigami-based GUI application. Hosts the QML interface, icons, and C++ sources that communicate with the daemon.

## Sibling directories
- **qml/** – QML components for the overlay, dashboard, and settings.
- **icons/** – SVG/PNG assets embedded into the application.
- **src/** – C++20 sources implementing application logic, API access, and settings persistence.

## Integration
The GUI talks to the daemon's HTTP server over `ApiClient` and reflects configuration changes stored in the daemon. Resources are embedded through `app.qrc` and built with Qt6/KF6 via the app's CMakeLists.txt.
