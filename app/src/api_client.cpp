#include "api_client.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringLiteral>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

ApiClient::ApiClient(QObject *parent) 
    : ApiClient(QStringLiteral("http://127.0.0.1:18080"), parent) {}

ApiClient::ApiClient(const QString &url, QObject *parent)
    : QObject(parent), base_url_(url) {}

QNetworkReply *ApiClient::makeGet(const QString &path) {
    QUrl url(base_url_ + path);
    QNetworkRequest request(url);
    return manager_.get(request);
}

QNetworkReply *ApiClient::makePost(const QString &path, const QJsonDocument &doc) {
    QUrl url(base_url_ + path);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return manager_.post(request, doc.toJson());
}

void ApiClient::getStatus() {
    auto *reply = makeGet(QStringLiteral("/v1/status"));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleStatusResponse);
}

void ApiClient::summarize(const QString &prompt, const QJsonObject &params) {
    QJsonObject obj = params;
    obj[QStringLiteral("text")] = prompt;
    auto *reply = makePost(QStringLiteral("/v1/summarize"), QJsonDocument(obj));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleSummarizeFinished);
}

void ApiClient::fetchNotes(const QDateTime &from, const QDateTime &to) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"), QString::number(from.toSecsSinceEpoch()));
    query.addQueryItem(QStringLiteral("to"), QString::number(to.toSecsSinceEpoch()));
    
    QUrl url(base_url_ + QStringLiteral("/v1/notes"));
    url.setQuery(query);
    QNetworkRequest request(url);
    auto *reply = manager_.get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleNotesResponse);
}

void ApiClient::exportData(const QString &format, const QDateTime &from, const QDateTime &to) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("format"), format);
    query.addQueryItem(QStringLiteral("from"), from.toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("to"), to.toString(Qt::ISODate));
    
    QUrl url(base_url_ + QStringLiteral("/v1/export"));
    url.setQuery(query);
    QNetworkRequest request(url);
    auto *reply = manager_.get(request);
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleExportResponse);
}

void ApiClient::toggleWatchMode(bool enable) {
    QString endpoint = enable ? QStringLiteral("/v1/watch/start") : QStringLiteral("/v1/watch/stop");
    auto *reply = makePost(endpoint, QJsonDocument());
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleWatchModeResponse);
}

void ApiClient::updateConfig(const QJsonObject &config) {
    auto *reply = makePost(QStringLiteral("/v1/config"), QJsonDocument(config));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleUpdateConfigResponse);
}

void ApiClient::fetchMetrics() {
    auto *reply = makeGet(QStringLiteral("/metrics"));
    connect(reply, &QNetworkReply::finished, this, &ApiClient::handleMetricsResponse);
}

void ApiClient::handleStatusResponse() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        Q_EMIT statusReceived(doc.object());
    } else {
        Q_EMIT error(reply->errorString());
    }
    reply->deleteLater();
}

void ApiClient::handleSummarizeFinished() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString result = doc.object()[QStringLiteral("summary")].toString();
        Q_EMIT summarizeComplete(result);
    } else {
        Q_EMIT error(reply->errorString());
    }
    reply->deleteLater();
}

void ApiClient::handleNotesResponse() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        Q_EMIT notesReceived(doc.array());
    } else {
        Q_EMIT error(reply->errorString());
    }
    reply->deleteLater();
}

void ApiClient::handleExportResponse() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QTemporaryFile file(QDir::tempPath() + QStringLiteral("/vibenote_exportXXXXXX"));
        if (file.open()) {
            file.write(reply->readAll());
            file.close();
            Q_EMIT exportCompleted(file.fileName());
        } else {
            Q_EMIT error(QStringLiteral("Failed to create export file"));
        }
    } else {
        Q_EMIT error(reply->errorString());
    }
    reply->deleteLater();
}

void ApiClient::handleWatchModeResponse() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        bool enabled = reply->url().path().endsWith(QStringLiteral("start"));
        Q_EMIT watchModeToggled(enabled);
    } else {
        Q_EMIT error(reply->errorString());
    }
    reply->deleteLater();
}

void ApiClient::handleUpdateConfigResponse() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        Q_EMIT configUpdated();
    } else {
        Q_EMIT error(reply->errorString());
    }
    reply->deleteLater();
}

void ApiClient::handleMetricsResponse() {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QString text = QString::fromUtf8(reply->readAll());
        QJsonObject metrics;
        
        // Parse Prometheus format
        const auto lines = text.split(QChar::fromLatin1('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const auto parts = line.split(QLatin1Char(' '));
            if (parts.size() == 2) {
                bool ok = false;
                double value = parts[1].toDouble(&ok);
                if (ok) {
                    metrics[parts[0]] = value;
                }
            }
        }
        
        Q_EMIT metricsReceived(metrics);
    } else {
        Q_EMIT error(reply->errorString());
    }
    reply->deleteLater();
}

void ApiClient::handleNetworkError(QNetworkReply::NetworkError) {
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (reply) {
        Q_EMIT error(reply->errorString());
    }
}
