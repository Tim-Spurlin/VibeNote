#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QStringList>
#include <QTextStream>

#include "store/sqlite_store.h"

namespace exporters {
namespace {
QString escapeCsvField(const QString &field, QChar delimiter) {
    QString escaped = field;
    escaped.replace('"', """");
    bool needsQuotes = escaped.contains(delimiter) || escaped.contains('\n') ||
                       escaped.contains('\r') || field.contains('"');
    if (needsQuotes) {
        escaped.prepend('"');
        escaped.append('"');
    }
    return escaped;
}

void writeHeader(QTextStream &stream, const QStringList &headers, QChar delimiter) {
    stream << headers.join(delimiter) << '\n';
}

QFile *openNextFile(QFile *file, const QString &base, const QString &suffix, int index,
                    const QByteArray &bom, const QStringList &headers, QChar delimiter,
                    QTextStream &stream) {
    QString filename = base;
    if (index > 0) {
        filename += QString("_part%1").arg(index);
    }
    if (!suffix.isEmpty()) {
        filename += '.' + suffix;
    }
    file->setFileName(filename);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return nullptr;
    }
    file->write(bom);
    stream.setDevice(file);
    writeHeader(stream, headers, delimiter);
    return file;
}
} // namespace

void exportCsv(SqliteStore *store, const QDateTime &from, const QDateTime &to,
               QIODevice *output, QChar delimiter) {
    if (!store || !output) {
        return;
    }

    const QByteArray bom("\xEF\xBB\xBF");
    QFile *file = qobject_cast<QFile *>(output);
    QTextStream stream(output);
    stream.setCodec("UTF-8");
    QStringList headers = {QStringLiteral("timestamp"), QStringLiteral("window_title"),
                           QStringLiteral("app_name"), QStringLiteral("summary"),
                           QStringLiteral("enriched_summary"),
                           QStringLiteral("duration_ms")};

    output->write(bom);
    writeHeader(stream, headers, delimiter);
    qint64 bytesWritten = output->pos();

    QString base;
    QString suffix;
    if (file) {
        QFileInfo info(file->fileName());
        base = info.path() + '/' + info.completeBaseName();
        suffix = info.completeSuffix();
    }

    const auto notes = store->fetchNotes(from, to);
    int rowCount = 0;
    int fileIndex = 0;

    for (const auto &note : notes) {
        QStringList fields;
        fields << escapeCsvField(note.timestamp.toUTC().toString(Qt::ISODateWithMs), delimiter)
               << escapeCsvField(note.windowTitle, delimiter)
               << escapeCsvField(note.appName, delimiter)
               << escapeCsvField(note.summary, delimiter)
               << escapeCsvField(note.enrichedSummary, delimiter)
               << QString::number(note.durationMs);

        QString line = fields.join(delimiter);
        stream << line << '\n';
        ++rowCount;

        if (rowCount % 1000 == 0) {
            stream.flush();
        }

        bytesWritten += line.toUtf8().size() + 1;
        if (file && bytesWritten > 10 * 1024 * 1024) {
            stream.flush();
            file->close();
            ++fileIndex;
            if (!openNextFile(file, base, suffix.isEmpty() ? QStringLiteral("csv") : suffix,
                              fileIndex, bom, headers, delimiter, stream)) {
                return;
            }
            bytesWritten = file->pos();
        }
    }
    stream.flush();
}

void exportStructuredPrompts(SqliteStore *store, const QDateTime &from,
                             const QDateTime &to, QIODevice *output,
                             QChar delimiter) {
    if (!store || !output) {
        return;
    }

    const QByteArray bom("\xEF\xBB\xBF");
    QFile *file = qobject_cast<QFile *>(output);
    QTextStream stream(output);
    stream.setCodec("UTF-8");
    QStringList headers = {QStringLiteral("question"), QStringLiteral("answer"),
                           QStringLiteral("timestamp"), QStringLiteral("context"),
                           QStringLiteral("confidence")};

    output->write(bom);
    writeHeader(stream, headers, delimiter);
    qint64 bytesWritten = output->pos();

    QString base;
    QString suffix;
    if (file) {
        QFileInfo info(file->fileName());
        base = info.path() + '/' + info.completeBaseName();
        suffix = info.completeSuffix();
    }

    const auto notes = store->fetchNotes(from, to);
    int rowCount = 0;
    int fileIndex = 0;

    for (const auto &note : notes) {
        QString timestamp = note.timestamp.toUTC().toString(Qt::ISODateWithMs);
        QString context = escapeCsvField(note.windowTitle, delimiter);
        QString confidence = escapeCsvField(QString::number(note.confidence), delimiter);

        auto writeRow = [&](const QString &question, const QString &answer) {
            QStringList fields;
            fields << escapeCsvField(question, delimiter)
                   << escapeCsvField(answer, delimiter)
                   << escapeCsvField(timestamp, delimiter)
                   << context << confidence;
            QString line = fields.join(delimiter);
            stream << line << '\n';
            ++rowCount;
            bytesWritten += line.toUtf8().size() + 1;
            if (rowCount % 1000 == 0) {
                stream.flush();
            }
            if (file && bytesWritten > 10 * 1024 * 1024) {
                stream.flush();
                file->close();
                ++fileIndex;
                if (!openNextFile(file, base,
                                  suffix.isEmpty() ? QStringLiteral("csv") : suffix,
                                  fileIndex, bom, headers, delimiter, stream)) {
                    return false;
                }
                bytesWritten = file->pos();
            }
            return true;
        };

        if (!writeRow(QString("What was I doing at %1?").arg(timestamp),
                      note.summary)) {
            break;
        }
        if (!writeRow(QStringLiteral("What application was active?"),
                      note.appName)) {
            break;
        }
        if (!writeRow(QStringLiteral("Describe the activity"),
                      note.enrichedSummary)) {
            break;
        }
    }
    stream.flush();
}

} // namespace exporters

