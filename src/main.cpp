#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSettings>
#include <QSurfaceFormat>
#include <QTranslator>
#include "app_state.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ASTROCHRON"));
    QCoreApplication::setApplicationName(QStringLiteral("ASTROCHRON"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ASTROCHRON_VERSION));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("ASTROCHRON"));
    app.setWindowIcon(QIcon(QStringLiteral(":/app/astrochron.ico")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QTranslator translations;
    QTranslator applicationTranslations;
    const auto applyLanguage = [&](const QString &language) {
        const bool chinese = language == "zh_CN" || (language != "en" && QLocale::system().language() == QLocale::Chinese);
        app.removeTranslator(&translations);
        app.removeTranslator(&applicationTranslations);
        QLocale::setDefault(chinese ? QLocale(QLocale::Chinese, QLocale::China) : QLocale(QLocale::English, QLocale::UnitedStates));
        if (chinese && translations.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app.installTranslator(&translations);
        if (!chinese && applicationTranslations.load(QStringLiteral(":/i18n/astrochron_en.qm")))
            app.installTranslator(&applicationTranslations);
        return chinese ? QStringLiteral("zh_CN") : QStringLiteral("en");
    };
    const auto uiLanguage = applyLanguage(QSettings().value("appearance/language", "system").toString());

    QSurfaceFormat format;
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QQmlApplicationEngine engine;
    engine.setUiLanguage(uiLanguage);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("Astrochron"), QStringLiteral("Main"));
    if (!engine.rootObjects().isEmpty()) {
        if (auto *state = engine.rootObjects().first()->findChild<AppState *>()) {
            QObject::connect(state, &AppState::languageChanged, &engine, [&engine, &applyLanguage, state] {
                engine.setUiLanguage(applyLanguage(state->language()));
                engine.retranslate();
                state->retranslate();
            });
        }
    }
    return app.exec();
}
