#include "screencast_portal.h"
#include "../logging.h"

namespace vibenote {

ScreencastPortal::ScreencastPortal(QObject *parent)
    : QObject(parent) {}

ScreencastPortal::~ScreencastPortal() {
    stop();
}

bool ScreencastPortal::start() {
    LOG_INFO("Starting screencast portal");
    active_ = true;
    return true;
}

void ScreencastPortal::stop() {
    if (active_) {
        LOG_INFO("Stopping screencast portal");
        active_ = false;
    }
}

bool ScreencastPortal::isActive() const {
    return active_;
}

} // namespace vibenote
