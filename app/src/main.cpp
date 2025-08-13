#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>

#include <KAboutData>
#include <KLocalizedString>
#include <KGlobalAccel>

#include <QKeySequence>
#include <QUrl>
#include <QObject>

#include "overlay_controller.h"
#include "api_client.h"
#include "settings_store.h"
#include "metrics_view.h"

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QGuiApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("vibenote");

    KAboutData aboutData(
        QStringLiteral("vibenote"),
        i18n("VibeNote"),
        QStringLiteral("0.1"),
        i18n("Vibe note taking application"),
        KAboutLicense::GPL_V3,
        i18n("© 2024 Saphyre"));
    aboutData.addAuthor(i18n("Tim Spurlin"), i18n("Maintainer"),
                        QStringLiteral("Tim.Spurlin@SaphyreSolutions.com"));
    KAboutData::setApplicationData(aboutData);

    SettingsStore *settings = new SettingsStore();
    ApiClient *api = new ApiClient(settings->daemonUrl());
    OverlayController *overlay = new OverlayController(api);
    MetricsView *metrics = new MetricsView(api);

    qmlRegisterSingletonInstance("org.saphyre.vibenote", 1, 0, "Settings", settings);
    qmlRegisterSingletonInstance("org.saphyre.vibenote", 1, 0, "Api", api);
    qmlRegisterSingletonInstance("org.saphyre.vibenote", 1, 0, "Overlay", overlay);
    qmlRegisterSingletonInstance("org.saphyre.vibenote", 1, 0, "Metrics", metrics);

    const QKeySequence toggleSequence(Qt::CTRL | Qt::ALT | Qt::Key_Space);
    KGlobalAccel::self()->setDefaultShortcut(overlay->toggleAction(), {toggleSequence});
    KGlobalAccel::self()->setShortcut(overlay->toggleAction(), {toggleSequence});

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
                     [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && objUrl == url)
                             QCoreApplication::exit(-1);
                     },
                     Qt::QueuedConnection);
    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    QObject::connect(&app, &QCoreApplication::aboutToQuit, settings,
                     &QObject::deleteLater);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, api,
                     &QObject::deleteLater);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, overlay,
                     &QObject::deleteLater);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, metrics,
                     &QObject::deleteLater);

    return app.exec();
}

