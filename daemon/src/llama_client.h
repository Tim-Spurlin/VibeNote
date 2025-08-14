#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QProcess>
#include <memory>
#include <functional>

class LlamaClient : public QObject {
    Q_OBJECT

public:
    explicit LlamaClient(QObject *parent = nullptr);
    ~LlamaClient();
    
    bool connectToServer(const QString &host, int port);
    bool spawnServer(const QString &model_path, int ngl, const QStringList &other_params);
    QString streamCompletion(const QString &prompt, const QJsonObject &params,
                           std::function<void(const QString&)> on_token);
    void stopGeneration(const QString &request_id);
    bool restartWithNgl(int new_ngl);

signals:
    void connected();
    void disconnected();
    void error(const QString &message);
    void completionChunk(const QString &text);
    void completionFinished();

private slots:
    void onReadyRead();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void attemptReconnect();

private:
    QString buildRequest(const QString &endpoint, const QJsonObject &data);
    QString generateRequestId() const;

    QTcpSocket *socket_;
    std::unique_ptr<QProcess> server_process_;
    QString host_;
    quint16 port_;
    QString model_path_;
    QStringList extra_params_;
    QByteArray response_buffer_;
    std::function<void(const QString&)> current_handler_;
};
