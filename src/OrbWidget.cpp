#include "OrbWidget.h"

#include "AboutDialog.h"
#include "Autostart.h"
#include "Brand.h"
#include "Config.h"
#include "Format.h"
#include "Logging.h"
#include "PetalPicker.h"
#include "Theme.h"
#include "TransferManager.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineF>
#include <QLinearGradient>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QScreen>
#include <QToolTip>
#include <QUrl>
#include <QtMath>

#include <cmath>

namespace {

constexpr int kStatusMs = 5200;

QString elided(const QFont &font, const QString &text, qreal width)
{
    return QFontMetrics(font).elidedText(text, Qt::ElideRight, int(qMax<qreal>(24, width)));
}

void paintText(QPainter &painter, const QRectF &rect, const QString &text, const QFont &font,
               const QColor &color, int flags = Qt::AlignCenter)
{
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, flags, text);
}

QString phaseLabel(Transfer::Progress::Phase phase)
{
    switch (phase) {
    case Transfer::Progress::Scanning: return QStringLiteral("MEASURING");
    case Transfer::Progress::Transferring: return QStringLiteral("MOVING");
    case Transfer::Progress::Finishing: return QStringLiteral("TIDYING UP");
    case Transfer::Progress::Completed: return QStringLiteral("DONE");
    case Transfer::Progress::Cancelled: return QStringLiteral("CANCELLED");
    case Transfer::Progress::Failed: return QStringLiteral("CHECK LOG");
    default: return QStringLiteral("READY");
    }
}

} // namespace

OrbWidget::OrbWidget(QWidget *parent) : FloatingWindow(parent)
{
    setWindowTitle(Brand::productName());
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app.ico")));
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    setSizeLimits(ConfigStore::kMinOrbSize, ConfigStore::kMaxOrbSize);
    setDefaultSide(ConfigStore::kDefaultOrbSize);

    AppConfig &cfg = ConfigStore::data();
    cfg.orbSize = qBound(ConfigStore::kMinOrbSize, cfg.orbSize, ConfigStore::kMaxOrbSize);

    const QRect area = QGuiApplication::primaryScreen()
                           ? QGuiApplication::primaryScreen()->availableGeometry()
                           : QRect(0, 0, 1280, 720);
    ConfigStore::applyDefaultsForScreen(area);
    ConfigStore::save();

    setSnapEnabled(cfg.snapToEdges);
    setResizable(!cfg.lockPosition);
    setDraggable(!cfg.lockPosition);
    applyWindowFlags();
    restorePlacement(cfg.orbGeometry, cfg.hasOrbGeometry, cfg.orbSize,
                     QPoint(area.right() - cfg.orbSize - 48, area.bottom() - cfg.orbSize - 72));
    setAnchorSpec(cfg.orbAnchor);

    connect(this, &FloatingWindow::placementChanged, this, &OrbWidget::savePlacement);
    connect(this, &FloatingWindow::sideLengthChanged, this, [this](int side) {
        AppConfig &c = ConfigStore::data();
        c.orbSize = side;
        ConfigStore::save();
    });

    m_statusTimer.setSingleShot(true);
    m_statusTimer.setInterval(kStatusMs);
    connect(&m_statusTimer, &QTimer::timeout, this, &OrbWidget::clearStatus);

    m_animTimer.setInterval(40);
    connect(&m_animTimer, &QTimer::timeout, this, [this]() {
        m_animPhase += 0.10;
        if (m_animPhase > 2.0 * M_PI)
            m_animPhase -= 2.0 * M_PI;
        update();
    });

    TransferManager &manager = TransferManager::instance();
    connect(&manager, &TransferManager::started, this, &OrbWidget::onTransferStarted);
    connect(&manager, &TransferManager::progress, this, &OrbWidget::onTransferProgress);
    connect(&manager, &TransferManager::finished, this, &OrbWidget::onTransferFinished);
}

OrbWidget::~OrbWidget() = default;

// ---------------------------------------------------------------- window state

void OrbWidget::applyWindowFlags()
{
    AppConfig &cfg = ConfigStore::data();
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
    if (cfg.alwaysOnTop)
        flags |= Qt::WindowStaysOnTopHint;

    const bool wasVisible = isVisible();
    const QRect keep = geometry();
    setWindowFlags(flags);
    setGeometry(keep);
    if (wasVisible) {
        show();
        raise();
    }
}

void OrbWidget::savePlacement()
{
    AppConfig &cfg = ConfigStore::data();
    cfg.orbGeometry = QRect(pos(), QSize(width(), height()));
    cfg.hasOrbGeometry = true;
    cfg.orbSize = width();
    cfg.orbAnchor = anchorSpec();
    ConfigStore::save();
}

void OrbWidget::bringUp()
{
    show();
    raise();
    activateWindow();
}

