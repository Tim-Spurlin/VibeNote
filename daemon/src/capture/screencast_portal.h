#pragma once

#include <QObject>

namespace vibenote {

class ScreencastPortal : public QObject {
    Q_OBJECT

public:
    explicit ScreencastPortal(QObject *parent = nullptr);
    ~ScreencastPortal();
    
    bool start();
    void stop();
    bool isActive() const;

signals:
    void frameAvailable(const QByteArray &data);
    void error(const QString &message);

private:
    bool active_ = false;
};

} // namespace vibenote
