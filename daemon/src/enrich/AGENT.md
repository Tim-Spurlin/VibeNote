# AGENT.md

## Purpose
Optional enrichment modules that augment OCR text using external APIs.

## Key files
- **enrich_none.cpp** – pass-through implementation.
- **enrich_openai.cpp** – calls OpenAI with API key #1.
- **enrich_secondary.cpp** – secondary provider for metrics or alternate summaries.

## Integration
Modules are selected via configuration and run after OCR before storage or export. They must respect rate limits and never block the main thread.
