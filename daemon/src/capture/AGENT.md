# AGENT.md

## Purpose
Capture utilities for obtaining screen frames and detecting changes before OCR.

## Key files
- **screencast_portal.cpp** – Wayland capture via xdg-desktop-portal and PipeWire.
- **frame_diff.cpp** – computes changed regions to minimise OCR work.

## Integration
Feeds images into the OCR engines under `ocr/` and respects user privacy by capturing only active windows.
