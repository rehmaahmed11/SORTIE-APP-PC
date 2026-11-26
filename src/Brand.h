// AMS Sortie - brand constants.
//
// Everything the user sees about the publisher lives here, so the product can be
// re-branded from a single place.
#pragma once

#include <QString>
#include <QUrl>

#ifndef SORTIE_VERSION
#define SORTIE_VERSION "1.0.0"
#endif

namespace Brand {

inline QString productName() { return QStringLiteral("AMS Sortie"); }
inline QString shortName() { return QStringLiteral("Sortie"); }
inline QString company() { return QStringLiteral("AMS"); }
inline QString author() { return QStringLiteral("Rehman Ahmed"); }
inline QString phoneDisplay() { return QStringLiteral("+92 306 0706989"); }
inline QString phoneRaw() { return QStringLiteral("+923060706989"); }
inline QString whatsAppNumber() { return QStringLiteral("923060706989"); }
inline QString email() { return QStringLiteral("rehmanahmed92@yandex.com"); }
inline QString tagline() { return QStringLiteral("Drop it. Sorted."); }
inline QString version() { return QString::fromLatin1(SORTIE_VERSION); }
inline QString copyrightLine()
{
    return QStringLiteral("Copyright (C) 2026 Rehman Ahmed (AMS)");
}
inline QString ownerLine()
{
    return QStringLiteral("AMS  \xE2\x80\xA2  Rehman Ahmed  \xE2\x80\xA2  +92 306 0706989");
}

// wa.me deep link with an optional pre-filled message.
inline QString whatsAppLink(const QString &message = QString())
{
    QString url = QStringLiteral("https://wa.me/") + whatsAppNumber();
    if (!message.isEmpty())
        url += QStringLiteral("?text=") + QString::fromLatin1(QUrl::toPercentEncoding(message));
    return url;
}

inline QString callLink() { return QStringLiteral("tel:") + phoneRaw(); }

} // namespace Brand
