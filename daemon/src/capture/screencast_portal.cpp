#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QImage>
#include <QThread>
#include <QDebug>

#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>

#include <sys/mman.h>
#include <unistd.h>

class ScreencastPortal : public QObject {
    Q_OBJECT

public:
    explicit ScreencastPortal(QObject *parent = nullptr);
    ~ScreencastPortal() override;

    void requestCapture();
    void startCapture();
    void stopCapture();

signals:
    void frameReady(const QImage &frame);
    void errorOccurred(const QString &message);

private slots:
    void onSessionCreated(uint response, const QVariantMap &results);
    void onSourcesSelected(uint response, const QVariantMap &results);
    void onStart(uint response, const QVariantMap &results);

private:
    void initPipeWire(uint32_t nodeId);
    static void onParamChanged(void *data, uint32_t id, const struct spa_pod *param);
    static void onProcess(void *data);

    pw_main_loop *m_loop{nullptr};
    pw_stream *m_stream{nullptr};
    QDBusInterface *m_portal{nullptr};
    QString m_sessionHandle;
    QString m_streamHandle;
    spa_video_info_raw m_videoInfo{};
    std::thread m_loopThread;
    bool m_active{false};
};

ScreencastPortal::ScreencastPortal(QObject *parent)
    : QObject(parent) {}

ScreencastPortal::~ScreencastPortal() {
    stopCapture();
}

void ScreencastPortal::requestCapture() {
    if (m_active) {
        return;
    }

    auto &bus = QDBusConnection::sessionBus();
    m_portal = new QDBusInterface(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        bus,
        this);

    if (!m_portal->isValid()) {
        emit errorOccurred(QStringLiteral("xdg-desktop-portal not available"));
        delete m_portal;
        m_portal = nullptr;
        return;
    }

    // Listen for the SessionCreated signal
    bus.connect("org.freedesktop.portal.Desktop",
                "/org/freedesktop/portal/desktop",
                "org.freedesktop.portal.ScreenCast",
                "SessionCreated",
                this,
                SLOT(onSessionCreated(uint,QVariantMap)));

    m_portal->call(QDBus::NoBlock, "CreateSession", QVariantMap{});
}

void ScreencastPortal::onSessionCreated(uint response, const QVariantMap &results) {
    if (response != 0) {
        emit errorOccurred(QStringLiteral("Session creation denied"));
        return;
    }

    m_sessionHandle = results.value(QStringLiteral("session_handle")).toString();
    if (m_sessionHandle.isEmpty()) {
        emit errorOccurred(QStringLiteral("Missing session handle"));
        return;
    }

    auto &bus = QDBusConnection::sessionBus();
    bus.connect("org.freedesktop.portal.Desktop",
                m_sessionHandle,
                "org.freedesktop.portal.Request",
                "Response",
                this,
                SLOT(onSourcesSelected(uint,QVariantMap)));

    QVariantMap opts;
    opts.insert(QStringLiteral("types"), 1u); // screen
    m_portal->callWithArgumentList(QDBus::NoBlock, "SelectSources",
                                   {QVariant::fromValue(m_sessionHandle), opts});
}

void ScreencastPortal::onSourcesSelected(uint response, const QVariantMap &results) {
    if (response != 0) {
        emit errorOccurred(QStringLiteral("Source selection denied"));
        return;
    }

    auto &bus = QDBusConnection::sessionBus();
    bus.connect("org.freedesktop.portal.Desktop",
                m_sessionHandle,
                "org.freedesktop.portal.Request",
                "Response",
                this,
                SLOT(onStart(uint,QVariantMap)));

    QVariantMap opts;
    m_portal->callWithArgumentList(QDBus::NoBlock, "Start",
                                   {QVariant::fromValue(m_sessionHandle), QString(), opts});
}

void ScreencastPortal::onStart(uint response, const QVariantMap &results) {
    if (response != 0) {
        emit errorOccurred(QStringLiteral("Start denied"));
        return;
    }

    QVariant streamsVar = results.value(QStringLiteral("streams"));
    if (!streamsVar.isValid() || streamsVar.toList().isEmpty()) {
        emit errorOccurred(QStringLiteral("No streams returned"));
        return;
    }

    QVariantMap streamMap = streamsVar.toList().first().toMap();
    uint nodeId = streamMap.value(QStringLiteral("node_id")).toUInt();
    if (nodeId == 0) {
        emit errorOccurred(QStringLiteral("Invalid PipeWire node"));
        return;
    }

    initPipeWire(nodeId);
    startCapture();
}

