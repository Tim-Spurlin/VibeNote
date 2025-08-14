#include "overlay_controller.h"
#include "api_client.h"
#include <QAction>
#include <QKeySequence>

OverlayController::OverlayController(ApiClient *client, QObject *parent)
    : QObject(parent), api_client(client) {
    
    // Create global hotkey action
    toggle_action = new QAction(this);
    toggle_action->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Space));
    connect(toggle_action, &QAction::triggered, this, &OverlayController::toggleOverlay);
}

bool OverlayController::visible() const {
    return visible_;
}

void OverlayController::setVisible(bool visible) {
    if (visible_ != visible) {
        visible_ = visible;
        Q_EMIT visibleChanged();
        
        if (visible) {
            Q_EMIT showOverlay();
        } else {
            Q_EMIT hideOverlay();
        }
    }
}

bool OverlayController::loading() const {
    return loading_;
}

QString OverlayController::response() const {
    return response_;
}

void OverlayController::toggleOverlay() {
    setVisible(!visible_);
}

void OverlayController::submitQuery(const QString &query) {
    if (query.isEmpty() || loading_) {
        return;
    }
    
    loading_ = true;
    response_.clear();
    Q_EMIT loadingChanged();
    
    // Connect to completion signal
    connect(api_client, &ApiClient::summarizeComplete, this,
            &OverlayController::onSummarizeResponse, Qt::UniqueConnection);
    
    api_client->summarize(query);
}

void OverlayController::onSummarizeResponse(const QString &response) {
    response_ = response;
    loading_ = false;
    
    Q_EMIT responseChanged();
    Q_EMIT loadingChanged();
}
