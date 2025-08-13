#include <QAction>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QQuickWindow>
#include <QString>

#include <KGlobalAccel>

#include "api_client.h"

Q_LOGGING_CATEGORY(overlayLog, "vibenote.overlay")

class OverlayController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString response READ response NOTIFY responseChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)

public:
    explicit OverlayController(ApiClient *api, QObject *parent = nullptr);

    QAction *toggleAction() const;

    bool isVisible() const;
    void setVisible(bool visible);

    QString response() const;
    bool isLoading() const;

    Q_INVOKABLE void submitQuery(const QString &text);
    Q_INVOKABLE void hideOverlay();

signals:
    void visibleChanged();
    void responseChanged();
    void loadingChanged();
    void showOverlay();

public slots:
    void toggleOverlay();
    void onSummarizeResponse(const QString &response);

private:
    QAction *toggle_action;
    ApiClient *api_client;
    mutable QMutex mutex;
    bool overlay_visible;
    QString current_response;
    bool is_loading;
};

OverlayController::OverlayController(ApiClient *api, QObject *parent)
    : QObject(parent),
      toggle_action(new QAction(tr("Toggle VibeNote"), this)),
      api_client(api),
      overlay_visible(false),
      is_loading(false) {
    connect(toggle_action, &QAction::triggered, this, &OverlayController::toggleOverlay);

    // Register default global shortcut Ctrl+Alt+Space
    KGlobalAccel::setGlobalShortcut(
        toggle_action, {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Space)});

    qCInfo(overlayLog) << "Overlay controller initialised";
}

QAction *OverlayController::toggleAction() const { return toggle_action; }

bool OverlayController::isVisible() const {
    QMutexLocker locker(&mutex);
    return overlay_visible;
}

void OverlayController::setVisible(bool visible) {
    {
        QMutexLocker locker(&mutex);
        if (overlay_visible == visible) {
            return;
        }
        overlay_visible = visible;
    }
    emit visibleChanged();
    if (visible) {
        emit showOverlay();
    }
}

QString OverlayController::response() const {
    QMutexLocker locker(&mutex);
    return current_response;
}

bool OverlayController::isLoading() const {
    QMutexLocker locker(&mutex);
    return is_loading;
}

void OverlayController::toggleOverlay() {
    setVisible(!isVisible());
    qCInfo(overlayLog) << "Overlay visibility toggled" << isVisible();
}

void OverlayController::submitQuery(const QString &text) {
    {
        QMutexLocker locker(&mutex);
        is_loading = true;
    }
    emit loadingChanged();

    qCInfo(overlayLog) << "Submitting query";

    connect(api_client, &ApiClient::summarizeFinished, this,
            &OverlayController::onSummarizeResponse, Qt::SingleShotConnection);
    api_client->summarize(text);
}

void OverlayController::onSummarizeResponse(const QString &response) {
    {
        QMutexLocker locker(&mutex);
        current_response = response;
        is_loading = false;
    }
    emit responseChanged();
    emit loadingChanged();

    qCInfo(overlayLog) << "Summarize response received";
}

void OverlayController::hideOverlay() {
    setVisible(false);
    qCInfo(overlayLog) << "Overlay hidden";
}

#include "overlay_controller.moc"

