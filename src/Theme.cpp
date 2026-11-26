#include "Theme.h"

#include <QFontDatabase>

namespace Theme {

namespace {
const QColor kPetalColors[] = {
    QColor(0x00, 0xE5, 0xB0), QColor(0xFF, 0xB0, 0x20), QColor(0x4F, 0xA8, 0xFF),
    QColor(0xFF, 0x7A, 0xC6), QColor(0xA7, 0x8B, 0xFA), QColor(0x5B, 0xE3, 0x7D),
    QColor(0xFF, 0x8A, 0x5C), QColor(0x38, 0xD9, 0xD9), QColor(0xF2, 0xD9, 0x4C),
    QColor(0x7E, 0x9C, 0xFF), QColor(0xE0, 0x7B, 0xFF), QColor(0x8F, 0xD6, 0x6A),
};
constexpr int kPetalColorCount = int(sizeof(kPetalColors) / sizeof(kPetalColors[0]));
} // namespace

QColor petalColor(int index)
{
    if (index < 0)
        return accent();
    return kPetalColors[index % kPetalColorCount];
}

QFont font(int pixelSize, bool bold, qreal letterSpacing)
{
    QFont f;
    f.setFamilies({ QStringLiteral("Segoe UI Variable Text"),
                    QStringLiteral("Segoe UI"),
                    QStringLiteral("Inter"),
                    QStringLiteral("Noto Sans"),
                    QStringLiteral("DejaVu Sans") });
    f.setPixelSize(qMax(1, pixelSize));
    f.setBold(bold);
    if (letterSpacing != 0.0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    return f;
}

QFont monoFont(int pixelSize)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setFamilies({ QStringLiteral("Cascadia Mono"),
                    QStringLiteral("Consolas"),
                    QStringLiteral("DejaVu Sans Mono"),
                    f.family() });
    f.setPixelSize(qMax(1, pixelSize));
    return f;
}

QString appStyle()
{
    return QStringLiteral(
        "QWidget { color:#E8EDF2; font-family:'Segoe UI Variable Text','Segoe UI','Inter','Noto Sans','DejaVu Sans'; }"
        "QDialog { background-color:#0B0F12; }"
        "QLabel { background:transparent; color:#E8EDF2; }"
        "QLabel#dimLabel { color:#93A3B1; }"
        "QLabel#faintLabel { color:#62717D; }"
        "QLabel#valueLabel { color:#E8EDF2; font-weight:600; }"
        "QLabel#titleLabel { color:#E8EDF2; font-weight:700; }"
        "QPushButton {"
        "  background-color:#1B242B; color:#E8EDF2; border:1px solid #2A343D;"
        "  border-radius:8px; padding:8px 18px; min-height:20px; font-weight:600; }"
        "QPushButton:hover { background-color:#243039; border-color:#3A4753; }"
        "QPushButton:pressed { background-color:#151C22; }"
        "QPushButton:disabled { color:#5A6772; border-color:#20272E; background-color:#131A1F; }"
        "QPushButton#primaryButton {"
        "  background-color:#00E5B0; color:#03110D; border:1px solid #00E5B0; }"
        "QPushButton#primaryButton:hover { background-color:#2BF0C1; border-color:#2BF0C1; }"
        "QPushButton#primaryButton:pressed { background-color:#00BE93; }"
        "QPushButton#dangerButton { background-color:#2A1719; color:#FF8F8F; border-color:#4A2A2E; }"
        "QPushButton#dangerButton:hover { background-color:#3A1D20; }"
        "QPushButton#linkButton { background:transparent; border:none; color:#00E5B0; padding:2px; text-align:left; }"
        "QPushButton#linkButton:hover { color:#54FFD3; text-decoration:underline; }"
        "QLineEdit, QComboBox {"
        "  background-color:#12181D; color:#E8EDF2; border:1px solid #2A343D;"
        "  border-radius:8px; padding:7px 10px; selection-background-color:#00A882; }"
        "QLineEdit:focus, QComboBox:focus { border-color:#00E5B0; }"
        "QCheckBox { spacing:9px; color:#C9D4DC; }"
        "QCheckBox::indicator { width:16px; height:16px; border-radius:4px;"
        "  border:1px solid #3A4753; background:#12181D; }"
        "QCheckBox::indicator:checked { background:#00E5B0; border-color:#00E5B0; }"
        "QProgressBar { border:1px solid #2A343D; border-radius:7px; background:#0F1418;"
        "  text-align:center; color:#E8EDF2; height:20px; font-weight:600; }"
        "QProgressBar::chunk { border-radius:6px;"
        "  background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2BF0C1, stop:1 #00A882); }"
        "QScrollArea, QScrollArea > QWidget > QWidget { background:transparent; }"
        "QToolTip { background-color:#12181D; color:#E8EDF2; border:1px solid #2A343D;"
        "  border-radius:6px; padding:5px 8px; font-size:12px; }"
        "QMenu { background-color:#12181D; color:#E8EDF2; border:1px solid #2A343D;"
        "  border-radius:10px; padding:6px; }"
        "QMenu::item { padding:8px 30px 8px 12px; border-radius:6px; margin:2px; }"
        "QMenu::item:selected { background-color:#1B2A32; color:#00E5B0; }"
        "QMenu::item:disabled { color:#5A6772; }"
        "QMenu::separator { height:1px; background:#242E36; margin:6px 10px; }"
        "QMenu::icon { padding-left:8px; }");
}

QString menuStyle()
{
    return appStyle();
}

QString buttonStyle()
{
    return QStringLiteral(
        "QPushButton { background-color:#00E5B0; color:#03110D; border:none;"
        " border-radius:8px; padding:8px 18px; font-weight:700; }"
        "QPushButton:hover { background-color:#2BF0C1; }");
}

QString progressStyle()
{
    return QStringLiteral(
        "QProgressBar { border:1px solid #2A343D; border-radius:7px; background:#0F1418;"
        " text-align:center; color:#E8EDF2; height:20px; font-weight:600; }"
        "QProgressBar::chunk { border-radius:6px;"
        " background-color:qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #2BF0C1, stop:1 #00A882); }");
}

} // namespace Theme