void ScreencastPortal::initPipeWire(uint32_t nodeId) {
    pw_init(nullptr, nullptr);

    m_loop = pw_main_loop_new(nullptr);
    if (!m_loop) {
        emit errorOccurred(QStringLiteral("Failed to create PipeWire main loop"));
        return;
    }

    auto *context = pw_context_new(pw_main_loop_get_loop(m_loop), nullptr, 0);
    if (!context) {
        emit errorOccurred(QStringLiteral("Failed to create PipeWire context"));
        return;
    }

    auto *core = pw_context_connect(context, nullptr, 0);
    if (!core) {
        emit errorOccurred(QStringLiteral("Failed to connect to PipeWire core"));
        return;
    }

    pw_properties *props = pw_properties_new(PW_KEY_TARGET_OBJECT,
                                             QString::number(nodeId).toUtf8().constData(),
                                             nullptr);

    m_stream = pw_stream_new_simple(pw_main_loop_get_loop(m_loop),
                                    "screencast",
                                    props,
                                    &(pw_stream_events){
                                        PW_VERSION_STREAM_EVENTS,
                                        .param_changed = &ScreencastPortal::onParamChanged,
                                        .process = &ScreencastPortal::onProcess},
                                    this);

    if (!m_stream) {
        emit errorOccurred(QStringLiteral("Failed to create PipeWire stream"));
        return;
    }

    pw_stream_connect(m_stream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                   PW_STREAM_FLAG_MAP_BUFFERS),
                      nullptr,
                      0);
}

void ScreencastPortal::startCapture() {
    if (!m_loop || m_active) {
        return;
    }
    m_active = true;
    m_loopThread = std::thread([this]() { pw_main_loop_run(m_loop); });
}

void ScreencastPortal::stopCapture() {
    m_active = false;

    if (m_loop) {
        pw_main_loop_quit(m_loop);
    }
    if (m_loopThread.joinable()) {
        m_loopThread.join();
    }
    if (m_stream) {
        pw_stream_destroy(m_stream);
        m_stream = nullptr;
    }
    if (m_loop) {
        pw_main_loop_destroy(m_loop);
        m_loop = nullptr;
    }
    if (m_portal) {
        delete m_portal;
        m_portal = nullptr;
    }
    pw_deinit();
}

void ScreencastPortal::onParamChanged(void *data, uint32_t id, const struct spa_pod *param) {
    auto *self = static_cast<ScreencastPortal *>(data);
    if (id != SPA_PARAM_Format || param == nullptr) {
        return;
    }

    spa_format_video_raw_parse(param, &self->m_videoInfo);
}

void ScreencastPortal::onProcess(void *data) {
    auto *self = static_cast<ScreencastPortal *>(data);
    if (!self->m_stream) {
        return;
    }

    struct pw_buffer *buffer = pw_stream_dequeue_buffer(self->m_stream);
    if (!buffer) {
        return;
    }

    spa_buffer *spaBuf = buffer->buffer;
    if (!spaBuf || spaBuf->datas[0].chunk->size == 0) {
        pw_stream_queue_buffer(self->m_stream, buffer);
        return;
    }

    spa_data *d = &spaBuf->datas[0];
    void *map = nullptr;
    if (d->type == SPA_DATA_MemPtr) {
        map = d->data;
    } else if (d->type == SPA_DATA_DmaBuf && d->fd != -1) {
        map = mmap(nullptr, d->maxsize, PROT_READ, MAP_SHARED, d->fd, d->mapoffset);
    }

    if (map) {
        int stride = d->chunk->stride;
        QImage::Format fmt = QImage::Format_Invalid;
        switch (self->m_videoInfo.format) {
        case SPA_VIDEO_FORMAT_BGRA:
        case SPA_VIDEO_FORMAT_BGRx:
            fmt = QImage::Format_ARGB32;
            break;
        case SPA_VIDEO_FORMAT_RGBA:
        case SPA_VIDEO_FORMAT_RGBx:
            fmt = QImage::Format_RGBA8888;
            break;
        case SPA_VIDEO_FORMAT_RGB:
            fmt = QImage::Format_RGB888;
            break;
        default:
            qWarning() << "Unsupported pixel format" << self->m_videoInfo.format;
            break;
        }

        if (fmt != QImage::Format_Invalid) {
            QImage img(static_cast<uchar *>(map),
                       self->m_videoInfo.size.width,
                       self->m_videoInfo.size.height,
                       stride,
                       fmt);
            emit self->frameReady(img.copy());
        }

        if (d->type == SPA_DATA_DmaBuf) {
            munmap(map, d->maxsize);
        }
    }

    pw_stream_queue_buffer(self->m_stream, buffer);
}

#include "screencast_portal.moc"
