#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ASTROCHRON"));
    QCoreApplication::setApplicationName(QStringLiteral("ASTROCHRON"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ASTROCHRON_VERSION));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("ASTROCHRON"));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/astrochron.ico")));
    const auto language = QSettings().value("appearance/language", "system").toString();
    const bool chinese = language == "zh_CN" || (language != "en" && QLocale::system().language() == QLocale::Chinese);
    QLocale::setDefault(chinese ? QLocale(QLocale::Chinese, QLocale::China) : QLocale(QLocale::English, QLocale::UnitedStates));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QTranslator translations;
    if (chinese && translations.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&translations);
    QTranslator applicationTranslations;
    if (!chinese && applicationTranslations.load(QStringLiteral(":/i18n/astrochron_en.qm")))
        app.installTranslator(&applicationTranslations);

    QSurfaceFormat format;
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    engine.setUiLanguage(chinese ? QStringLiteral("zh_CN") : QStringLiteral("en"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Astrochron"), QStringLiteral("Main"));
    return app.exec();
}
