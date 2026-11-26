#include "Format.h"

#include <QLocale>
#include <QtMath>

namespace Format {

QString bytes(qint64 value, int precision)
{
    const qint64 abs = qAbs(value);
    if (abs < 1024)
        return QStringLiteral("%1 B").arg(value);

    static const char *units[] = { "KB", "MB", "GB", "TB", "PB" };
    double scaled = double(abs) / 1024.0;
    int unit = 0;
    while (scaled >= 1024.0 && unit < 4) {
        scaled /= 1024.0;
        ++unit;
    }

    // Keep the number short: 3 significant digits is what Explorer shows.
    int decimals = precision;
    if (scaled >= 100.0)
        decimals = 0;
    else if (scaled >= 10.0)
        decimals = 1;

    const QString number = QLocale::c().toString(scaled, 'f', decimals);
    return QStringLiteral("%1 %2").arg(number, QString::fromLatin1(units[unit]));
}

QString duration(qint64 milliseconds)
{
    if (milliseconds < 0)
        return QStringLiteral("--");
    const qint64 totalSeconds = qRound64(double(milliseconds) / 1000.0);
    if (totalSeconds < 60)
        return QStringLiteral("%1 s").arg(totalSeconds);

    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    if (minutes < 60)
        return QStringLiteral("%1 min %2 s").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));

    const qint64 hours = minutes / 60;
    const qint64 restMinutes = minutes % 60;
    return QStringLiteral("%1 h %2 min").arg(hours).arg(restMinutes);
}

QString speed(qint64 bytesPerSecond)
{
    if (bytesPerSecond <= 0)
        return QStringLiteral("--");
    return QStringLiteral("%1/s").arg(bytes(bytesPerSecond, 1));
}

} // namespace Format
