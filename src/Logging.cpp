#include "Logging.h"

#include "Brand.h"
#include "Config.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QTextStream>

#include <cstdlib>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QMutex g_mutex;
QString g_logPath;
constexpr qint64 kMaxLogBytes = 2 * 1024 * 1024;

void rotateIfNeeded()
{
    QFileInfo info(g_logPath);
    if (!info.exists() || info.size() < kMaxLogBytes)
        return;
    const QString old = g_logPath + QStringLiteral(".1");
    QFile::remove(old);
    QFile::rename(g_logPath, old);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const char *level = "INFO";
    switch (type) {
    case QtDebugMsg: level = "DEBUG"; break;
    case QtInfoMsg: level = "INFO"; break;
    case QtWarningMsg: level = "WARN"; break;
    case QtCriticalMsg: level = "ERROR"; break;
    case QtFatalMsg: level = "FATAL"; break;
    }

    const QString line = QStringLiteral("%1 [%2] %3%4")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(level), message,
                                  context.file ? QStringLiteral("   (%1:%2)")
                                                     .arg(QString::fromUtf8(context.file))
                                                     .arg(context.line)
                                               : QString());

    {
        QMutexLocker locker(&g_mutex);
        rotateIfNeeded();
        QFile file(g_logPath);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << line << '\n';
        }
    }

    if (type == QtFatalMsg)
        abort();
}

#ifdef Q_OS_WIN
LONG WINAPI crashFilter(EXCEPTION_POINTERS *info)
{
    QString code = QStringLiteral("unknown");
    if (info && info->ExceptionRecord)
        code = QStringLiteral("0x%1")
                   .arg(quintptr(info->ExceptionRecord->ExceptionCode), 0, 16);

    const QString text = QStringLiteral("%1 [CRASH] unhandled exception %2\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODate), code);
    {
        QMutexLocker locker(&g_mutex);
        QFile file(g_logPath);
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out << text;
        }
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

namespace Log {

void init()
{
    g_logPath = QDir(ConfigStore::logFolderPath())
                    .filePath(QStringLiteral("sortie-%1.log")
                                  .arg(QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))));

    qInstallMessageHandler(messageHandler);
    qInfo().noquote() << QStringLiteral("%1 %2 starting (Qt %3)")
                             .arg(Brand::productName(), Brand::version(),
                                  QString::fromLatin1(qVersion()));

#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(crashFilter);
#endif
}

QString filePath() { return g_logPath; }

QString folderPath() { return ConfigStore::logFolderPath(); }

} // namespace Log
