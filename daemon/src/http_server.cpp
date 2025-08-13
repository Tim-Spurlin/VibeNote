#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QBuffer>
#include <QFile>
#include <QDateTime>
#include <QIODevice>
#include <QHostAddress>
#include <QUrlQuery>

#include "queue.h"
#include "llama_client.h"
#include "store/sqlite_store.h"
#include "exporters/export_raw.h"
#include "exporters/export_json.h"
#include "exporters/export_csv.h"
#include "exporters/export_structured_prompts.h"
#include "enrich/enrich_none.h"
#include "enrich/enrich_openai.h"
#include "enrich/enrich_secondary.h"
#include "metrics.h"
#include "config.h"
#include "gpu_guard.h"
#include "capture/screencast_portal.h"

// HttpServer exposes REST and metrics endpoints. It validates all
// incoming data against the OpenAPI schema and coordinates with
// queue, llama client, SQLite store, enrichment modules and
// exporters. Responses include CORS headers restricted to local
// origins. All requests are logged with timing and status codes.

class HttpServer : public QObject {
    Q_OBJECT
public:
    HttpServer(quint16 port,
               Queue* queue,
               LlamaClient* llama,
               SqliteStore* store,
               Config* config,
               QObject* parent = nullptr)
        : QObject(parent),
          m_port(port),
          m_queue(queue),
          m_llama(llama),
          m_store(store),
          m_config(config),
          m_metrics(config ? config->metrics() : nullptr) {
        setupRoutes();
        m_server.listen(QHostAddress::LocalHost, m_port);
    }

private:
    QHttpServer m_server;
    quint16 m_port;
    Queue* m_queue;
    LlamaClient* m_llama;
    SqliteStore* m_store;
    Config* m_config;
    Metrics* m_metrics;
    mutable QMutex m_mutex;

    using ReqHandler = std::function<QHttpServerResponse(const QHttpServerRequest&)>;

    void setupRoutes() {
        m_server.route("/v1/status", QHttpServerRequest::Method::Get,
                       wrap(&HttpServer::handleStatus));
        m_server.route("/v1/summarize", QHttpServerRequest::Method::Post,
                       wrap(&HttpServer::handleSummarize));
        m_server.route("/v1/notes", QHttpServerRequest::Method::Get,
                       wrap(&HttpServer::handleNotes));
        m_server.route("/v1/export", QHttpServerRequest::Method::Get,
                       wrap(&HttpServer::handleExport));
        m_server.route("/v1/watch/start", QHttpServerRequest::Method::Post,
                       wrap(&HttpServer::handleWatchStart));
        m_server.route("/v1/watch/stop", QHttpServerRequest::Method::Post,
                       wrap(&HttpServer::handleWatchStop));
        m_server.route("/v1/config", QHttpServerRequest::Method::Get,
                       wrap(&HttpServer::handleGetConfig));
        m_server.route("/v1/config", QHttpServerRequest::Method::Put,
                       wrap(&HttpServer::handleSetConfig));
        m_server.route("/metrics", QHttpServerRequest::Method::Get,
                       wrap(&HttpServer::handleMetrics));
    }

    ReqHandler wrap(QHttpServerResponse (HttpServer::*func)(const QHttpServerRequest&)) {
        return [this, func](const QHttpServerRequest& req) {
            QElapsedTimer t;
            t.start();
            QHttpServerResponse resp = (this->*func)(req);
            resp.addHeader("Access-Control-Allow-Origin", "http://localhost");
            resp.addHeader("Access-Control-Allow-Headers", "content-type");
            qInfo().nospace() << req.method() << " " << req.url().toString() << " -> "
                               << static_cast<int>(resp.statusCode()) << " in "
                               << t.elapsed() << "ms";
            return resp;
        };
    }

