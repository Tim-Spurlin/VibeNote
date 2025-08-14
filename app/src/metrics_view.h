#ifndef METRICS_VIEW_H
#define METRICS_VIEW_H

#include <QObject>
#include <QVariantList>
#include <QJsonObject>

class ApiClient;

class MetricsView : public QObject {
    Q_OBJECT
    Q_PROPERTY(double gpuUsage READ gpuUsage NOTIFY metricsUpdated)
    Q_PROPERTY(int queueDepth READ queueDepth NOTIFY metricsUpdated)
    Q_PROPERTY(QVariantList noteRateHistory READ noteRateHistory NOTIFY metricsUpdated)
    
public:
    explicit MetricsView(ApiClient *client, QObject *parent = nullptr);
    
    double gpuUsage() const;
    int queueDepth() const;
    QVariantList noteRateHistory() const;
    
    Q_INVOKABLE QString requestAiAnalysis();
    Q_INVOKABLE void clearHistory();
    
Q_SIGNALS:
    void metricsUpdated();
    
private Q_SLOTS:
    void refreshMetrics();
    void onMetricsReceived(const QJsonObject &metrics);
    
private:
    ApiClient *api_client;
    double gpu_usage = 0.0;
    int queue_depth = 0;
    QList<double> note_rate_history_;
};

#endif // METRICS_VIEW_H
