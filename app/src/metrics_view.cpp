#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

#include "api_client.h"

class MetricsView : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList noteRateHistory READ noteRateHistory NOTIFY metricsUpdated)
    Q_PROPERTY(double currentGpuUsage READ currentGpuUsage NOTIFY metricsUpdated)
    Q_PROPERTY(int queueDepth READ queueDepth NOTIFY metricsUpdated)
    Q_PROPERTY(QVariantMap appUsageStats READ appUsageStats NOTIFY metricsUpdated)

public:
    explicit MetricsView(ApiClient *api, QObject *parent = nullptr)
        : QObject(parent), api_client(api), refresh_timer(new QTimer(this)) {
        refresh_timer->setInterval(5000);
        connect(refresh_timer, &QTimer::timeout, this, &MetricsView::refreshMetrics);
        refresh_timer->start();
        connect(api_client, &ApiClient::metricsReceived, this, &MetricsView::onMetricsReceived);
    }

    QVariantList noteRateHistory() const {
        QVariantList list;
        for (double value : note_rate_history) {
            list << value;
        }
        return list;
    }

    double currentGpuUsage() const { return gpu_usage; }
    int queueDepth() const { return queue_depth; }

    QVariantMap appUsageStats() const {
        QVariantMap map;
        for (auto it = app_usage.cbegin(); it != app_usage.cend(); ++it) {
            map.insert(it.key(), it.value());
        }
        return map;
    }

    Q_INVOKABLE QString requestAiAnalysis() {
        QJsonObject obj;
        obj["gpu_usage"] = gpu_usage;
        obj["queue_depth"] = queue_depth;
        obj["note_rate_history"] = QJsonArray::fromVariantList(noteRateHistory());
        QJsonObject usage;
        for (auto it = app_usage.cbegin(); it != app_usage.cend(); ++it) {
            usage.insert(it.key(), it.value());
        }
        obj["app_usage"] = usage;
        QString prompt = QStringLiteral("Provide an analysis of the following metrics: %1")
                              .arg(QString::fromUtf8(
                                  QJsonDocument(obj).toJson(QJsonDocument::Compact)));
        return api_client->summarize(prompt);
    }

    Q_INVOKABLE void clearHistory() {
        note_rate_history.clear();
        gpu_usage = 0.0;
        queue_depth = 0;
        app_usage.clear();
        last_notes_total = 0.0;
        emit metricsUpdated();
    }

signals:
    void metricsUpdated();

private slots:
    void refreshMetrics() { api_client->fetchMetrics(); }

    void onMetricsReceived(const QString &prometheus_text) {
        double total_notes = 0.0;
        QMap<QString, int> new_app_usage;
        const auto lines = prometheus_text.split('\n');
        QRegularExpression rx(
            QLatin1String(R"(^([a-zA-Z_][a-zA-Z0-9_:]*)(\{[^}]*\})?\s+(\d+(?:\.\d+)?)$)"));
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#')) {
                continue;
            }
            QRegularExpressionMatch m = rx.match(trimmed);
            if (!m.hasMatch()) {
                continue;
            }
            const QString metric = m.captured(1);
            const QString labels = m.captured(2);
            const double value = m.captured(3).toDouble();

            if (metric == QLatin1String("vibenote_notes_total")) {
                total_notes += value;
                QRegularExpression app_rx(QLatin1String("app=\"([^\"]+)\""));
                QRegularExpressionMatch app_match = app_rx.match(labels);
                if (app_match.hasMatch()) {
                    new_app_usage.insert(app_match.captured(1), static_cast<int>(value));
                }
            } else if (metric == QLatin1String("vibenote_gpu_utilization_percent")) {
                gpu_usage = value;
            } else if (metric == QLatin1String("vibenote_queue_depth")) {
                queue_depth = static_cast<int>(value);
            }
        }
        app_usage = new_app_usage;

        const double rate = (total_notes - last_notes_total) / 5.0;
        last_notes_total = total_notes;
        note_rate_history.append(rate);
        if (note_rate_history.size() > 60) {
            note_rate_history.removeFirst();
        }
        emit metricsUpdated();
    }

private:
    ApiClient *api_client;
    QTimer *refresh_timer;
    QList<double> note_rate_history;
    double gpu_usage = 0.0;
    int queue_depth = 0;
    QMap<QString, int> app_usage;
    double last_notes_total = 0.0;
};

#include "metrics_view.moc"
