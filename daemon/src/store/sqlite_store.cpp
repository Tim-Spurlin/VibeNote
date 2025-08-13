#include <sqlite3.h>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <mutex>
#include <stdexcept>
#include <string>

#include "logging.h"

namespace {
// Expected schema version defined in schema.sql PRAGMA user_version
constexpr int kExpectedSchemaVersion = 1;

inline void finalize(sqlite3_stmt* stmt) {
    if (stmt) {
        sqlite3_finalize(stmt);
    }
}
}  // namespace

class SqliteStore {
public:
    explicit SqliteStore(const QString& dbPath);
    ~SqliteStore();

    qint64 insertNote(qint64 timestamp, qint64 windowId, const QString& text,
                      const QString& enrichedText, const QJsonObject& metadata);

    QJsonArray queryNotes(qint64 fromTs, qint64 toTs, const QString& appFilter,
                          int limit);

    void insertWindowEvent(qint64 windowId, const QString& title,
                           const QString& appName, int pid);

    QJsonObject getStats(const QString& period);

    void vacuum();

private:
    void openDatabase(const QString& dbPath);
    void applyMigrations();
    void prepareStatements();
    void exec(const QString& sql);

    sqlite3* db_ {nullptr};
    sqlite3_stmt* insertNoteStmt_ {nullptr};
    sqlite3_stmt* insertWindowStmt_ {nullptr};

    std::mutex mutex_;
};

SqliteStore::SqliteStore(const QString& dbPath) {
    openDatabase(dbPath);
    applyMigrations();
    prepareStatements();
}

SqliteStore::~SqliteStore() {
    finalize(insertNoteStmt_);
    finalize(insertWindowStmt_);
    if (db_) {
        sqlite3_close(db_);
    }
}

void SqliteStore::openDatabase(const QString& dbPath) {
    if (sqlite3_open_v2(dbPath.toUtf8().constData(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) !=
        SQLITE_OK) {
        throw std::runtime_error("Failed to open database");
    }

    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA synchronous=NORMAL;");
    exec("PRAGMA cache_size=10000;");

    // Load schema
    QString schemaPath = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath() +
                         "/schema.sql";
    QFile schemaFile(schemaPath);
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error("Failed to open schema.sql");
    }
    QString schema = QString::fromUtf8(schemaFile.readAll());
    exec(schema);
}

void SqliteStore::exec(const QString& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.toUtf8().constData(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

void SqliteStore::applyMigrations() {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &stmt, nullptr) !=
        SQLITE_OK) {
        throw std::runtime_error("PRAGMA user_version failed");
    }
    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (version == kExpectedSchemaVersion) {
        return;
    }
    // Basic migration strategy: reapply schema for newer versions
    exec(QStringLiteral("PRAGMA user_version=%1;").arg(kExpectedSchemaVersion));
}

void SqliteStore::prepareStatements() {
    if (sqlite3_prepare_v2(
            db_,
            "INSERT INTO notes(timestamp, window_id, text, enriched_text, metadata)"
            " VALUES(?,?,?,?,?);",
            -1, &insertNoteStmt_, nullptr) != SQLITE_OK) {
        throw std::runtime_error("prepare insert_note failed");
    }
    if (sqlite3_prepare_v2(
            db_,
            "INSERT INTO windows(id, title, app_name, pid, last_seen)"
            " VALUES(?,?,?,?,strftime('%s','now'))"
            " ON CONFLICT(id) DO UPDATE SET title=excluded.title,"
            " app_name=excluded.app_name, pid=excluded.pid,"
            " last_seen=strftime('%s','now'));",
            -1, &insertWindowStmt_, nullptr) != SQLITE_OK) {
        throw std::runtime_error("prepare insert_window failed");
    }
}

