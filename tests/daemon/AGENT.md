# AGENT.md

## Purpose
GoogleTest or Catch2 unit tests for daemon subsystems: queue scheduling, GPU guard behaviour, OCR pipelines, HTTP handlers, and storage.

## Integration
Mocks NVML, PipeWire, and database to test logic deterministically. Linked against daemon objects but not the GUI.
