# AGENT.md

## Purpose
OCR engines translating captured frames into text.

## Key files
- **ocr_engine.h** – factory interface.
- **ocr_tesseract.cpp** – CPU path using Tesseract.
- **ocr_paddle.cpp** – GPU-accelerated path using ONNX Runtime and PaddleOCR.

## Integration
Receives images from `capture/`, outputs text to enrichment modules and storage. GPU path respects NVML limits controlled by `gpu_guard.cpp`.