qint64 SqliteStore::insertNote(qint64 timestamp, qint64 windowId,
                               const QString& text,
                               const QString& enrichedText,
                               const QJsonObject& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);

    sqlite3_reset(insertNoteStmt_);
    sqlite3_bind_int64(insertNoteStmt_, 1, timestamp);
    sqlite3_bind_int64(insertNoteStmt_, 2, windowId);
    sqlite3_bind_text(insertNoteStmt_, 3, text.toUtf8().constData(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(insertNoteStmt_, 4, enrichedText.toUtf8().constData(), -1,
                      SQLITE_TRANSIENT);
    QString metaStr = QString::fromUtf8(QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    sqlite3_bind_text(insertNoteStmt_, 5, metaStr.toUtf8().constData(), -1,
                      SQLITE_TRANSIENT);

    if (sqlite3_step(insertNoteStmt_) != SQLITE_DONE) {
        sqlite3_reset(insertNoteStmt_);
        throw std::runtime_error("insert note failed");
    }
    sqlite3_reset(insertNoteStmt_);
    return sqlite3_last_insert_rowid(db_);
}

QJsonArray SqliteStore::queryNotes(qint64 fromTs, qint64 toTs,
                                   const QString& appFilter, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);

    QString sql =
        "SELECT n.id, n.timestamp, n.window_id, n.text, n.enriched_text,"
        " n.metadata, w.title, w.app_name, w.pid"
        " FROM notes n JOIN windows w ON w.id = n.window_id"
        " WHERE n.timestamp BETWEEN ? AND ?";
    if (!appFilter.isEmpty()) {
        sql += " AND w.app_name = ?";
    }
    sql += " ORDER BY n.timestamp DESC";
    if (limit > 0) {
        sql += QStringLiteral(" LIMIT %1").arg(limit);
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.toUtf8().constData(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        throw std::runtime_error("prepare query_notes failed");
    }

    int index = 1;
    sqlite3_bind_int64(stmt, index++, fromTs);
    sqlite3_bind_int64(stmt, index++, toTs);
    if (!appFilter.isEmpty()) {
        sqlite3_bind_text(stmt, index++, appFilter.toUtf8().constData(), -1,
                          SQLITE_TRANSIENT);
    }

    QJsonArray results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QJsonObject note;
        note.insert("id", static_cast<qint64>(sqlite3_column_int64(stmt, 0)));
        note.insert("timestamp", static_cast<qint64>(sqlite3_column_int64(stmt, 1)));
        note.insert("window_id", static_cast<qint64>(sqlite3_column_int64(stmt, 2)));
        note.insert("text", QString::fromUtf8(reinterpret_cast<const char*>(
                                               sqlite3_column_text(stmt, 3))));
        note.insert(
            "enriched_text",
            QString::fromUtf8(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4))));
        QString meta = QString::fromUtf8(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        QJsonDocument metaDoc = QJsonDocument::fromJson(meta.toUtf8());
        note.insert("metadata", metaDoc.object());

        QJsonObject window;
        window.insert(
            "title",
            QString::fromUtf8(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6))));
        window.insert(
            "app_name",
            QString::fromUtf8(
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7))));
        window.insert("pid", sqlite3_column_int(stmt, 8));
        note.insert("window", window);

        results.append(note);
    }
    sqlite3_finalize(stmt);
    return results;
}

void SqliteStore::insertWindowEvent(qint64 windowId, const QString& title,
                                    const QString& appName, int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_reset(insertWindowStmt_);
    sqlite3_bind_int64(insertWindowStmt_, 1, windowId);
    sqlite3_bind_text(insertWindowStmt_, 2, title.toUtf8().constData(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(insertWindowStmt_, 3, appName.toUtf8().constData(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_int(insertWindowStmt_, 4, pid);

    if (sqlite3_step(insertWindowStmt_) != SQLITE_DONE) {
        sqlite3_reset(insertWindowStmt_);
        throw std::runtime_error("insert window event failed");
    }
    sqlite3_reset(insertWindowStmt_);
}

QJsonObject SqliteStore::getStats(const QString& period) {
    std::lock_guard<std::mutex> lock(mutex_);
    QString sql =
        "SELECT COUNT(*), MIN(timestamp), MAX(timestamp) FROM notes"
        " WHERE timestamp > strftime('%s','now','-1 day')";
    if (period == QStringLiteral("week")) {
        sql =
            "SELECT COUNT(*), MIN(timestamp), MAX(timestamp) FROM notes"
            " WHERE timestamp > strftime('%s','now','-7 day')";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.toUtf8().constData(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
        throw std::runtime_error("prepare stats failed");
    }
    QJsonObject stats;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats.insert("notes", sqlite3_column_int(stmt, 0));
        stats.insert("from", static_cast<qint64>(sqlite3_column_int64(stmt, 1)));
        stats.insert("to", static_cast<qint64>(sqlite3_column_int64(stmt, 2)));
    }
    sqlite3_finalize(stmt);
    return stats;
}

void SqliteStore::vacuum() {
    std::lock_guard<std::mutex> lock(mutex_);
    exec("VACUUM;");
}

// Integration notes:
// SqliteStore is utilised by HTTP handlers, exporters and enrichment modules.
// All access is serialized through the internal mutex to ensure thread-safety.

