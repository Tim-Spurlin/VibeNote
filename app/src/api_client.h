#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>

class ApiClient : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);
    explicit ApiClient(const QString &url, QObject *parent = nullptr);
    
    void getStatus();
    void summarize(const QString &prompt, const QJsonObject &params = {});
    void fetchNotes(const QDateTime &from, const QDateTime &to);
    void exportData(const QString &format, const QDateTime &from, const QDateTime &to);
    void toggleWatchMode(bool enable);
    void updateConfig(const QJsonObject &config);
    void fetchMetrics();

Q_SIGNALS:
    void statusReceived(const QJsonObject &status);
    void summarizeComplete(const QString &result);
    void notesReceived(const QJsonArray &notes);
    void exportCompleted(const QString &path);
    void watchModeToggled(bool enabled);
    void configUpdated();
    void metricsReceived(const QJsonObject &metrics);
    void error(const QString &message);

private Q_SLOTS:
    void handleStatusResponse();
    void handleSummarizeFinished();
    void handleNotesResponse();
    void handleExportResponse();
    void handleWatchModeResponse();
    void handleUpdateConfigResponse();
    void handleMetricsResponse();
    void handleNetworkError(QNetworkReply::NetworkError error);

private:
    QNetworkReply *makeGet(const QString &path);
    QNetworkReply *makePost(const QString &path, const QJsonDocument &doc = QJsonDocument());
    
    QNetworkAccessManager manager_;
    QString base_url_;
};

#endif // API_CLIENT_H
