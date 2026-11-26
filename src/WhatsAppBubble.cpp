#include "WhatsAppBubble.h"

#include "Brand.h"
#include "Config.h"
#include "Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QScreen>
#include <QSvgRenderer>
#include <QToolTip>
#include <QUrl>
#include <QtMath>

WhatsAppBubble::WhatsAppBubble(QWidget *parent) : FloatingWindow(parent)
{
    setWindowTitle(QStringLiteral("WhatsApp - %1").arg(Brand::author()));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app.ico")));
    setSizeLimits(ConfigStore::kMinBubbleSize, ConfigStore::kMaxBubbleSize);
    setDefaultSide(ConfigStore::kDefaultBubbleSize);

    AppConfig &cfg = ConfigStore::data();
    cfg.whatsAppSize = qBound(ConfigStore::kMinBubbleSize, cfg.whatsAppSize,
                              ConfigStore::kMaxBubbleSize);
    setSnapEnabled(cfg.snapToEdges);
    setResizable(true);
    applyWindowFlags();

    const QRect area = QGuiApplication::primaryScreen()
                           ? QGuiApplication::primaryScreen()->availableGeometry()
                           : QRect(0, 0, 1280, 720);
    ConfigStore::applyDefaultsForScreen(area);
    restorePlacement(QRect(cfg.whatsAppPos, QSize(cfg.whatsAppSize, cfg.whatsAppSize)),
                     cfg.hasWhatsAppPos, cfg.whatsAppSize,
                     QPoint(area.left() + 32, area.bottom() - cfg.whatsAppSize - 48));
    setAnchorSpec(cfg.whatsAppAnchor);

    setToolTip(QStringLiteral("Chat with %1 on WhatsApp\n%2")
                   .arg(Brand::author(), Brand::phoneDisplay()));

    m_pulseTimer.setInterval(50);
    connect(&m_pulseTimer, &QTimer::timeout, this, [this]() {
        m_pulse += 0.022;
        if (m_pulse > 1.0)
            m_pulse -= 1.0;
        update();
    });

    connect(this, &FloatingWindow::placementChanged, this, [this]() {
        AppConfig &c = ConfigStore::data();
        c.whatsAppPos = pos();
        c.whatsAppSize = width();
        c.hasWhatsAppPos = true;
        c.whatsAppAnchor = anchorSpec();
        ConfigStore::save();
    });
    connect(this, &FloatingWindow::sideLengthChanged, this, [this](int side) {
        AppConfig &c = ConfigStore::data();
        c.whatsAppSize = side;
        ConfigStore::save();
        update();
    });

    m_pulseTimer.start();
}

void WhatsAppBubble::applyWindowFlags()
{
    AppConfig &cfg = ConfigStore::data();
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint;
    if (cfg.alwaysOnTop)
        flags |= Qt::WindowStaysOnTopHint;
    const bool wasVisible = isVisible();
    setWindowFlags(flags);
    if (wasVisible) {
        show();
        raise();
    }
}

void WhatsAppBubble::paintEmblem(QPainter &painter, const QPointF &center, qreal size)
{
    const int pixel = int(qMax(16.0, size));
    const qreal dpr = devicePixelRatioF();
    if (m_emblem.isNull() || m_emblemSide != pixel) {
        QSvgRenderer renderer(QStringLiteral(":/assets/icons/whatsapp.svg"));
        QPixmap pixmap(int(pixel * dpr), int(pixel * dpr));
        pixmap.fill(Qt::transparent);
        if (renderer.isValid()) {
            QPainter emblemPainter(&pixmap);
            emblemPainter.setRenderHint(QPainter::Antialiasing);
            renderer.render(&emblemPainter, QRectF(0, 0, pixel * dpr, pixel * dpr));
        }
        pixmap.setDevicePixelRatio(dpr);
        m_emblem = pixmap;
        m_emblemSide = pixel;
    }
    painter.drawPixmap(QPointF(center.x() - size / 2.0, center.y() - size / 2.0), m_emblem);
}

