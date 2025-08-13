# AGENT.md

## Purpose
Test suite covering daemon logic, GUI components, and end-to-end flows.

## Subdirectories
- **daemon/** – unit tests for queue, GPU guard, OCR, and HTTP handlers.
- **app/** – QtTest-based tests for overlay controller and API client.
- **integration/** – starts real components to test summarisation and export paths.

## Integration
Tests use mocks for NVML, PipeWire, and external APIs. Run with CTest after building.
