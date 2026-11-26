#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

namespace {
constexpr char kRunKey[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr char kValueName[] = "AMSSortie";

#ifdef Q_OS_UNIX
QString desktopFilePath()
{
    const QString configHome = QDir::homePath() + QStringLiteral("/.config/autostart");
    QDir().mkpath(configHome);
    return QDir(configHome).filePath(QStringLiteral("ams-sortie.desktop"));
}
#endif
} // namespace

namespace Autostart {

bool isEnabled()
{
#ifdef Q_OS_WIN
    QSettings settings(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return settings.contains(QString::fromLatin1(kValueName));
#elif defined(Q_OS_UNIX)
    return QFile::exists(desktopFilePath());
#else
    return false;
#endif
}

void setEnabled(bool on)
{
#ifdef Q_OS_WIN
    QSettings settings(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (on) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue(QString::fromLatin1(kValueName), QStringLiteral("\"%1\"").arg(exe));
    } else {
        settings.remove(QString::fromLatin1(kValueName));
    }
    settings.sync();
#elif defined(Q_OS_UNIX)
    const QString path = desktopFilePath();
    if (!on) {
        QFile::remove(path);
        return;
    }
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=AMS Sortie\n"
            << "Comment=Drop it. Sorted.\n"
            << "Exec=" << QCoreApplication::applicationFilePath() << "\n"
            << "Terminal=false\n"
            << "X-GNOME-Autostart-enabled=true\n";
    }
#else
    Q_UNUSED(on)
#endif
}

} // namespace Autostart
