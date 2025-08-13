#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QUuid>
#include <functional>

#include "logging.h"

class LlamaClient : public QObject {
    Q_OBJECT
public:
    explicit LlamaClient(QObject *parent = nullptr);
    ~LlamaClient() override;

    void connectToServer(const QString &host, quint16 port);
    bool spawnServer(const QString &model_path, int ngl, const QStringList &other_params);
    QString streamCompletion(const QString &prompt, const QJsonObject &params,
                             std::function<void(const QString &)> callback);
    void stopGeneration(const QString &request_id);
    bool restartWithNgl(int new_ngl);

signals:
    void connected();
    void disconnected();
    void error(const QString &msg);

private slots:
    void onReadyRead();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError err);

private:
    void attemptReconnect();
    QString buildRequest(const QString &path, const QJsonObject &payload) const;
    QString generateRequestId() const;

    QTcpSocket *socket_;
    QProcess *server_process_;
    QString host_;
    quint16 port_;
    int ngl_;
    QString model_path_;
    QStringList extra_params_;

    QMutex mutex_;
    QHash<QString, std::function<void(const QString &)>> callbacks_;
    QByteArray buffer_;
    QString last_event_id_;
    int reconnect_attempts_;
};

LlamaClient::LlamaClient(QObject *parent)
    : QObject(parent),
      socket_(new QTcpSocket(this)),
      server_process_(nullptr),
      port_(0),
      ngl_(0),
      reconnect_attempts_(0) {
    connect(socket_, &QTcpSocket::connected, this, [this]() {
        reconnect_attempts_ = 0;
        emit connected();
    });
    connect(socket_, &QTcpSocket::readyRead, this, &LlamaClient::onReadyRead);
    connect(socket_, &QTcpSocket::disconnected, this, &LlamaClient::onDisconnected);
    connect(socket_, &QTcpSocket::errorOccurred, this, &LlamaClient::onErrorOccurred);
}

LlamaClient::~LlamaClient() {
    if (server_process_) {
        server_process_->terminate();
        server_process_->waitForFinished(3000);
    }
}

void LlamaClient::connectToServer(const QString &host, quint16 port) {
    host_ = host;
    port_ = port;
    attemptReconnect();
}

void LlamaClient::attemptReconnect() {
    if (socket_->state() == QAbstractSocket::ConnectedState)
        return;
    int delay = qMin(30000, (1 << reconnect_attempts_) * 1000);
    QTimer::singleShot(delay, this, [this]() {
        LOG_INFO("Connecting to llama.cpp server %s:%d", qPrintable(host_), port_);
        socket_->connectToHost(host_, port_);
    });
    reconnect_attempts_++;
}

bool LlamaClient::spawnServer(const QString &model_path, int ngl, const QStringList &other_params) {
    model_path_ = model_path;
    ngl_ = ngl;
    extra_params_ = other_params;

    if (server_process_) {
        server_process_->terminate();
        server_process_->waitForFinished(5000);
        server_process_->deleteLater();
    }

    server_process_ = new QProcess(this);
    QString program = QStringLiteral("third_party/llama.cpp/server");
    QStringList args;
    args << "--model" << model_path_ << "--host" << host_ << "--port" << QString::number(port_)
         << "--ngl" << QString::number(ngl_);
    args << extra_params_;

    server_process_->start(program, args);
    if (!server_process_->waitForStarted()) {
        emit error(tr("Failed to start llama server: %1").arg(server_process_->errorString()));
        return false;
    }

    for (int i = 0; i < 30; ++i) {
        socket_->connectToHost(host_, port_);
        if (socket_->waitForConnected(1000)) {
            return true;
        }
        QThread::sleep(1);
    }
    emit error(tr("Server did not become ready"));
    return false;
}

QString LlamaClient::buildRequest(const QString &path, const QJsonObject &payload) const {
    QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QString request = QStringLiteral("POST %1 HTTP/1.1\r\n")
                          .arg(path) +
                      QStringLiteral("Host: %1\r\n").arg(host_) +
                      QStringLiteral("Content-Type: application/json\r\n") +
                      QStringLiteral("Connection: keep-alive\r\n") +
                      QStringLiteral("Content-Length: %1\r\n\r\n")
                          .arg(body.size()) +
                      QString::fromUtf8(body);
    return request;
}

QString LlamaClient::generateRequestId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString LlamaClient::streamCompletion(const QString &prompt, const QJsonObject &params,
                                      std::function<void(const QString &)> callback) {
    QString id = generateRequestId();
    QJsonObject payload = params;
    payload.insert("id", id);
    payload.insert("prompt", prompt);
    payload.insert("stream", true);

    QString request = buildRequest(QStringLiteral("/v1/completions"), payload);
    {
        QMutexLocker locker(&mutex_);
        callbacks_.insert(id, std::move(callback));
    }
    socket_->write(request.toUtf8());
    return id;
}

void LlamaClient::stopGeneration(const QString &request_id) {
    QJsonObject payload{{"id", request_id}};
    QString request = buildRequest(QStringLiteral("/v1/stop"), payload);
    socket_->write(request.toUtf8());
}

bool LlamaClient::restartWithNgl(int new_ngl) {
    if (server_process_) {
        server_process_->terminate();
        if (!server_process_->waitForFinished(5000)) {
            server_process_->kill();
            server_process_->waitForFinished();
        }
    }
    ngl_ = new_ngl;
    return spawnServer(model_path_, ngl_, extra_params_);
}

void LlamaClient::onDisconnected() {
    emit disconnected();
    attemptReconnect();
}

void LlamaClient::onErrorOccurred(QAbstractSocket::SocketError) {
    emit error(socket_->errorString());
    attemptReconnect();
}

void LlamaClient::onReadyRead() {
    buffer_.append(socket_->readAll());
    while (true) {
        int idx = buffer_.indexOf("\n\n");
        if (idx == -1)
            break;
        QByteArray event = buffer_.left(idx);
        buffer_.remove(0, idx + 2);
        if (!event.startsWith("data: "))
            continue;
        QByteArray data = event.mid(6).trimmed();
        if (data == "[DONE]") {
            QMutexLocker locker(&mutex_);
            if (!last_event_id_.isEmpty()) {
                callbacks_.remove(last_event_id_);
                last_event_id_.clear();
            }
            continue;
        }
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject())
            continue;
        QJsonObject obj = doc.object();
        QString id = obj.value("id").toString();
        QJsonArray choices = obj.value("choices").toArray();
        QString token;
        if (!choices.isEmpty()) {
            QJsonObject choice = choices.first().toObject();
            QJsonObject delta = choice.value("delta").toObject();
            token = delta.value("content").toString();
            if (token.isEmpty()) {
                token = choice.value("text").toString();
            }
        }
        std::function<void(const QString &)> cb;
        {
            QMutexLocker locker(&mutex_);
            cb = callbacks_.value(id);
            last_event_id_ = id;
        }
        if (cb && !token.isEmpty()) {
            cb(token);
        }
    }
}

#include "moc_llama_client.cpp"
