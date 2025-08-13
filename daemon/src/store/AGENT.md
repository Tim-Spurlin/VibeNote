# AGENT.md

## Purpose
Persistent storage using SQLite with WAL and FTS5.

## Key files
- **sqlite_store.cpp** – database wrapper using prepared statements.
- **schema.sql** – versioned schema with triggers.

## Integration
Stores notes, configurations, and metadata consumed by exporters and API handlers. Accessed via thread-safe connections.