// ---------------------------------------------------------------- geometry helpers

QRectF OrbWidget::bodyRect() const
{
    const qreal side = qMin(width(), height());
    const qreal pad = side * 0.05;
    return QRectF(rect()).adjusted(pad, pad, -pad, -pad);
}

qreal OrbWidget::ringRadius() const
{
    return bodyRect().width() / 2.0;
}

QPointF OrbWidget::badgeCenter() const
{
    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal rad = qDegreesToRadians(45.0);
    return QPointF(center.x() + radius * 0.855 * qCos(rad),
                   center.y() - radius * 0.855 * qSin(rad));
}

int OrbWidget::visiblePetalCount() const
{
    return qMin(8, int(ConfigStore::data().petals.size()));
}

OrbWidget::Hit OrbWidget::hitAt(const QPoint &pos) const
{
    Hit hit;
    const QPointF center = bodyRect().center();
    const qreal radius = ringRadius();
    const qreal side = qMin(width(), height());

    if (QLineF(badgeCenter(), QPointF(pos)).length() <= side * 0.085) {
        hit.addBadge = true;
        return hit;
    }

    const int count = visiblePetalCount();
    if (count <= 0)
        return hit;

    const QPointF delta = QPointF(pos) - center;
    const qreal distance = std::hypot(delta.x(), delta.y());
    if (distance < radius * 0.60 || distance > radius * 1.02)
        return hit;

    qreal angle = qRadiansToDegrees(std::atan2(-delta.y(), delta.x()));
    if (angle < 0)
        angle += 360.0;
    const qreal fromTop = std::fmod(angle + 90.0 + 360.0, 360.0);
    const qreal step = 360.0 / count;
    int index = int(std::floor(fromTop / step));
    if (index >= count)
        index = count - 1;
    hit.petal = index;
    return hit;
}

// ---------------------------------------------------------------- painting

void OrbWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal side = qMin(width(), height());

    const QColor accentColor = m_statusError ? Theme::danger()
                                             : (m_dragOver ? Theme::amber() : Theme::accent());
    const bool excited = m_dragOver || m_busy || !m_pending.isEmpty();

    // Outer glow
    {
        QRadialGradient glow(center, radius + side * 0.065);
        QColor inner = accentColor;
        inner.setAlpha(0);
        QColor outer = accentColor;
        outer.setAlpha(int(excited ? 105 : 44));
        glow.setColorAt(0.78, inner);
        glow.setColorAt(1.0, outer);
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(center, radius + side * 0.058, radius + side * 0.058);
    }

    // Body
    {
        QRadialGradient fill(center - QPointF(0, radius * 0.45), radius * 1.75);
        fill.setColorAt(0.0, QColor(0x21, 0x2D, 0x36));
        fill.setColorAt(0.52, QColor(0x12, 0x19, 0x1E));
        fill.setColorAt(1.0, QColor(0x08, 0x0C, 0x0F));
        painter.setBrush(fill);
        painter.setPen(QPen(QColor(0x2B, 0x38, 0x42), 1.0));
        painter.drawEllipse(body);
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 12), 1.0));
    painter.drawEllipse(body.adjusted(side * 0.05, side * 0.05, -side * 0.05, -side * 0.05));

    paintPetals(painter);
    if (m_busy)
        paintProgressRing(painter);
    paintAddBadge(painter);

    if (m_busy)
        paintBusy(painter);
    else if (!m_pending.isEmpty())
        paintChoose(painter);
    else if (!m_statusTitle.isEmpty())
        paintStatus(painter);
    else
        paintIdle(painter);

    if (m_dragOver) {
        QColor halo = Theme::amber();
        halo.setAlpha(170);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(halo, 2.0, Qt::DashLine));
        painter.drawEllipse(body.adjusted(2, 2, -2, -2));
    }
}

