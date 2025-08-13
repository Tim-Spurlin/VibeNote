PRAGMA journal_mode=WAL;
PRAGMA foreign_keys=ON;

CREATE TABLE schema_version (
    version INTEGER PRIMARY KEY,
    applied_at TIMESTAMP
);

CREATE TABLE windows (
    window_id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    app_name TEXT,
    pid INTEGER,
    first_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE notes (
    note_id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TIMESTAMP NOT NULL,
    window_id INTEGER REFERENCES windows(window_id),
    raw_text TEXT,
    summary TEXT,
    enriched_summary TEXT,
    metadata JSON,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_notes_timestamp ON notes (timestamp);
CREATE INDEX idx_notes_window ON notes (window_id);

CREATE VIRTUAL TABLE notes_fts USING fts5(
    content,
    tokenize='unicode61',
    content_rowid='note_id'
);

CREATE TRIGGER notes_fts_insert AFTER INSERT ON notes
BEGIN
    INSERT INTO notes_fts(rowid, content) VALUES (new.note_id, new.summary || ' ' || new.enriched_summary);
END;

CREATE TABLE events (
    event_id INTEGER PRIMARY KEY,
    timestamp TIMESTAMP,
    event_type TEXT,
    payload JSON
);

CREATE TABLE metrics (
    metric_name TEXT,
    timestamp TIMESTAMP,
    value REAL,
    labels JSON,
    PRIMARY KEY (metric_name, timestamp)
);

INSERT INTO schema_version VALUES (1, CURRENT_TIMESTAMP);
