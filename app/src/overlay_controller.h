#ifndef OVERLAY_CONTROLLER_H
#define OVERLAY_CONTROLLER_H

#include <QObject>
#include <QString>

class ApiClient;
class QAction;

class OverlayController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString response READ response NOTIFY responseChanged)
    
public:
    explicit OverlayController(ApiClient *client, QObject *parent = nullptr);
    
    bool visible() const;
    void setVisible(bool visible);
    bool loading() const;
    QString response() const;
    
    Q_INVOKABLE void submitQuery(const QString &query);
    
Q_SIGNALS:
    void visibleChanged();
    void loadingChanged();
    void responseChanged();
    void showOverlay();
    void hideOverlay();
    
public Q_SLOTS:
    void toggleOverlay();
    
private Q_SLOTS:
    void onSummarizeResponse(const QString &response);
    
private:
    ApiClient *api_client;
    bool visible_ = false;
    bool loading_ = false;
    QString response_;
    QAction *toggle_action;
};

#endif // OVERLAY_CONTROLLER_H