void OrbWidget::paintPetals(QPainter &painter)
{
    const int count = visiblePetalCount();
    if (count <= 0)
        return;

    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal side = qMin(width(), height());
    const bool choosing = !m_pending.isEmpty();

    const qreal outerR = radius * (choosing ? 0.900 : 0.910);
    const qreal innerR = radius * (choosing ? 0.700 : 0.848);
    const qreal step = 360.0 / count;
    const qreal gap = choosing ? 2.4 : 6.0;

    for (int i = 0; i < count; ++i) {
        const QColor color = Theme::petalColor(i);
        const bool hovered = (i == m_hoverPetal);
        const qreal grow = hovered ? side * 0.012 : 0.0;

        const qreal start = -90.0 + i * step + gap * 0.5;
        const qreal sweep = step - gap;

        const qreal oR = outerR + grow;
        const qreal iR = qMax(radius * 0.55, innerR - grow * 0.4);
        const QRectF outerRect(center.x() - oR, center.y() - oR, oR * 2, oR * 2);
        const QRectF innerRect(center.x() - iR, center.y() - iR, iR * 2, iR * 2);

        QPainterPath segment;
        segment.arcMoveTo(outerRect, start);
        segment.arcTo(outerRect, start, sweep);
        segment.arcTo(innerRect, start + sweep, -sweep);
        segment.closeSubpath();

        QColor fill = color;
        if (choosing)
            fill.setAlpha(hovered ? 215 : 132);
        else
            fill.setAlpha(hovered ? 190 : 62);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(segment);

        if (hovered) {
            QColor edge = color.lighter(140);
            edge.setAlpha(220);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(edge, 1.2));
            painter.drawPath(segment);
        }

        if (choosing || hovered) {
            const qreal midAngle = start + sweep / 2.0;
            const qreal midRadius = (oR + iR) / 2.0;
            const qreal rad = qDegreesToRadians(midAngle);
            const QPointF labelPos(center.x() + midRadius * qCos(rad),
                                   center.y() - midRadius * qSin(rad));
            const int fontPx = qMax(7, int((oR - iR) * 0.72));
            paintText(painter, QRectF(labelPos.x() - 14, labelPos.y() - 9, 28, 18),
                      QString::number(i + 1), Theme::font(fontPx, true),
                      QColor(0x05, 0x0A, 0x0C, 220));
        }
    }
}

void OrbWidget::paintProgressRing(QPainter &painter)
{
    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal side = qMin(width(), height());
    const qreal ringR = radius * 0.958;
    const QRectF ringRect(center.x() - ringR, center.y() - ringR, ringR * 2, ringR * 2);
    const qreal thickness = qMax(3.0, side * 0.030);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 22), thickness, Qt::SolidLine, Qt::RoundCap));
    painter.drawEllipse(ringRect);

    QColor arcColor = m_progress.phase == Transfer::Progress::Failed ? Theme::danger() : Theme::accent();
    painter.setPen(QPen(arcColor, thickness, Qt::SolidLine, Qt::RoundCap));

    if (m_progress.bytesTotal > 0) {
        const double fraction = m_progress.fraction();
        painter.drawArc(ringRect, 90 * 16, int(-fraction * 360.0 * 16));
    } else {
        // Indeterminate: a 70 degree arc that travels around the ring.
        const int start = int((90.0 - qRadiansToDegrees(m_animPhase) * 3.0) * 16);
        painter.drawArc(ringRect, start, -70 * 16);
    }
}

