# AGENT.md

## Purpose
Platform-specific helpers for window metadata.

## Key file
- **kwin_watcher.cpp** – monitors active window via KWin DBus and supplies metadata to capture and logging systems.

## Integration
Used by capture and logging to associate OCR text with originating windows while respecting Wayland security boundaries.