    QHttpServerResponse handleStatus(const QHttpServerRequest&) {
        QJsonObject gpu;
        if (m_config && m_config->gpuGuard()) {
            const auto stats = m_config->gpuGuard()->stats();
            gpu["utilization"] = stats.utilization;
            gpu["memoryUsed"] = stats.memoryUsed;
        }

        QJsonObject model;
        if (m_llama) {
            model = m_llama->status();
        }

        QJsonObject obj{
            {"queueDepth", m_queue ? m_queue->depth() : 0},
            {"gpu", gpu},
            {"model", model},
            {"watch", m_config ? m_config->watchEnabled() : false},
        };
        return QHttpServerResponse(QJsonDocument(obj).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    }

    QHttpServerResponse handleSummarize(const QHttpServerRequest& req) {
        if (!m_queue || !m_queue->canAccept()) {
            return makeError(QHttpServerResponse::StatusCode::TooManyRequests,
                             "queue overloaded");
        }

        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(req.body(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            return makeError(QHttpServerResponse::StatusCode::BadRequest,
                             "invalid json");
        }
        const QJsonObject obj = doc.object();
        const auto promptVal = obj.value("prompt");
        if (!promptVal.isString()) {
            return makeError(QHttpServerResponse::StatusCode::BadRequest,
                             "prompt required");
        }
        const QString prompt = promptVal.toString();

        QIODevice* stream = nullptr;
        if (m_queue) {
            stream = m_queue->enqueue(prompt, m_llama, m_config);
        }
        if (!stream) {
            return makeError(QHttpServerResponse::StatusCode::InternalServerError,
                             "unable to enqueue");
        }
        return QHttpServerResponse(stream);
    }

    QHttpServerResponse handleNotes(const QHttpServerRequest& req) {
        QUrlQuery query(req.url());
        const QString from = query.queryItemValue("from");
        const QString to = query.queryItemValue("to");
        const QString app = query.queryItemValue("app");

        QList<Note> notes;
        if (m_store) {
            notes = m_store->query(from, to, app);
        }
        QJsonArray arr;
        for (const auto& n : notes) {
            arr.push_back(n.toJson());
        }
        return QHttpServerResponse(QJsonDocument(arr).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    }

    QHttpServerResponse handleExport(const QHttpServerRequest& req) {
        QUrlQuery query(req.url());
        const QString format = query.queryItemValue("format");
        const QString from = query.queryItemValue("from");
        const QString to = query.queryItemValue("to");
        const QString app = query.queryItemValue("app");

        QIODevice* out = nullptr;
        QString contentType = "application/octet-stream";

        if (format == "raw") {
            out = export_raw(m_store, from, to, app);
            contentType = "text/plain";
        } else if (format == "json") {
            out = export_json(m_store, from, to, app);
            contentType = "application/json";
        } else if (format == "csv") {
            out = export_csv(m_store, from, to, app);
            contentType = "text/csv";
        } else if (format == "structured_prompts") {
            out = export_structured_prompts(m_store, from, to, app);
            contentType = "application/json";
        } else {
            return makeError(QHttpServerResponse::StatusCode::BadRequest,
                             "unsupported format");
        }

        if (!out) {
            return makeError(QHttpServerResponse::StatusCode::InternalServerError,
                             "export failed");
        }

        QHttpServerResponse resp(out);
        resp.addHeader("Content-Type", contentType);
        return resp;
    }

    QHttpServerResponse handleWatchStart(const QHttpServerRequest&) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_config) {
                m_config->setWatchEnabled(true);
            }
        }
        ScreencastPortal::instance()->start();
        QJsonObject obj{{"watch", true}};
        return QHttpServerResponse(QJsonDocument(obj).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    }

    QHttpServerResponse handleWatchStop(const QHttpServerRequest&) {
        ScreencastPortal::instance()->stop();
        {
            QMutexLocker locker(&m_mutex);
            if (m_config) {
                m_config->setWatchEnabled(false);
            }
        }
        QJsonObject obj{{"watch", false}};
        return QHttpServerResponse(QJsonDocument(obj).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    }

    QHttpServerResponse handleGetConfig(const QHttpServerRequest&) {
        QJsonObject obj;
        if (m_config) {
            obj = m_config->toJson();
        }
        return QHttpServerResponse(QJsonDocument(obj).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    }

    QHttpServerResponse handleSetConfig(const QHttpServerRequest& req) {
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(req.body(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            return makeError(QHttpServerResponse::StatusCode::BadRequest,
                             "invalid json");
        }
        {
            QMutexLocker locker(&m_mutex);
            if (m_config && !m_config->apply(doc.object())) {
                return makeError(QHttpServerResponse::StatusCode::BadRequest,
                                 "invalid config");
            }
        }
        QJsonObject obj{{"updated", true}};
        return QHttpServerResponse(QJsonDocument(obj).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    }

    QHttpServerResponse handleMetrics(const QHttpServerRequest&) {
        QByteArray body;
        if (m_metrics) {
            body = m_metrics->toPrometheus().toUtf8();
        }
        QHttpServerResponse resp(QHttpServerResponse::StatusCode::Ok);
        resp.setHeader("Content-Type", "text/plain; version=0.0.4");
        resp.setBody(body);
        return resp;
    }

    QHttpServerResponse makeError(QHttpServerResponse::StatusCode code,
                                  const QString& message) {
        QJsonObject obj{{"error", message}};
        return QHttpServerResponse(QJsonDocument(obj).toJson(), code);
    }
};

#include "http_server.moc"
