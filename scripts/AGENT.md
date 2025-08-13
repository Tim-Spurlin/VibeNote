# AGENT.md

## Purpose
Shell utilities for development, setup, benchmarking, and exporting notes.

## Key scripts
- **setup-arch.sh** – installs dependencies, downloads models, deploys services.
- **build.sh** – configures and builds the project.
- **run-dev.sh** – launches llama server, daemon, and GUI for development.
- **bench.sh** – benchmarks summarisation and GPU usage.
- **export-examples.sh** – demonstrates export API outputs.

## Integration
Scripts invoke the compiled binaries in `app` and `daemon` and rely on `third_party/llama.cpp` and `models/` when applicable.
