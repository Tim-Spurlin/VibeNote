# AGENT.md

## Purpose
Headless daemon providing OCR capture, summarisation, storage, and HTTP APIs.

## Sibling items
- **src/** – C++20 sources for queues, HTTP server, storage, and integrations.
- **openapi.yaml** – REST contract consumed by the GUI and scripts.
- **CMakeLists.txt** – build rules linking QtCore, NVML, SQLite, PipeWire, and optional ONNX runtime.

## Integration
Listens only on localhost. Interacts with `third_party/llama.cpp` for language model inference and stores data under SQLite schemas in `daemon/src/store/`.
