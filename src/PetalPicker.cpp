#include "PetalPicker.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#include "Format.h"
#include "Theme.h"

namespace {

constexpr int kWidth = 392;
constexpr int kPad = 10;
constexpr int kHeaderH = 64;
constexpr int kRowH = 54;
constexpr int kFooterH = 40;
constexpr int kMaxRows = 9;

void drawFolder(QPainter &p, const QRectF &r, const QColor &color)
{
    QPainterPath body;
    body.addRoundedRect(QRectF(r.left(), r.top() + r.height() * 0.22, r.width(),
                               r.height() * 0.78),
                        3, 3);
    QPainterPath tab;
    tab.addRoundedRect(QRectF(r.left(), r.top(), r.width() * 0.52, r.height() * 0.34), 3, 3);
    QPainterPath shape = body.united(tab);

    QColor fill = color;
    fill.setAlpha(48);
    p.setBrush(fill);
    p.setPen(QPen(color, 1.4));
    p.drawPath(shape);
}

QString elide(const QFontMetrics &fm, const QString &text, int width)
{
    return fm.elidedText(text, Qt::ElideMiddle, qMax(20, width));
}

} // namespace

PetalPicker::PetalPicker(const QStringList &items, const QList<Petal> &petals, bool copyMode,
                         QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_items(items)
    , m_petals(petals)
    , m_copyMode(copyMode)
{
    if (m_petals.size() > kMaxRows)
        m_petals = m_petals.mid(0, kMaxRows);

    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    const int rows = qMax(1, int(m_petals.size())) + 1; // petals + "add" row
    resize(kWidth, kPad * 2 + kHeaderH + rows * kRowH + kFooterH);
}

QString PetalPicker::itemsLabel() const
{
    if (m_items.size() == 1)
        return QFileInfo(m_items.first()).fileName();
    return QStringLiteral("%1 items").arg(m_items.size());
}

int PetalPicker::rowCount() const
{
    return int(m_petals.size()) + 1; // + add row
}

QRect PetalPicker::rowRect(int row) const
{
    return QRect(kPad, kPad + kHeaderH + row * kRowH, width() - kPad * 2, kRowH);
}

int PetalPicker::rowAt(const QPoint &pos) const
{
    for (int i = 0; i < rowCount(); ++i) {
        if (rowRect(i).contains(pos))
            return i;
    }
    return -1;
}

void PetalPicker::updateHover(int row)
{
    if (row == m_hover)
        return;
    m_hover = row;
    update();
}

void PetalPicker::popupAt(const QPoint &globalPos)
{
    QRect area;
    if (const QScreen *screen = QGuiApplication::screenAt(globalPos))
        area = screen->availableGeometry();
    else if (const QScreen *primary = QGuiApplication::primaryScreen())
        area = primary->availableGeometry();

    QPoint topLeft = globalPos + QPoint(14, 14);
    if (area.isValid()) {
        if (topLeft.x() + width() > area.right())
            topLeft.setX(globalPos.x() - width() - 14);
        if (topLeft.y() + height() > area.bottom())
            topLeft.setY(globalPos.y() - height() - 14);
        topLeft.setX(qBound(area.left() + 8, topLeft.x(), area.right() - width() - 8));
        topLeft.setY(qBound(area.top() + 8, topLeft.y(), area.bottom() - height() - 8));
    }

    move(topLeft);
    show();
    raise();
    activateWindow();
    setFocus(Qt::PopupFocusReason);
}

void PetalPicker::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // Card
    const QRectF card = QRectF(rect()).adjusted(1, 1, -1, -1);
    QLinearGradient bg(card.topLeft(), card.bottomLeft());
    bg.setColorAt(0.0, QColor(0x15, 0x1D, 0x23));
    bg.setColorAt(1.0, QColor(0x0D, 0x13, 0x17));
    p.setBrush(bg);
    p.setPen(QPen(Theme::stroke(), 1));
    p.drawRoundedRect(card, 14, 14);

    // Header: what is being moved
    const QString caption = m_copyMode ? QStringLiteral("COPY TO") : QStringLiteral("MOVE TO");
    p.setPen(Theme::accent());
    p.setFont(Theme::font(10, true, 1.6));
    p.drawText(QRect(kPad + 6, kPad + 10, 180, 16), Qt::AlignLeft | Qt::AlignVCenter, caption);

    p.setPen(Theme::text());
    p.setFont(Theme::font(15, true));
    p.drawText(QRect(kPad + 6, kPad + 26, width() - kPad * 2 - 12, 22),
               Qt::AlignLeft | Qt::AlignVCenter,
               elide(QFontMetrics(Theme::font(15, true)), itemsLabel(), width() - kPad * 2 - 30));

    p.setPen(Theme::textFaint());
    p.setFont(Theme::font(10));
    p.drawText(QRect(kPad + 6, kPad + 45, width() - kPad * 2 - 12, 14),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Choose a destination folder"));

    for (int row = 0; row < rowCount(); ++row) {
        const QRect r = rowRect(row);
        const bool hovered = (row == m_hover);
        const bool isAdd = (row == addRowIndex());

        if (hovered) {
            QColor fill = isAdd ? QColor(0x12, 0x2A, 0x25) : Theme::panelHi();
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawRoundedRect(QRectF(r).adjusted(0, 1, 0, -1), 9, 9);
        }

        const int iconSize = 22;
        const QRectF iconRect(r.left() + 12, r.center().y() - iconSize / 2.0, iconSize, iconSize);

        if (isAdd) {
            p.setPen(QPen(Theme::accent(), 1.6));
            p.setBrush(Qt::NoBrush);
            const QPointF c = iconRect.center();
            p.drawEllipse(c, 10, 10);
            p.drawLine(QPointF(c.x() - 4.5, c.y()), QPointF(c.x() + 4.5, c.y()));
            p.drawLine(QPointF(c.x(), c.y() - 4.5), QPointF(c.x(), c.y() + 4.5));

            p.setPen(Theme::accent());
            p.setFont(Theme::font(13, true));
            p.drawText(QRect(r.left() + 46, r.top(), r.width() - 60, r.height()),
                       Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Add a new folder..."));
            continue;
        }

        drawFolder(p, iconRect, Theme::petalColor(row));

        const Petal &petal = m_petals.at(row);
        const bool missing = !QFileInfo(petal.path).isDir();
        const QFont nameFont = Theme::font(13, true);
        const QFont pathFont = Theme::font(10);
        const int textLeft = r.left() + 46;
        const int textWidth = r.width() - 46 - 30;

        p.setPen(missing ? Theme::textDim() : Theme::text());
        p.setFont(nameFont);
        p.drawText(QRect(textLeft, r.top() + 7, textWidth, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   elide(QFontMetrics(nameFont),
                         missing ? petal.name + QStringLiteral("  (will be created)") : petal.name,
                         textWidth));

        p.setPen(Theme::textFaint());
        p.setFont(pathFont);
        p.drawText(QRect(textLeft, r.top() + 25, textWidth, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   elide(QFontMetrics(pathFont), QDir::toNativeSeparators(petal.path), textWidth));

        // 1..9 shortcut chip
        if (row < 9) {
            const QRectF chip(r.right() - 26, r.center().y() - 9, 18, 18);
            p.setPen(QPen(Theme::stroke(), 1));
            p.setBrush(QColor(0x0E, 0x14, 0x18));
            p.drawRoundedRect(chip, 5, 5);
            p.setPen(Theme::textFaint());
            p.setFont(Theme::font(9, true));
            p.drawText(chip, Qt::AlignCenter, QString::number(row + 1));
        }
    }

    // Footer
    p.setPen(Theme::textFaint());
    p.setFont(Theme::font(10));
    p.drawText(QRect(kPad + 6, height() - kFooterH, width() - kPad * 2 - 12, kFooterH),
               Qt::AlignLeft | Qt::AlignVCenter,
               m_copyMode ? QStringLiteral("Ctrl held: files will be copied, originals stay")
                          : QStringLiteral("Esc to cancel  -  hold Ctrl while dropping to copy"));
}

void PetalPicker::mouseMoveEvent(QMouseEvent *event)
{
    updateHover(rowAt(event->position().toPoint()));
    QWidget::mouseMoveEvent(event);
}

void PetalPicker::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = rowAt(event->position().toPoint());
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void PetalPicker::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const int row = rowAt(event->position().toPoint());
        if (row >= 0 && row == m_pressed)
            choose(row);
        m_pressed = -1;
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PetalPicker::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        close();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_hover >= 0)
            choose(m_hover);
        event->accept();
        return;
    case Qt::Key_Down:
        updateHover(qMin(rowCount() - 1, m_hover + 1));
        event->accept();
        return;
    case Qt::Key_Up:
        updateHover(qMax(0, m_hover - 1));
        event->accept();
        return;
    default:
        break;
    }

    const int key = event->key();
    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        const int row = key - Qt::Key_1;
        if (row < rowCount())
            choose(row);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void PetalPicker::choose(int row)
{
    if (row == addRowIndex()) {
        m_dismissed = true;
        emit addFolderRequested();
        close();
        return;
    }
    if (row < 0 || row >= m_petals.size())
        return;
    m_dismissed = true;
    emit petalChosen(row);
    close();
}

void PetalPicker::leaveEvent(QEvent *event)
{
    updateHover(-1);
    QWidget::leaveEvent(event);
}

void PetalPicker::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (!m_dismissed) {
        m_dismissed = true;
        emit dismissed();
    }
}
