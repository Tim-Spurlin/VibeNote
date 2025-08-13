#include <QCommandLineParser>
#include <QCoreApplication>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <csignal>

#include <nvml.h>

#include "config.h"
#include "logging.h"
#include "gpu_guard.h"
#include "queue.h"
#include "http_server.h"

#include "capture/screencast_portal.h"
#include "windows/kwin_watcher.h"
#include "ocr/ocr_engine.h"
#include "store/sqlite_store.h"
#include "llama_client.h"

namespace {

void installSignalHandlers() {
    auto handler = [](int) { QCoreApplication::quit(); };
    std::signal(SIGINT, handler);
    std::signal(SIGTERM, handler);
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("VibeNote daemon");
    parser.addHelpOption();

    QCommandLineOption configOpt("config", "Path to configuration file", "path");
    QCommandLineOption portOpt("port", "HTTP server port", "port");
    QCommandLineOption spawnOpt("spawn-server", "Spawn llama.cpp server process");
    QCommandLineOption verboseOpt("verbose", "Enable verbose logging");
    parser.addOption(configOpt);
    parser.addOption(portOpt);
    parser.addOption(spawnOpt);
    parser.addOption(verboseOpt);
    parser.process(app);

    Logging::Options logOpts;
    logOpts.json = true;
    logOpts.verbose = parser.isSet(verboseOpt);
    Logging::init(logOpts);

    nvmlReturn_t nvmlRes = nvmlInit_v2();
    if (nvmlRes != NVML_SUCCESS) {
        qCritical() << "nvmlInit_v2 failed:" << nvmlErrorString(nvmlRes);
        return 1;
    }
    nvmlDevice_t device;
    nvmlRes = nvmlDeviceGetHandleByIndex_v2(0, &device);
    if (nvmlRes != NVML_SUCCESS) {
        qCritical() << "nvmlDeviceGetHandleByIndex_v2 failed:" << nvmlErrorString(nvmlRes);
        nvmlShutdown();
        return 1;
    }

    Config config;
    if (parser.isSet(configOpt)) {
        auto loaded = Config::fromFile(parser.value(configOpt));
        if (!loaded) {
            qCritical() << "Failed to load config";
            nvmlShutdown();
            return 1;
        }
        config = *loaded;
    } else {
        config = Config::defaults();
    }
    if (parser.isSet(portOpt)) {
        config.setPort(parser.value(portOpt).toUInt());
    }

    SqliteStore store(config.databasePath());
    if (!store.open()) {
        qCritical() << "Failed to open database";
        nvmlShutdown();
        return 1;
    }
    if (!store.migrate()) {
        qCritical() << "Failed to migrate database";
        nvmlShutdown();
        return 1;
    }

    GpuGuard gpuGuard(device, config.gpuLimits());
    TaskQueue queue(config.queueLimits());
    QObject::connect(&gpuGuard, &GpuGuard::throttle, &queue, &TaskQueue::pause);
    QObject::connect(&gpuGuard, &GpuGuard::resume, &queue, &TaskQueue::resume);

    QProcess llamaProcess;
    if (parser.isSet(spawnOpt)) {
        QStringList args;
        args << "--model" << config.modelPath()
             << "-ngl" << QString::number(gpuGuard.recommendedLayers());
        llamaProcess.start(config.llamaServerBinary(), args);
    }

    std::unique_ptr<LlamaClient> llamaClient;
    for (int attempt = 0; attempt < 5 && !llamaClient; ++attempt) {
        llamaClient = LlamaClient::connect(config.llamaEndpoint());
        if (!llamaClient) {
            QThread::sleep(1 << attempt);
        }
    }
    if (!llamaClient) {
        qCritical() << "Unable to connect to llama server";
        nvmlShutdown();
        return 1;
    }

    std::unique_ptr<OcrEngine> ocr = OcrEngine::create(config.ocrConfig());

    ScreencastPortal portal;
    QObject::connect(&portal, &ScreencastPortal::frameCaptured, ocr.get(), &OcrEngine::processFrame);
    portal.start();

    KWinWatcher watcher;
    QObject::connect(&watcher, &KWinWatcher::windowChanged, &store, &SqliteStore::updateWindow);
    watcher.start();

    HttpServer server(config.port(), &queue, &store, llamaClient.get());
    if (!server.start()) {
        qCritical() << "Failed to start HTTP server";
        nvmlShutdown();
        return 1;
    }

    installSignalHandlers();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        server.stop();
        portal.stop();
        watcher.stop();
        queue.stop();
        llamaClient.reset();
        if (llamaProcess.state() == QProcess::Running) {
            llamaProcess.terminate();
            llamaProcess.waitForFinished(3000);
        }
        nvmlShutdown();
    });

    return app.exec();
}
