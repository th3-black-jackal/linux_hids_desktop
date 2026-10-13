#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QUrl>

#include "AppController.h"
#include "ThemeProvider.h"
#include "services/ThreatIntelController.h"
#ifdef IRONWATCH_HAS_WEBENGINE
#include <QtWebEngineQuick/QtWebEngineQuick>
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Ironwatch"));
    app.setApplicationName(QStringLiteral("Ironwatch Edge HIDS Console"));

    // Declared before the engine so they are destroyed *after* it. Locals go
    // in reverse order, and the QML object tree re-evaluates bindings as it
    // tears down — with the engine outliving these, every binding on App or
    // Theme resolves against a deleted object on the way out.
    AppController controller;
    ThemeProvider themeProvider(&controller);
#ifdef IRONWATCH_HAS_WEBENGINE
    QtWebEngineQuick::initialize();   // must precede the QQmlApplicationEngine
#endif
    QQmlApplicationEngine engine;

    // Registered as singletons (not rootContext properties) so they're
    // usable from anywhere, including other singletons. A dedicated URI
    // (not "Ironwatch") keeps these separate from qt_add_qml_module's own
    // static registration for that module/version.
    //
    // Theme was originally a composite `pragma Singleton` QtObject in
    // Theme.qml (design tokens only, no C++), which is the more usual
    // Qt6 pattern — but in this project's build it consistently resolved
    // at runtime to an empty placeholder object (every property
    // undefined) despite loading without any error. Promoting it to a
    // small C++ class sidesteps that; see ThemeProvider.h.
    qmlRegisterSingletonInstance("IronwatchApp", 1, 0, "App", &controller);
    qmlRegisterSingletonInstance("IronwatchApp", 1, 0, "Theme", &themeProvider);
    ThreatIntelController intel(&controller /* for shared BackendSettings */);
    qmlRegisterSingletonInstance("IronwatchApp", 1, 0, "Intel", &intel);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    // Qt::loadFromModule() (Qt 6.5+) would be more concise, but this URL
    // form works from Qt 6.2 on wherever qt_add_qml_module generates its
    // resource path, so it's kept for broader Qt6 compatibility.
    engine.load(QUrl(QStringLiteral("qrc:/Ironwatch/qml/Main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
