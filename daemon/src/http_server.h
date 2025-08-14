#pragma once

#include <QObject>
#include <QHttpServer>
#include <memory>

class LlamaClient;
class Metrics;
namespace vibenote {
    class TaskQueue;
    class SqliteStore;
}

class HttpServer : public QObject {
    Q_OBJECT
    
public:
    HttpServer(vibenote::TaskQueue *queue, LlamaClient *llama,
              vibenote::SqliteStore *store, Metrics *metrics,
              QObject *parent = nullptr);
    ~HttpServer();
    
    bool start(quint16 port);
    void stop();
    
private:
    QHttpServer server_;
    vibenote::TaskQueue *queue_;
    LlamaClient *llama_;
    vibenote::SqliteStore *store_;
    Metrics *metrics_;
};