void OrbWidget::paintAddBadge(QPainter &painter)
{
    const QPointF center = badgeCenter();
    const qreal side = qMin(width(), height());
    const qreal radius = side * (m_hoverAdd ? 0.082 : 0.074);

    QColor fill(0x0B, 0x12, 0x16);
    fill.setAlpha(240);
    painter.setBrush(fill);
    painter.setPen(QPen(Theme::accent(), 1.3, Qt::DashLine));
    painter.drawEllipse(center, radius, radius);

    painter.setPen(QPen(Theme::accent(), qMax(1.4, side * 0.011), Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center - QPointF(radius * 0.44, 0), center + QPointF(radius * 0.44, 0));
    painter.drawLine(center - QPointF(0, radius * 0.44), center + QPointF(0, radius * 0.44));
}

void OrbWidget::paintIdle(QPainter &painter)
{
    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal side = qMin(width(), height());

    paintText(painter, QRectF(center.x() - radius, center.y() - radius * 0.60,
                              radius * 2, radius * 0.20),
              QStringLiteral("A M S"), Theme::font(qMax(8, int(side * 0.070)), true, 1.8),
              Theme::accent());

    paintText(painter, QRectF(center.x() - radius, center.y() - radius * 0.34,
                              radius * 2, radius * 0.34),
              QStringLiteral("SORTIE"), Theme::font(qMax(11, int(side * 0.150)), true, 0.8),
              Theme::text());

    QColor line = Theme::accent();
    line.setAlpha(95);
    painter.setPen(QPen(line, 1.2));
    painter.drawLine(QPointF(center.x() - radius * 0.13, center.y() + radius * 0.05),
                     QPointF(center.x() + radius * 0.13, center.y() + radius * 0.05));

    paintText(painter, QRectF(center.x() - radius, center.y() + radius * 0.10,
                              radius * 2, radius * 0.22),
              elided(Theme::font(qMax(8, int(side * 0.068))), QStringLiteral("drop files here"),
                     radius * 1.5),
              Theme::font(qMax(8, int(side * 0.068))), Theme::textDim());

    if (side >= 152) {
        paintText(painter, QRectF(center.x() - radius, center.y() + radius * 0.48,
                                  radius * 2, radius * 0.16),
                  elided(Theme::font(qMax(7, int(side * 0.048))), QStringLiteral("by Rehman Ahmed"),
                         radius * 1.3),
                  Theme::font(qMax(7, int(side * 0.048))), Theme::textFaint());
    }
}

void OrbWidget::paintChoose(QPainter &painter)
{
    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal side = qMin(width(), height());

    const QString caption = m_pendingCopy
                                ? QStringLiteral("%1  -  COPY").arg(m_pending.size() == 1
                                                                        ? QStringLiteral("1 ITEM")
                                                                        : QStringLiteral("%1 ITEMS").arg(m_pending.size()))
                                : (m_pending.size() == 1
                                       ? QStringLiteral("1 ITEM")
                                       : QStringLiteral("%1 ITEMS").arg(m_pending.size()));

    paintText(painter, QRectF(center.x() - radius, center.y() - radius * 0.56,
                              radius * 2, radius * 0.18),
              elided(Theme::font(qMax(7, int(side * 0.058)), true, 1.2), caption, radius * 1.6),
              Theme::font(qMax(7, int(side * 0.058)), true, 1.2), Theme::accent());

    const QString label = (m_hoverPetal >= 0 && m_hoverPetal < ConfigStore::data().petals.size())
                              ? ConfigStore::data().petals.at(m_hoverPetal).name
                              : QStringLiteral("pick a folder");
    const QFont bigFont = Theme::font(qMax(10, int(side * 0.098)), true);
    paintText(painter, QRectF(center.x() - radius * 0.78, center.y() - radius * 0.22,
                              radius * 1.56, radius * 0.44),
              elided(bigFont, label, radius * 1.5), bigFont, Theme::text());

    paintText(painter, QRectF(center.x() - radius, center.y() + radius * 0.26,
                              radius * 2, radius * 0.18),
              elided(Theme::font(qMax(7, int(side * 0.052))), QStringLiteral("click a petal"),
                     radius * 1.5),
              Theme::font(qMax(7, int(side * 0.052))), Theme::textDim());

    paintText(painter, QRectF(center.x() - radius, center.y() + radius * 0.44,
                              radius * 2, radius * 0.16),
              elided(Theme::font(qMax(7, int(side * 0.048))), QStringLiteral("Esc cancels"),
                     radius * 1.4),
              Theme::font(qMax(7, int(side * 0.048))), Theme::textFaint());
}

void OrbWidget::paintBusy(QPainter &painter)
{
    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal side = qMin(width(), height());

    paintText(painter, QRectF(center.x() - radius, center.y() - radius * 0.58,
                              radius * 2, radius * 0.18),
              elided(Theme::font(qMax(7, int(side * 0.054)), true, 1.4),
                     phaseLabel(m_progress.phase), radius * 1.6),
              Theme::font(qMax(7, int(side * 0.054)), true, 1.4), Theme::accent());

    QString headline;
    if (m_progress.bytesTotal > 0)
        headline = QStringLiteral("%1%").arg(int(m_progress.fraction() * 100.0));
    else
        headline = QStringLiteral("...");

    paintText(painter, QRectF(center.x() - radius, center.y() - radius * 0.30,
                              radius * 2, radius * 0.42),
              headline, Theme::font(qMax(13, int(side * 0.175)), true), Theme::text());

    const QString sizesText =
        QStringLiteral("%1 of %2")
            .arg(Format::bytes(m_progress.bytesDone), Format::bytes(m_progress.bytesTotal));
    const QFont sizesFont = Theme::font(qMax(7, int(side * 0.056)), false);
    paintText(painter, QRectF(center.x() - radius * 0.92, center.y() + radius * 0.13,
                              radius * 1.84, radius * 0.18),
              elided(sizesFont, sizesText, radius * 1.8), sizesFont, Theme::textDim());

    QString detail = Format::speed(m_progress.bytesPerSecond);
    if (m_progress.msRemaining > 0)
        detail += QStringLiteral("  -  %1 left").arg(Format::duration(m_progress.msRemaining));

    const QFont detailFont = Theme::font(qMax(7, int(side * 0.046)));
    paintText(painter, QRectF(center.x() - radius * 0.9, center.y() + radius * 0.36,
                              radius * 1.8, radius * 0.16),
              elided(detailFont, detail, radius * 1.75), detailFont, Theme::textFaint());
}

void OrbWidget::paintStatus(QPainter &painter)
{
    const QRectF body = bodyRect();
    const QPointF center = body.center();
    const qreal radius = body.width() / 2.0;
    const qreal side = qMin(width(), height());
    const QColor color = m_statusError ? Theme::danger() : Theme::accent();

    paintText(painter, QRectF(center.x() - radius, center.y() - radius * 0.30,
                              radius * 2, radius * 0.30),
              elided(Theme::font(qMax(10, int(side * 0.100)), true, 1.0), m_statusTitle,
                     radius * 1.5),
              Theme::font(qMax(10, int(side * 0.100)), true, 1.0), color);

    const QFont detailFont = Theme::font(qMax(7, int(side * 0.054)));
    painter.setFont(detailFont);
    painter.setPen(Theme::textDim());
    const QRectF detailRect(center.x() - radius * 0.78, center.y() + radius * 0.04,
                            radius * 1.56, radius * 0.52);
    painter.drawText(detailRect, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                     m_statusDetail);

    if (m_hasResult && side >= 160) {
        paintText(painter, QRectF(center.x() - radius, center.y() + radius * 0.60,
                                  radius * 2, radius * 0.14),
                  QStringLiteral("%1 s").arg(m_lastResult.elapsedMs / 1000.0, 0, 'f', 1),
                  Theme::font(qMax(7, int(side * 0.044))), Theme::textFaint());
    }
}

// ---------------------------------------------------------------- status

void OrbWidget::setStatus(const QString &title, const QString &detail, bool error)
{
    m_statusTitle = title;
    m_statusDetail = detail;
    m_statusError = error;
    m_statusTimer.start(kStatusMs);
    update();
}

void OrbWidget::clearStatus()
{
    m_statusTitle.clear();
    m_statusDetail.clear();
    m_statusError = false;
    setToolTip(QString());
    update();
}

// ---------------------------------------------------------------- mouse

void OrbWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const Hit hit = hitAt(event->position().toPoint());
        if (hit.addBadge) {
            m_pressedAdd = true;
            update();
            event->accept();
            return;
        }
        if (hit.petal >= 0) {
            m_pressedPetal = hit.petal;
            event->accept();
            return;
        }
    }
    FloatingWindow::mousePressEvent(event);
}

