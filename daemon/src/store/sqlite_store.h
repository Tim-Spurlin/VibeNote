#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QDateTime>

namespace vibenote {

class SqliteStore : public QObject {
    Q_OBJECT

public:
    explicit SqliteStore(const QString &path, QObject *parent = nullptr);
    ~SqliteStore();
    
    bool open();
    void close();
    
    bool storeNote(const QJsonObject &note);
    QJsonArray queryNotes(qint64 from, qint64 to, const QString &app = QString(), int limit = 100);
    QJsonObject getStats();
    bool vacuum();

signals:
    void noteStored(const QJsonObject &note);
    void error(const QString &message);

private:
    bool createTables();
    
    QString db_path_;
    QSqlDatabase db_;
};

} // namespace vibenote
