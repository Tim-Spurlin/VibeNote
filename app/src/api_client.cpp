#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

class ApiClient : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(const QString &url = QStringLiteral("http://127.0.0.1:18080"), QObject *parent = nullptr);

    Q_INVOKABLE void getStatus();
    Q_INVOKABLE void summarize(const QString &prompt);
    Q_INVOKABLE void fetchNotes(const QDateTime &from, const QDateTime &to);
    Q_INVOKABLE void exportNotes(const QString &format, const QDateTime &from, const QDateTime &to);
    Q_INVOKABLE void toggleWatchMode(bool enable);
    Q_INVOKABLE void updateConfig(const QJsonObject &config);
    Q_INVOKABLE void fetchMetrics();

signals:
    void statusReceived(const QJsonObject &status);
    void summarizeComplete(const QString &result);
    void notesReceived(const QJsonArray &notes);
    void exportCompleted(const QString &path);
    void watchModeToggled(bool enabled);
    void configUpdated();
    void metricsReceived(const QVariantMap &metrics);
    void error(const QString &message);

private slots:
    void handleStatusResponse();
    void handleSummarizeReadyRead();
    void handleSummarizeFinished();
    void handleNotesResponse();
    void handleExportResponse();
    void handleWatchModeResponse();
    void handleUpdateConfigResponse();
    void handleMetricsResponse();
    void handleNetworkError(QNetworkReply::NetworkError code);

private:
    QNetworkReply *makeGet(const QString &path, const QUrlQuery &query = QUrlQuery());
    QNetworkReply *makePost(const QString &path, const QJsonDocument &doc = QJsonDocument());
    QNetworkReply *makePut(const QString &path, const QJsonDocument &doc = QJsonDocument());
    void setupTimeout(QNetworkReply *reply);

    QNetworkAccessManager *network_manager;
    QString base_url;
    QMap<QNetworkReply *, QString> pending_requests;
};

ApiClient::ApiClient(const QString &url, QObject *parent)
    : QObject(parent), network_manager(new QNetworkAccessManager(this)), base_url(url) {}

void ApiClient::setupTimeout(QNetworkReply *reply) {
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, reply]() {
        if (reply->isRunning()) {
            reply->abort();
            emit error(QStringLiteral("Request to %1 timed out").arg(reply->url().toString()));
        }
    });
    timer->start(30000);
}

QNetworkReply *ApiClient::makeGet(const QString &path, const QUrlQuery &query) {
    QUrl url(base_url + path);
    QUrlQuery q = query;
    url.setQuery(q);
    QNetworkRequest req(url);
    auto *reply = network_manager->get(req);
    connect(reply, &QNetworkReply::errorOccurred, this, &ApiClient::handleNetworkError);
    setupTimeout(reply);
    return reply;
}

QNetworkReply *ApiClient::makePost(const QString &path, const QJsonDocument &doc) {
    QUrl url(base_url + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    auto *reply = network_manager->post(req, doc.toJson());
    connect(reply, &QNetworkReply::errorOccurred, this, &ApiClient::handleNetworkError);
    setupTimeout(reply);
    return reply;
}

QNetworkReply *ApiClient::makePut(const QString &path, const QJsonDocument &doc) {
    QUrl url(base_url + path);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    auto *reply = network_manager->put(req, doc.toJson());
    connect(reply, &QNetworkReply::errorOccurred, this, &ApiClient::handleNetworkError);
    setupTimeout(reply);
    return reply;
}

void ApiClient::getStatus() {
    auto *reply = makeGet(QStringLiteral("/v1/status"));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleStatusResponse);
}

void ApiClient::summarize(const QString &prompt) {
    QJsonObject obj{{QStringLiteral("prompt"), prompt}};
    auto *reply = makePost(QStringLiteral("/v1/summarize"), QJsonDocument(obj));
    pending_requests.insert(reply, QString());
    connect(reply, &QIODevice::readyRead, this, &ApiClient::handleSummarizeReadyRead);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleSummarizeFinished);
}

void ApiClient::fetchNotes(const QDateTime &from, const QDateTime &to) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"), from.toUTC().toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("to"), to.toUTC().toString(Qt::ISODate));
    auto *reply = makeGet(QStringLiteral("/v1/notes"), query);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleNotesResponse);
}

void ApiClient::exportNotes(const QString &format, const QDateTime &from, const QDateTime &to) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("from"), from.toUTC().toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("to"), to.toUTC().toString(Qt::ISODate));
    auto *reply = makeGet(QStringLiteral("/v1/export"), query);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleExportResponse);
}

void ApiClient::toggleWatchMode(bool enable) {
    auto *reply = makePost(enable ? QStringLiteral("/v1/watch/start")
                                  : QStringLiteral("/v1/watch/stop"));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleWatchModeResponse);
}

void ApiClient::updateConfig(const QJsonObject &config) {
    auto *reply = makePut(QStringLiteral("/v1/config"), QJsonDocument(config));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleUpdateConfigResponse);
}

void ApiClient::fetchMetrics() {
    auto *reply = makeGet(QStringLiteral("/metrics"));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleMetricsResponse);
}

void ApiClient::handleStatusResponse() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (reply->error() != QNetworkReply::NoError || !doc.isObject()) {
        emit error(reply->error() == QNetworkReply::NoError ? QStringLiteral("Invalid status response")
                                                            : reply->errorString());
    } else {
        emit statusReceived(doc.object());
    }
    reply->deleteLater();
}

void ApiClient::handleSummarizeReadyRead() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply || !pending_requests.contains(reply)) {
        return;
    }
    pending_requests[reply].append(QString::fromUtf8(reply->readAll()));
}

void ApiClient::handleSummarizeFinished() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    const QString result = pending_requests.take(reply);
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    } else {
        emit summarizeComplete(result);
    }
    reply->deleteLater();
}

void ApiClient::handleNotesResponse() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (reply->error() != QNetworkReply::NoError || !doc.isArray()) {
        emit error(reply->error() == QNetworkReply::NoError ? QStringLiteral("Invalid notes response")
                                                            : reply->errorString());
    } else {
        emit notesReceived(doc.array());
    }
    reply->deleteLater();
}

void ApiClient::handleExportResponse() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
        reply->deleteLater();
        return;
    }
    QTemporaryFile file(QDir::tempPath() + "/vibenote_exportXXXXXX");
    if (!file.open()) {
        emit error(QStringLiteral("Failed to create export file"));
    } else {
        file.write(reply->readAll());
        file.setAutoRemove(false);
        const QString path = file.fileName();
        file.close();
        emit exportCompleted(path);
    }
    reply->deleteLater();
}

void ApiClient::handleWatchModeResponse() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    } else {
        emit watchModeToggled(reply->url().path().endsWith("start"));
    }
    reply->deleteLater();
}

void ApiClient::handleUpdateConfigResponse() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    } else {
        emit configUpdated();
    }
    reply->deleteLater();
}

void ApiClient::handleMetricsResponse() {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    const QString text = QString::fromUtf8(reply->readAll());
    QVariantMap metrics;
    const auto lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith('#')) {
            continue;
        }
        const auto parts = line.split(' ');
        if (parts.size() == 2) {
            bool ok = false;
            const double value = parts[1].toDouble(&ok);
            if (ok) {
                metrics.insert(parts[0], value);
            }
        }
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
    } else {
        emit metricsReceived(metrics);
    }
    reply->deleteLater();
}

void ApiClient::handleNetworkError(QNetworkReply::NetworkError) {
    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    emit error(reply->errorString());
    pending_requests.remove(reply);
    reply->deleteLater();
}

#include "api_client.moc"
