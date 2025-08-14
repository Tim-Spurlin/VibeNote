#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <KLocalizedString>
#include <KLocalizedContext>

#ifdef HAVE_KABOUTDATA
#include <KAboutData>
#endif

#include "api_client.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set application metadata
    app.setOrganizationName(QStringLiteral("VibeNote"));
    app.setOrganizationDomain(QStringLiteral("vibenote.local"));
    app.setApplicationName(QStringLiteral("VibeNote"));
    app.setApplicationDisplayName(QStringLiteral("VibeNote"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("vibenote")));

#ifdef HAVE_KABOUTDATA
    KAboutData aboutData(
        QStringLiteral("vibenote"),
        i18n("VibeNote"),
        QStringLiteral("1.0.0"),
        i18n("Local-first AI note-taking"),
        KAboutLicense::GPL_V3,
        i18n("© 2025 VibeNote Contributors")
    );
    KAboutData::setApplicationData(aboutData);
#endif

    // Create API client
    ApiClient apiClient;
    
    // Setup QML engine
    QQmlApplicationEngine engine;
    
    // Add localization context
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    
    // Register API client
    engine.rootContext()->setContextProperty(QStringLiteral("apiClient"), &apiClient);
    
    // Load QML
    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    return app.exec();
}