void WhatsAppBubble::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const qreal side = qMin(width(), height());
    const qreal inset = side * 0.055;
    const QRectF circleRect(inset, inset, side - inset * 2, side - inset * 2);
    const QPointF center = circleRect.center();
    const qreal radius = circleRect.width() / 2.0;

    // Pulsing halo
    if (!m_hover) {
        const qreal haloRadius = radius + side * (0.035 + 0.10 * m_pulse);
        QColor halo = Theme::whatsapp();
        halo.setAlpha(int(120 * (1.0 - m_pulse)));
        painter.setPen(QPen(halo, qMax(1.5, side * 0.02)));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, haloRadius, haloRadius);
    }

    // Soft shadow / glow
    {
        QRadialGradient glow(center, radius * 1.35);
        QColor mid = Theme::whatsappDeep();
        mid.setAlpha(m_hover ? 130 : 80);
        QColor edge = Theme::whatsappDeep();
        edge.setAlpha(0);
        glow.setColorAt(0.60, mid);
        glow.setColorAt(1.0, edge);
        painter.setPen(Qt::NoPen);
        painter.setBrush(glow);
        painter.drawEllipse(center, radius * 1.30, radius * 1.30);
    }

    paintEmblem(painter, center, circleRect.width());

    // Hover / stretch affordance ring
    QColor ringColor(0xFF, 0xFF, 0xFF, m_hover ? 150 : 60);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(ringColor, qMax(1.0, side * 0.012)));
    painter.drawEllipse(circleRect.adjusted(-1, -1, 1, 1));

    if (m_hover && side >= 70) {
        // Four little stretch ticks so the grip is discoverable.
        painter.setPen(QPen(QColor(255, 255, 255, 120), 1.4, Qt::SolidLine, Qt::RoundCap));
        const qreal tick = side * 0.055;
        const qreal offset = radius + side * 0.045;
        painter.drawLine(QPointF(center.x() - tick, center.y() - offset),
                         QPointF(center.x() + tick, center.y() - offset));
        painter.drawLine(QPointF(center.x() - tick, center.y() + offset),
                         QPointF(center.x() + tick, center.y() + offset));
        painter.drawLine(QPointF(center.x() - offset, center.y() - tick),
                         QPointF(center.x() - offset, center.y() + tick));
        painter.drawLine(QPointF(center.x() + offset, center.y() - tick),
                         QPointF(center.x() + offset, center.y() + tick));
    }
}

void WhatsAppBubble::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressGlobal = event->globalPosition().toPoint();
        m_moved = false;
    }
    FloatingWindow::mousePressEvent(event);
}

void WhatsAppBubble::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPoint now = event->globalPosition().toPoint();
        if (!m_moved && (now - m_pressGlobal).manhattanLength() < 6 && !isOnGrip(event->position().toPoint())) {
            openChat();
            event->accept();
            return;
        }
    }
    FloatingWindow::mouseReleaseEvent(event);
}

void WhatsAppBubble::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->globalPosition().toPoint() - m_pressGlobal).manhattanLength() > 4)
        m_moved = true;
    FloatingWindow::mouseMoveEvent(event);
}

void WhatsAppBubble::enterEvent(QEnterEvent *event)
{
    m_hover = true;
    update();
    FloatingWindow::enterEvent(event);
}

void WhatsAppBubble::leaveEvent(QEvent *event)
{
    m_hover = false;
    update();
    FloatingWindow::leaveEvent(event);
}

void WhatsAppBubble::openChat()
{
    QDesktopServices::openUrl(QUrl(Brand::whatsAppLink(
        QStringLiteral("Hi Rehman, I am using %1 %2")
            .arg(Brand::productName(), Brand::version()))));
}

void WhatsAppBubble::openCall()
{
    QDesktopServices::openUrl(QUrl(Brand::callLink()));
}

void WhatsAppBubble::copyNumber()
{
    QApplication::clipboard()->setText(Brand::phoneDisplay());
}

void WhatsAppBubble::setSizePreset(int side)
{
    const QPoint center = frameGeometry().center();
    setSideLength(side);
    move(center - QPoint(width() / 2, height() / 2));
    refreshAnchor();
    update();
}

void WhatsAppBubble::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.setStyleSheet(Theme::appStyle());

    QAction *heading = menu.addAction(QStringLiteral("WhatsApp - %1").arg(Brand::author()));
    heading->setEnabled(false);
    QAction *number = menu.addAction(Brand::phoneDisplay());
    number->setEnabled(false);
    menu.addSeparator();

    QAction *chat = menu.addAction(QStringLiteral("Chat on WhatsApp"));
    connect(chat, &QAction::triggered, this, &WhatsAppBubble::openChat);
    QAction *call = menu.addAction(QStringLiteral("Call %1").arg(Brand::phoneDisplay()));
    connect(call, &QAction::triggered, this, &WhatsAppBubble::openCall);
    QAction *copy = menu.addAction(QStringLiteral("Copy number"));
    connect(copy, &QAction::triggered, this, &WhatsAppBubble::copyNumber);

    menu.addSeparator();
    QMenu *sizeMenu = menu.addMenu(QStringLiteral("Icon size"));
    struct Preset { const char *label; int side; };
    const Preset presets[] = {
        { "Small (48 px)", 48 }, { "Medium (66 px)", 66 }, { "Large (96 px)", 96 },
        { "Extra large (128 px)", 128 }
    };
    for (const Preset &preset : presets) {
        QAction *action = sizeMenu->addAction(QString::fromLatin1(preset.label));
        connect(action, &QAction::triggered, this, [this, preset]() {
            setSizePreset(preset.side);
        });
    }

    menu.addSeparator();
    QAction *hide = menu.addAction(QStringLiteral("Hide this bubble"));
    connect(hide, &QAction::triggered, this, [this]() {
        hide();
        emit hideRequested();
    });

    menu.exec(event->globalPos());
}
