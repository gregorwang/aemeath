#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

#include "app/application_bootstrap.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(QStringList{ QStringLiteral("autostart") }, QStringLiteral("Launched from Windows Run key.")));
    parser.addOption(QCommandLineOption(QStringList{ QStringLiteral("start-minimized") }, QStringLiteral("Force hidden startup regardless of saved visibility state.")));
    parser.process(app);
    app.setProperty("launch.source.autostart", parser.isSet(QStringLiteral("autostart")));
    app.setProperty("launch.forceStartMinimized", parser.isSet(QStringLiteral("start-minimized")));

    ApplicationBootstrap bootstrap(app);
    if (!bootstrap.initialize()) {
        return 0;
    }

    return app.exec();
}