void OrbWidget::mouseMoveEvent(QMouseEvent *event)
{
    const Hit hit = hitAt(event->position().toPoint());
    if (hit.petal != m_hoverPetal || hit.addBadge != m_hoverAdd) {
        m_hoverPetal = hit.petal;
        m_hoverAdd = hit.addBadge;
        if (hit.petal >= 0 && hit.petal < ConfigStore::data().petals.size()) {
            const Petal &petal = ConfigStore::data().petals.at(hit.petal);
            const QString tip = QStringLiteral("%1\n%2")
                                    .arg(petal.name, QDir::toNativeSeparators(petal.path));
            if (tip != m_tooltipCache) {
                setToolTip(tip);
                m_tooltipCache = tip;
            }
        } else if (!m_tooltipCache.isEmpty() && m_statusTitle.isEmpty()) {
            setToolTip(QString());
            m_tooltipCache.clear();
        }
        update();
    }
    FloatingWindow::mouseMoveEvent(event);
}

void OrbWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const Hit hit = hitAt(event->position().toPoint());

        if (m_pressedAdd) {
            m_pressedAdd = false;
            update();
            if (hit.addBadge) {
                event->accept();
                addPetalInteractive();
                return;
            }
        }

        if (m_pressedPetal >= 0) {
            const int index = m_pressedPetal;
            m_pressedPetal = -1;
            if (hit.petal == index) {
                event->accept();
                if (!m_pending.isEmpty())
                    movePendingTo(index);
                else
                    openPetal(index);
                return;
            }
        }
    }
    FloatingWindow::mouseReleaseEvent(event);
}

void OrbWidget::enterEvent(QEnterEvent *event)
{
    update();
    FloatingWindow::enterEvent(event);
}

void OrbWidget::leaveEvent(QEvent *event)
{
    if (m_hoverPetal != -1 || m_hoverAdd) {
        m_hoverPetal = -1;
        m_hoverAdd = false;
        update();
    }
    FloatingWindow::leaveEvent(event);
}

void OrbWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        cancelPending();
        event->accept();
        return;
    }
    if (!m_pending.isEmpty()) {
        const int key = event->key();
        if (key >= Qt::Key_1 && key <= Qt::Key_8) {
            const int index = key - Qt::Key_1;
            if (index < ConfigStore::data().petals.size()) {
                movePendingTo(index);
                event->accept();
                return;
            }
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            const int last = ConfigStore::data().lastPetal;
            if (last >= 0 && last < ConfigStore::data().petals.size()) {
                movePendingTo(last);
                event->accept();
                return;
            }
        }
    }
    FloatingWindow::keyPressEvent(event);
}

// ---------------------------------------------------------------- drag and drop

void OrbWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    m_dragOver = true;
    if (!m_animTimer.isActive())
        m_animTimer.start();
    update();
}

void OrbWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void OrbWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
    m_dragOver = false;
    if (!m_busy)
        m_animTimer.stop();
    update();
}

