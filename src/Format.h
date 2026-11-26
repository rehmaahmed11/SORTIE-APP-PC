// Small formatting helpers shared by the floating dial and the transfer window.
#pragma once

#include <QString>

namespace Format {

// 0 -> "0 B", 1536 -> "1.5 KB", 4823449600 -> "4.49 GB"
QString bytes(qint64 value, int precision = 2);

// 41000 -> "41 s", 125000 -> "2 min 05 s", 4300000 -> "1 h 11 min"
QString duration(qint64 milliseconds);

// 79000000 -> "79 MB/s"
QString speed(qint64 bytesPerSecond);

} // namespace Format
