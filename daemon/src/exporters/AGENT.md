# AGENT.md

## Purpose
Implement export formats for stored notes.

## Key files
- **export_raw.cpp** – streams raw text.
- **export_json.cpp** – emits structured JSON records.
- **export_csv.cpp** – outputs CSV for spreadsheets.

## Integration
Exporters read from `store/sqlite_store.cpp` using prepared statements and support chunked streaming over HTTP.