void OrbWidget::dropEvent(QDropEvent *event)
{
    QStringList files;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            const QString local = url.toLocalFile();
            if (!local.isEmpty() && !files.contains(local))
                files << local;
        }
    }

    m_dragOver = false;
    if (!m_busy)
        m_animTimer.stop();

    if (files.isEmpty()) {
        event->ignore();
        update();
        return;
    }

    event->acceptProposedAction();
    const bool copyMode = event->modifiers().testFlag(Qt::ControlModifier)
                          || event->dropAction() == Qt::CopyAction;
    beginPending(files, copyMode);
    showPicker(event->position().toPoint());
}

// ---------------------------------------------------------------- pending job

void OrbWidget::beginPending(const QStringList &files, bool copyMode)
{
    m_pending = files;
    m_pendingCopy = copyMode;
    m_statusTimer.stop();
    clearStatus();
    update();
}

void OrbWidget::showPicker(const QPoint &pos)
{
    if (m_picker)
        m_picker->close();

    const AppConfig &cfg = ConfigStore::data();
    auto *picker = new PetalPicker(m_pending, cfg.petals, m_pendingCopy, this);
    picker->setAttribute(Qt::WA_DeleteOnClose);
    m_picker = picker;

    connect(picker, &PetalPicker::petalChosen, this, &OrbWidget::movePendingTo);
    connect(picker, &PetalPicker::addFolderRequested, this, [this]() {
        const int before = ConfigStore::data().petals.size();
        addPetalInteractive();
        if (ConfigStore::data().petals.size() > before && !m_pending.isEmpty())
            movePendingTo(ConfigStore::data().petals.size() - 1);
    });
    connect(picker, &PetalPicker::dismissed, this, [this]() { update(); });

    picker->popupAt(mapToGlobal(pos));
}

void OrbWidget::cancelPending()
{
    if (m_picker)
        m_picker->close();
    m_pending.clear();
    m_pendingCopy = false;
    update();
}

void OrbWidget::movePendingTo(int petalIndex)
{
    AppConfig &cfg = ConfigStore::data();
    if (m_pending.isEmpty() || petalIndex < 0 || petalIndex >= cfg.petals.size())
        return;

    if (TransferManager::instance().busy()) {
        setStatus(QStringLiteral("BUSY"), QStringLiteral("Wait for the running job"), true);
        return;
    }

    const Petal petal = cfg.petals.at(petalIndex);
    Transfer::Options options;
    options.mode = m_pendingCopy ? Transfer::Mode::Copy : Transfer::Mode::Move;

    const QStringList sources = m_pending;
    if (!TransferManager::instance().start(sources, petal.path, options)) {
        setStatus(QStringLiteral("BUSY"), QStringLiteral("Another move is running"), true);
        return;
    }

    cfg.lastPetal = petalIndex;
    ConfigStore::save();
    m_pending.clear();
    m_pendingCopy = false;
    if (m_picker)
        m_picker->close();
    update();
}

// ---------------------------------------------------------------- petals

