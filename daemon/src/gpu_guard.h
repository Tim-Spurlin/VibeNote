#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

class GpuGuard : public QObject {
    Q_OBJECT

public:
    explicit GpuGuard(QObject *parent = nullptr);
    ~GpuGuard();
    
    bool initialize();
    bool canAcceptWork() const;
    void requestThrottle(bool throttle);
    void requestModelRestart(int new_ngl);

signals:
    void throttleRequested(bool throttle);
    void throttleStateChanged(bool throttled);
    void modelRestartRequested(int new_ngl);

public slots:
    void pollGpu();

private:
    QTimer m_timer;
    void *m_device = nullptr;
    bool m_throttled = false;
    int m_util_threshold = 85;
    int m_vram_headroom_mb = 800;
};
