#include <QDateTime>
#include <QIODevice>
#include <QString>

#include "store/sqlite_store.h"

namespace exporters {
void exportCsv(SqliteStore *store, const QDateTime &from, const QDateTime &to,
               QIODevice *output, QChar delimiter = ',');
void exportStructuredPrompts(SqliteStore *store, const QDateTime &from,
                             const QDateTime &to, QIODevice *output,
                             QChar delimiter = ',');
} // namespace exporters

class HttpServer {
public:
    explicit HttpServer(SqliteStore *store) : store_(store) {}

    void exportNotes(const QString &format, const QDateTime &from,
                     const QDateTime &to, QIODevice *output,
                     QChar delimiter = ',');

private:
    SqliteStore *store_;
};

void HttpServer::exportNotes(const QString &format, const QDateTime &from,
                             const QDateTime &to, QIODevice *output,
                             QChar delimiter) {
    if (!output) {
        return;
    }

    if (format == QLatin1String("csv")) {
        exporters::exportCsv(store_, from, to, output, delimiter);
    } else if (format == QLatin1String("structured_prompts")) {
        exporters::exportStructuredPrompts(store_, from, to, output, delimiter);
    }
}