void OrbWidget::openPetal(int index)
{
    AppConfig &cfg = ConfigStore::data();
    if (index < 0 || index >= cfg.petals.size())
        return;
    const QString path = cfg.petals.at(index).path;
    if (!QFileInfo(path).isDir() && !QDir().mkpath(path)) {
        setStatus(QStringLiteral("MISSING"),
                  QStringLiteral("That folder is gone"), true);
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void OrbWidget::addPetalInteractive()
{
    const QString startDir = QDir::homePath();
    const QString chosen = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose a destination folder for this petal"), startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (chosen.isEmpty())
        return;

    const QString clean = QDir::cleanPath(chosen);
    QString defaultName = QDir(clean).dirName();
    if (defaultName.isEmpty())
        defaultName = clean;

    QInputDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Name this petal"));
    dialog.setLabelText(QStringLiteral("Shown on the dial as:"));
    dialog.setTextValue(defaultName);
    dialog.setInputMode(QInputDialog::TextInput);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QString name = dialog.textValue().trimmed();
    if (name.isEmpty())
        name = defaultName;

    AppConfig &cfg = ConfigStore::data();
    for (const Petal &existing : cfg.petals) {
        if (QDir::cleanPath(existing.path) == clean) {
            setStatus(QStringLiteral("ALREADY"), QStringLiteral("That folder is on the dial"), true);
            return;
        }
    }
    cfg.petals.append(Petal{ name, clean });
    ConfigStore::save();
    setStatus(QStringLiteral("ADDED"), name, false);
    update();
}

void OrbWidget::renamePetal(int index)
{
    AppConfig &cfg = ConfigStore::data();
    if (index < 0 || index >= cfg.petals.size())
        return;

    QInputDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Rename petal"));
    dialog.setLabelText(QStringLiteral("New name:"));
    dialog.setTextValue(cfg.petals.at(index).name);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString name = dialog.textValue().trimmed();
    if (name.isEmpty())
        return;
    cfg.petals[index].name = name;
    ConfigStore::save();
    update();
}

void OrbWidget::removePetal(int index)
{
    AppConfig &cfg = ConfigStore::data();
    if (index < 0 || index >= cfg.petals.size())
        return;
    const QString name = cfg.petals.at(index).name;
    cfg.petals.removeAt(index);
    if (cfg.lastPetal >= cfg.petals.size())
        cfg.lastPetal = -1;
    ConfigStore::save();
    setStatus(QStringLiteral("REMOVED"), name, false);
    update();
}

void OrbWidget::managePetal(int index)
{
    openPetal(index);
}

// ---------------------------------------------------------------- menus and dialogs

void OrbWidget::buildMenu(QMenu &menu)
{
    AppConfig &cfg = ConfigStore::data();

    QAction *heading = menu.addAction(QStringLiteral("%1  %2").arg(Brand::productName(), Brand::version()));
    heading->setEnabled(false);
    QAction *owner = menu.addAction(Brand::ownerLine());
    owner->setEnabled(false);
    menu.addSeparator();

    QAction *add = menu.addAction(QIcon(QStringLiteral(":/assets/icons/icon_add.svg")),
                                  QStringLiteral("Add folder..."));
    connect(add, &QAction::triggered, this, &OrbWidget::addPetalInteractive);

    if (!cfg.petals.isEmpty()) {
        QMenu *manage = menu.addMenu(QStringLiteral("Folders"));
        for (int i = 0; i < cfg.petals.size(); ++i) {
            QMenu *sub = manage->addMenu(cfg.petals.at(i).name);
            QAction *open = sub->addAction(QStringLiteral("Open in Explorer"));
            QAction *rename = sub->addAction(QStringLiteral("Rename..."));
            sub->addSeparator();
            QAction *remove = sub->addAction(QStringLiteral("Remove from dial"));
            connect(open, &QAction::triggered, this, [this, i]() { openPetal(i); });
            connect(rename, &QAction::triggered, this, [this, i]() { renamePetal(i); });
            connect(remove, &QAction::triggered, this, [this, i]() { removePetal(i); });
        }
    }

    QAction *undo = menu.addAction(QStringLiteral("Undo last move"));
    undo->setEnabled(canUndo());
    connect(undo, &QAction::triggered, this, &OrbWidget::undoLastMove);

    if (m_busy) {
        QAction *show = menu.addAction(QStringLiteral("Show transfer window..."));
        connect(show, &QAction::triggered, this, &OrbWidget::showTransferWindow);
    }

    menu.addSeparator();

    QAction *whatsapp = menu.addAction(QStringLiteral("WhatsApp bubble"));
    whatsapp->setCheckable(true);
    whatsapp->setChecked(cfg.whatsAppVisible);
    connect(whatsapp, &QAction::toggled, this, [this](bool on) {
        AppConfig &c = ConfigStore::data();
        c.whatsAppVisible = on;
        ConfigStore::save();
        emit whatsAppToggled(on);
    });

    QAction *onTop = menu.addAction(QStringLiteral("Always on top"));
    onTop->setCheckable(true);
    onTop->setChecked(cfg.alwaysOnTop);
    connect(onTop, &QAction::toggled, this, [this](bool on) {
        AppConfig &c = ConfigStore::data();
        c.alwaysOnTop = on;
        ConfigStore::save();
        applyWindowFlags();
    });

    QAction *lock = menu.addAction(QStringLiteral("Lock position"));
    lock->setCheckable(true);
    lock->setChecked(cfg.lockPosition);
    connect(lock, &QAction::toggled, this, [this](bool on) {
        AppConfig &c = ConfigStore::data();
        c.lockPosition = on;
        ConfigStore::save();
        setResizable(!on);
        setDraggable(!on);
        update();
    });

    QAction *snap = menu.addAction(QStringLiteral("Snap to screen edges"));
    snap->setCheckable(true);
    snap->setChecked(cfg.snapToEdges);
    connect(snap, &QAction::toggled, this, [this](bool on) {
        AppConfig &c = ConfigStore::data();
        c.snapToEdges = on;
        ConfigStore::save();
        setSnapEnabled(on);
    });

    QAction *progressWindow = menu.addAction(QStringLiteral("Open progress window for jobs"));
    progressWindow->setCheckable(true);
    progressWindow->setChecked(cfg.showProgressWindow);
    connect(progressWindow, &QAction::toggled, this, [](bool on) {
        AppConfig &c = ConfigStore::data();
        c.showProgressWindow = on;
        ConfigStore::save();
    });

    QAction *autostart = menu.addAction(QStringLiteral("Start with Windows"));
    autostart->setCheckable(true);
    autostart->setChecked(Autostart::isEnabled());
    connect(autostart, &QAction::toggled, this, [](bool on) {
        Autostart::setEnabled(on);
        AppConfig &c = ConfigStore::data();
        c.startWithWindows = on;
        ConfigStore::save();
    });

    menu.addSeparator();
    QAction *logs = menu.addAction(QStringLiteral("Open log folder"));
    connect(logs, &QAction::triggered, this, &OrbWidget::openLogFolder);
    QAction *settings = menu.addAction(QStringLiteral("Open settings folder"));
    connect(settings, &QAction::triggered, this, &OrbWidget::openConfigFolder);
    QAction *about = menu.addAction(QStringLiteral("About AMS Sortie"));
    connect(about, &QAction::triggered, this, &OrbWidget::showAbout);

    menu.addSeparator();
    QAction *quit = menu.addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, this, &OrbWidget::quitRequested);
}

void OrbWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    buildMenu(menu);
    menu.exec(event->globalPos());
}

