#include "metrics_view.h"
#include "api_client.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

MetricsView::MetricsView(ApiClient *client, QObject *parent)
    : QObject(parent), api_client(client) {
    
    // Set up periodic refresh
    auto *refresh_timer = new QTimer(this);
    refresh_timer->setInterval(30000); // 30 seconds
    refresh_timer->start();
    
    connect(refresh_timer, &QTimer::timeout, this, &MetricsView::refreshMetrics);
    connect(api_client, &ApiClient::metricsReceived, this, &MetricsView::onMetricsReceived);
    
    // Initial fetch
    refreshMetrics();
}

double MetricsView::gpuUsage() const {
    return gpu_usage;
}

int MetricsView::queueDepth() const {
    return queue_depth;
}

QVariantList MetricsView::noteRateHistory() const {
    QVariantList list;
    for (double rate : note_rate_history_) {
        list.append(rate);
    }
    return list;
}

QString MetricsView::requestAiAnalysis() {
    if (!api_client) {
        return QString();
    }
    
    // Build metrics data as JSON
    QJsonObject metricsData;
    metricsData[QStringLiteral("gpu_usage")] = gpu_usage;
    metricsData[QStringLiteral("queue_depth")] = queue_depth;
    
    QJsonDocument doc(metricsData);
    QString prompt = QStringLiteral("Analyze these metrics and provide insights: ");
    prompt += QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    
    api_client->summarize(prompt);
    return QStringLiteral("Analysis requested...");
}

void MetricsView::clearHistory() {
    note_rate_history_.clear();
    gpu_usage = 0.0;
    queue_depth = 0;
    Q_EMIT metricsUpdated();
}

void MetricsView::refreshMetrics() {
    if (api_client) {
        api_client->fetchMetrics();
    }
}

void MetricsView::onMetricsReceived(const QJsonObject &metrics) {
    // Update GPU usage
    if (metrics.contains(QStringLiteral("gpu_usage"))) {
        gpu_usage = metrics[QStringLiteral("gpu_usage")].toDouble();
    }
    
    // Update queue depth
    if (metrics.contains(QStringLiteral("queue_depth"))) {
        queue_depth = metrics[QStringLiteral("queue_depth")].toInt();
    }
    
    // Update note rate history
    if (metrics.contains(QStringLiteral("note_rate"))) {
        double rate = metrics[QStringLiteral("note_rate")].toDouble();
        note_rate_history_.append(rate);
        
        // Keep only last 100 data points
        while (note_rate_history_.size() > 100) {
            note_rate_history_.removeFirst();
        }
    }
    
    Q_EMIT metricsUpdated();
}
