// AMS Sortie - design tokens, fonts and stylesheets.
#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>

namespace Theme {

// ---------------------------------------------------------------- palette
inline QColor bg() { return QColor(0x0B, 0x0F, 0x12); }
inline QColor panel() { return QColor(0x12, 0x18, 0x1D); }
inline QColor panelHi() { return QColor(0x1B, 0x24, 0x2B); }
inline QColor stroke() { return QColor(0x2A, 0x34, 0x3D); }
inline QColor strokeSoft() { return QColor(0x22, 0x2B, 0x33); }
inline QColor text() { return QColor(0xE8, 0xED, 0xF2); }
inline QColor textDim() { return QColor(0x93, 0xA3, 0xB1); }
inline QColor textFaint() { return QColor(0x62, 0x71, 0x7D); }
inline QColor accent() { return QColor(0x00, 0xE5, 0xB0); }      // mint
inline QColor accentDeep() { return QColor(0x00, 0xA8, 0x82); }
inline QColor amber() { return QColor(0xFF, 0xB0, 0x20); }
inline QColor danger() { return QColor(0xFF, 0x6B, 0x6B); }
inline QColor whatsapp() { return QColor(0x25, 0xD3, 0x66); }
inline QColor whatsappDeep() { return QColor(0x0E, 0x8A, 0x4E); }

// Colour used for a petal at the given index (cycles).
QColor petalColor(int index);

// ---------------------------------------------------------------- fonts
// A font family list that exists on Windows first, then safe fall-backs.
QFont font(int pixelSize, bool bold = false, qreal letterSpacing = 0.0);
QFont monoFont(int pixelSize);

// ---------------------------------------------------------------- styles
QString appStyle();       // whole application (dialogs, labels, buttons, inputs)
QString menuStyle();      // QMenu / tray menu
QString buttonStyle();    // primary action button
QString progressStyle();  // QProgressBar

} // namespace Theme