void OrbWidget::showAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void OrbWidget::showTransferWindow()
{
    emit transferWindowRequested();
}

void OrbWidget::openConfigFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(ConfigStore::folderPath()));
}

void OrbWidget::openLogFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(Log::folderPath()));
}

bool OrbWidget::canUndo() const
{
    const AppConfig &cfg = ConfigStore::data();
    return !TransferManager::instance().busy() && cfg.undo.valid && !cfg.undo.targets.isEmpty();
}

void OrbWidget::undoLastMove()
{
    AppConfig &cfg = ConfigStore::data();
    if (!canUndo())
        return;

    QString parent;
    for (const QString &source : cfg.undo.sources) {
        const QString dir = QFileInfo(source).absolutePath();
        if (parent.isEmpty())
            parent = dir;
        else if (parent != dir) {
            setStatus(QStringLiteral("NO UNDO"),
                      QStringLiteral("Items came from several folders"), true);
            return;
        }
    }
    if (parent.isEmpty())
        return;

    // Make sure every target still exists before promising an undo.
    QStringList existing;
    for (const QString &target : cfg.undo.targets) {
        if (QFileInfo::exists(target))
            existing << target;
    }
    if (existing.isEmpty()) {
        cfg.undo.valid = false;
        ConfigStore::save();
        setStatus(QStringLiteral("NO UNDO"), QStringLiteral("Nothing left to move back"), true);
        return;
    }

    Transfer::Options options;
    options.mode = Transfer::Mode::Move;
    if (TransferManager::instance().start(existing, parent, options)) {
        cfg.undo.valid = false;
        ConfigStore::save();
    }
}

// ---------------------------------------------------------------- transfer signals

void OrbWidget::onTransferStarted(const QStringList &sources, const QString &destination)
{
    m_busy = true;
    m_hasResult = false;
    m_progress = Transfer::Progress();
    m_progress.destination = destination;
    m_progress.itemCount = sources.size();
    m_progress.phase = Transfer::Progress::Scanning;
    m_pending.clear();
    m_statusTitle.clear();
    m_statusTimer.stop();
    if (!m_animTimer.isActive())
        m_animTimer.start();
    update();
}

void OrbWidget::onTransferProgress(const Transfer::Progress &progress)
{
    m_progress = progress;
    if (!m_busy) {
        m_busy = true;
        if (!m_animTimer.isActive())
            m_animTimer.start();
    }
    update();
}

void OrbWidget::onTransferFinished(const Transfer::Result &result)
{
    m_busy = false;
    m_lastResult = result;
    m_hasResult = true;
    if (!m_dragOver)
        m_animTimer.stop();

    if (result.cancelled)
        setStatus(QStringLiteral("CANCELLED"), result.summary, true);
    else if (result.itemsFailed > 0)
        setStatus(QStringLiteral("PARTIAL"), result.summary, true);
    else
        setStatus(QStringLiteral("DONE"), result.summary, false);

    if (!result.errors.isEmpty()) {
        QStringList lines;
        const int shown = qMin(6, int(result.errors.size()));
        for (int i = 0; i < shown; ++i) {
            const Transfer::Error &error = result.errors.at(i);
            lines << (error.path.isEmpty()
                          ? error.message
                          : QStringLiteral("%1: %2").arg(QFileInfo(error.path).fileName(),
                                                         error.message));
        }
        if (result.errors.size() > shown)
            lines << QStringLiteral("(+%1 more - see the log)").arg(result.errors.size() - shown);
        setToolTip(lines.join(QLatin1Char('\n')));
    }

    if (result.ok && !result.undoFrom.isEmpty()) {
        AppConfig &cfg = ConfigStore::data();
        cfg.undo.valid = true;
        cfg.undo.sources = result.undoFrom;
        cfg.undo.targets = result.undoTo;
        cfg.undo.destination = result.destination;
        ConfigStore::save();
    }
    update();
}
