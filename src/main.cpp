#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSurfaceFormat>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ASTROCHRON"));
    QCoreApplication::setApplicationName(QStringLiteral("ASTROCHRON"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("星纪"));
    QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::China));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QTranslator translations;
    if (translations.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&translations);

    QSurfaceFormat format;
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    engine.setUiLanguage(QStringLiteral("zh_CN"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Astrochron"), QStringLiteral("Main"));
    return app.exec();
}
