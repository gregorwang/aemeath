#include "runtime/app_logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace {

QMutex g_logMutex;
QFile *g_logFile = nullptr;
bool g_debugMode = false;

QString levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker locker(&g_logMutex);

    if (!g_logFile || !g_logFile->isOpen()) {
        return;
    }

    if (!g_debugMode && type == QtDebugMsg) {
        return;
    }

    QTextStream stream(g_logFile);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
           << ' '
           << '[' << levelName(type) << ']'
           << ' ';

    if (context.category && *context.category) {
        stream << context.category << ' ';
    }

    stream << message << '\n';
    stream.flush();

    if (type == QtFatalMsg) {
        abort();
    }
}

} // namespace

void AppLogger::initialize(const QString &logFilePath, bool debugMode)
{
    QMutexLocker locker(&g_logMutex);

    g_debugMode = debugMode;

    if (g_logFile) {
        if (g_logFile->isOpen()) {
            g_logFile->close();
        }
        delete g_logFile;
        g_logFile = nullptr;
    }

    QFileInfo info(logFilePath);
    QDir().mkpath(info.absolutePath());

    g_logFile = new QFile(logFilePath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(g_logFile);
        stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
               << " [INFO] logger initialized\n";
        stream.flush();
    }

    qInstallMessageHandler(messageHandler);
}
