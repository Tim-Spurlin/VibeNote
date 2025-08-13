# AGENT.md

## Purpose
Core daemon sources implementing capture pipelines, task queue, GPU guard, HTTP server, storage, logging, metrics, and config handling.

## Key modules
- **main.cpp** – initialises subsystems and event loop.
- **http_server.cpp** – exposes REST and metrics endpoints.
- **queue.cpp** – priority job scheduler coordinating with GpuGuard.
- **gpu_guard.cpp** – monitors NVML utilisation and throttles queue.
- **ocr/** – OCR engines and capture helpers.
- **store/** – SQLite persistence layer.
- **exporters/** – data export formats.

## Integration
Uses NVML, PipeWire, ONNX Runtime (optional) and SQLite. Communicates with llama.cpp via `llama_client.cpp` and serves the GUI through localhost HTTP.
