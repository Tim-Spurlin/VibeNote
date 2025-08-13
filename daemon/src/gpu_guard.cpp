#include <nvml.h>
#include <QTimer>
#include <QObject>
#include <atomic>

#include "logging.h"

class GpuGuard : public QObject {
    Q_OBJECT
public:
    struct Stats {
        float utilization;           // current GPU utilization percentage
        size_t vramFreeMb;           // free VRAM in megabytes
        size_t vramTotalMb;          // total VRAM in megabytes
        bool throttled;              // whether throttling is active
    };

    GpuGuard(nvmlDevice_t dev, float util_threshold, size_t vram_headroom_mb, QObject *parent = nullptr);

    bool initialize();

    bool canAcceptWork() const;
    int calculateOptimalNgl(size_t model_size_mb) const;
    void requestModelRestart(int new_ngl);
    Stats getStats() const;

public slots:
    void pollGpu();

signals:
    void utilizationChanged(float percent);
    void throttleRequested(bool enable);
    void modelRestartRequested(int newNgl);

private:
    nvmlDevice_t m_device{};
    std::atomic<float> m_utilization{0.0f};
    std::atomic<size_t> m_vram_free{0};
    size_t m_vram_total{0};
    float m_util_threshold{0.0f};
    size_t m_vram_headroom{0};
    bool m_throttled{false};
    bool m_nvml_available{false};
    QTimer m_timer;
};

static constexpr size_t kMB = 1024 * 1024;

GpuGuard::GpuGuard(nvmlDevice_t dev, float util_threshold, size_t vram_headroom_mb, QObject *parent)
    : QObject(parent), m_device(dev), m_util_threshold(util_threshold), m_vram_headroom(vram_headroom_mb) {}

bool GpuGuard::initialize() {
    if (!m_device) {
        LOG_WARN("NVML device handle is null; GPU monitoring disabled");
        m_nvml_available = false;
        m_throttled = true;
        emit throttleRequested(true);
        return false;
    }

    m_nvml_available = true;
    QObject::connect(&m_timer, &QTimer::timeout, this, &GpuGuard::pollGpu);
    m_timer.start(200); // 5Hz
    return true;
}

void GpuGuard::pollGpu() {
    if (!m_nvml_available)
        return;

    nvmlUtilization_t util{};
    nvmlReturn_t err = nvmlDeviceGetUtilizationRates(m_device, &util);
    if (err != NVML_SUCCESS) {
        LOG_ERROR("NVML utilization query failed: %s", nvmlErrorString(err));
        m_nvml_available = false;
        m_throttled = true;
        emit throttleRequested(true);
        m_timer.stop();
        return;
    }

    nvmlMemory_t mem{};
    err = nvmlDeviceGetMemoryInfo(m_device, &mem);
    if (err != NVML_SUCCESS) {
        LOG_ERROR("NVML memory query failed: %s", nvmlErrorString(err));
        m_nvml_available = false;
        m_throttled = true;
        emit throttleRequested(true);
        m_timer.stop();
        return;
    }

    float gpu_util = static_cast<float>(util.gpu);
    m_utilization.store(gpu_util, std::memory_order_relaxed);

    size_t free_mb = mem.free / kMB;
    size_t total_mb = mem.total / kMB;
    m_vram_free.store(free_mb, std::memory_order_relaxed);
    m_vram_total = total_mb;

    emit utilizationChanged(gpu_util);

    bool shouldThrottle = gpu_util > m_util_threshold || free_mb <= m_vram_headroom;
    if (shouldThrottle && !m_throttled) {
        m_throttled = true;
        emit throttleRequested(true);
    } else if (m_throttled && gpu_util < (m_util_threshold - 10.0f) && free_mb > m_vram_headroom) {
        m_throttled = false;
        emit throttleRequested(false);
    }
}

bool GpuGuard::canAcceptWork() const {
    if (!m_nvml_available)
        return false;
    return m_utilization.load(std::memory_order_relaxed) < m_util_threshold &&
           m_vram_free.load(std::memory_order_relaxed) > m_vram_headroom;
}

int GpuGuard::calculateOptimalNgl(size_t model_size_mb) const {
    if (!m_nvml_available || model_size_mb == 0)
        return 0;
    size_t free_mb = m_vram_free.load(std::memory_order_relaxed);
    if (free_mb <= m_vram_headroom)
        return 0;
    size_t usable = free_mb - m_vram_headroom;
    const int total_layers = 32; // assume 32 layers for model
    size_t per_layer = model_size_mb / total_layers;
    if (per_layer == 0)
        per_layer = 1;
    int layers = static_cast<int>(usable / per_layer);
    if (layers > total_layers)
        layers = total_layers;
    if (layers < 0)
        layers = 0;
    return layers;
}

void GpuGuard::requestModelRestart(int new_ngl) { emit modelRestartRequested(new_ngl); }

GpuGuard::Stats GpuGuard::getStats() const {
    return Stats{m_utilization.load(std::memory_order_relaxed),
                 m_vram_free.load(std::memory_order_relaxed), m_vram_total, m_throttled};
}

#include "gpu_guard.moc"

