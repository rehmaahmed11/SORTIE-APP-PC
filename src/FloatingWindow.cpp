#include "FloatingWindow.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QScreen>
#include <QWheelEvent>

namespace {
constexpr int kGripPx = 16;     // thickness of the invisible stretch border
constexpr int kSnapPx = 18;     // magnet distance to a work-area edge
constexpr int kAnchorPx = 48;   // distance that counts as "anchored"
}

FloatingWindow::FloatingWindow(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
    watchScreens();
}

void FloatingWindow::watchScreens()
{
    for (QScreen *screen : QGuiApplication::screens()) {
        connect(screen, &QScreen::availableGeometryChanged, this,
                [this](const QRect &) { onScreenGeometryChanged(); });
        connect(screen, &QScreen::geometryChanged, this,
                [this](const QRect &) { onScreenGeometryChanged(); });
    }
    if (auto *app = qGuiApp) {
        connect(app, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
            connect(screen, &QScreen::availableGeometryChanged, this,
                    [this](const QRect &) { onScreenGeometryChanged(); });
            connect(screen, &QScreen::geometryChanged, this,
                    [this](const QRect &) { onScreenGeometryChanged(); });
        });
        connect(app, &QGuiApplication::screenRemoved, this,
                [this](QScreen *) { onScreenGeometryChanged(); });
        connect(app, &QGuiApplication::primaryScreenChanged, this,
                [this](QScreen *) { onScreenGeometryChanged(); });
    }
}

QScreen *FloatingWindow::screenFor(const QRect &rect) const
{
    if (QScreen *screen = QGuiApplication::screenAt(rect.center()))
        return screen;
    if (QScreen *screen = QGuiApplication::screenAt(rect.topLeft()))
        return screen;
    // Overlap test, so a window half off-screen still resolves to the right screen.
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->availableGeometry().intersects(rect))
            return screen;
    }
    return QGuiApplication::primaryScreen();
}

void FloatingWindow::setSizeLimits(int minSide, int maxSide)
{
    m_minSide = qMax(24, minSide);
    m_maxSide = qMax(m_minSide, maxSide);
}

void FloatingWindow::setDefaultSide(int side)
{
    m_defaultSide = qBound(m_minSide, side, m_maxSide);
}

void FloatingWindow::setSideLength(int side, bool notify)
{
    const int bounded = qBound(m_minSide, side, m_maxSide);
    if (bounded == width() && bounded == height())
        return;
    setFixedSize(bounded, bounded);
    if (notify)
        emit sideLengthChanged(bounded);
    update();
}

void FloatingWindow::nudgeSize(int delta)
{
    int next = qBound(m_minSide, width() + delta, m_maxSide);
    if (next == width())
        return;

    // Grow/shrink around the middle so the window does not walk away.
    const QPoint center = frameGeometry().center();
    setSideLength(next);
    move(center - QPoint(next / 2, next / 2));
    refreshAnchor();
    emit placementChanged();
}

// ---------------------------------------------------------------- placement

void FloatingWindow::restorePlacement(const QRect &savedGeometry, bool hasGeometry,
                                      int preferredSize, const QPoint &fallbackTopLeft)
{
    const int side = qBound(m_minSide, preferredSize > 0 ? preferredSize : m_defaultSide, m_maxSide);
    setSideLength(side, false);

    if (hasGeometry && savedGeometry.isValid()) {
        QRect candidate(savedGeometry.topLeft(), QSize(side, side));
        if (QGuiApplication::screenAt(candidate.center())) {
            setGeometry(candidate);
            clampInsideScreen();
            refreshAnchor();
            return;
        }
    }

    QRect candidate(fallbackTopLeft, QSize(side, side));
    if (const QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect area = screen->availableGeometry();
        if (!area.contains(candidate))
            candidate.moveTopLeft(QPoint(area.right() - side - 48, area.bottom() - side - 72));
    }
    setGeometry(candidate);
    clampInsideScreen();
    refreshAnchor();
}

void FloatingWindow::setAnchorSpec(const QString &spec)
{
    m_anchor = spec;
    refreshAnchor();
}

void FloatingWindow::refreshAnchor()
{
    QScreen *screen = screenFor(frameGeometry());
    if (!screen)
        return;
    const QRect area = screen->availableGeometry();
    const QRect g = frameGeometry();

    m_marginLeft = g.left() - area.left();
    m_marginRight = area.right() - g.right();
    m_marginTop = g.top() - area.top();
    m_marginBottom = area.bottom() - g.bottom();

    // Only remember the edges the window is actually sitting against.
    m_anchor.clear();
    if (m_marginLeft <= kAnchorPx && m_marginLeft <= m_marginRight)
        m_anchor += QLatin1Char('l');
    else if (m_marginRight <= kAnchorPx)
        m_anchor += QLatin1Char('r');
    if (m_marginTop <= kAnchorPx && m_marginTop <= m_marginBottom)
        m_anchor += QLatin1Char('t');
    else if (m_marginBottom <= kAnchorPx)
        m_anchor += QLatin1Char('b');
}

void FloatingWindow::onScreenGeometryChanged()
{
    QScreen *screen = screenFor(frameGeometry());
    if (!screen) {
        clampInsideScreen();
        return;
    }
    const QRect area = screen->availableGeometry();
    if (!area.isValid() || area.width() < 50)
        return;

    if (m_anchor.isEmpty()) {
        clampInsideScreen();
        return;
    }

    QRect g = frameGeometry();
    if (m_anchor.contains(QLatin1Char('l')))
        g.moveLeft(area.left() + m_marginLeft);
    if (m_anchor.contains(QLatin1Char('r')))
        g.moveRight(area.right() - m_marginRight);
    if (m_anchor.contains(QLatin1Char('t')))
        g.moveTop(area.top() + m_marginTop);
    if (m_anchor.contains(QLatin1Char('b')))
        g.moveBottom(area.bottom() - m_marginBottom);

    setGeometry(g);
    clampInsideScreen();
}

void FloatingWindow::clampInsideScreen()
{
    QRect g = frameGeometry();
    QScreen *screen = screenFor(g);
    if (!screen)
        return;
    const QRect area = screen->availableGeometry();
    if (!area.isValid())
        return;

    if (g.width() > area.width())
        setSideLength(area.width(), true);

    g = frameGeometry();
    if (g.right() > area.right())
        g.moveRight(area.right());
    if (g.bottom() > area.bottom())
        g.moveBottom(area.bottom());
    if (g.left() < area.left())
        g.moveLeft(area.left());
    if (g.top() < area.top())
        g.moveTop(area.top());
    setGeometry(g);
}

QRect FloatingWindow::snapped(const QRect &rect) const
{
    if (!m_snapEnabled)
        return rect;

    QScreen *screen = screenFor(rect);
    if (!screen)
        return rect;
    const QRect area = screen->availableGeometry();
    QRect result = rect;

    if (qAbs(result.left() - area.left()) <= kSnapPx)
        result.moveLeft(area.left());
    else if (qAbs(result.right() - area.right()) <= kSnapPx)
        result.moveRight(area.right());

    if (qAbs(result.top() - area.top()) <= kSnapPx)
        result.moveTop(area.top());
    else if (qAbs(result.bottom() - area.bottom()) <= kSnapPx)
        result.moveBottom(area.bottom());

    return result;
}

// ---------------------------------------------------------------- grips

bool FloatingWindow::isOnGrip(const QPoint &pos) const
{
    if (!m_resizable)
        return false;
    return zoneAt(pos) != ZoneNone;
}

FloatingWindow::Zone FloatingWindow::zoneAt(const QPoint &pos) const
{
    if (!m_resizable)
        return ZoneNone;

    const QRect r = rect();
    const bool left = pos.x() <= kGripPx;
    const bool right = pos.x() >= r.width() - kGripPx;
    const bool top = pos.y() <= kGripPx;
    const bool bottom = pos.y() >= r.height() - kGripPx;

    if (left && top) return ZoneTopLeft;
    if (right && top) return ZoneTopRight;
    if (left && bottom) return ZoneBottomLeft;
    if (right && bottom) return ZoneBottomRight;
    if (left) return ZoneLeft;
    if (right) return ZoneRight;
    if (top) return ZoneTop;
    if (bottom) return ZoneBottom;
    return ZoneNone;
}

// ---------------------------------------------------------------- interaction

void FloatingWindow::beginDrag(const QPoint &globalPos)
{
    m_dragging = true;
    m_pressGlobal = globalPos;
    m_pressGeometry = frameGeometry();
}

void FloatingWindow::beginResize(const QPoint &globalPos, Zone zone)
{
    m_sizing = true;
    m_zone = zone;
    m_pressGlobal = globalPos;
    m_pressGeometry = frameGeometry();
}

void FloatingWindow::continueDrag(const QPoint &globalPos)
{
    if (!m_dragging)
        return;
    const QPoint delta = globalPos - m_pressGlobal;
    QRect target = m_pressGeometry.translated(delta);
    target = snapped(target);
    move(target.topLeft());
}

void FloatingWindow::continueResize(const QPoint &globalPos)
{
    if (!m_sizing)
        return;

    const int dx = globalPos.x() - m_pressGlobal.x();
    const int dy = globalPos.y() - m_pressGlobal.y();
    const int startSide = m_pressGeometry.width();

    int delta = 0;
    switch (m_zone) {
    case ZoneLeft: delta = -dx; break;
    case ZoneRight: delta = dx; break;
    case ZoneTop: delta = -dy; break;
    case ZoneBottom: delta = dy; break;
    case ZoneTopLeft: delta = qMax(-dx, -dy); break;
    case ZoneTopRight: delta = qMax(dx, -dy); break;
    case ZoneBottomLeft: delta = qMax(-dx, dy); break;
    case ZoneBottomRight: delta = qMax(dx, dy); break;
    default: return;
    }

    const int next = qBound(m_minSide, startSide + delta, m_maxSide);
    if (next == width())
        return;

    QRect g = m_pressGeometry;
    const bool fromLeft = (m_zone == ZoneLeft || m_zone == ZoneTopLeft || m_zone == ZoneBottomLeft);
    const bool fromTop = (m_zone == ZoneTop || m_zone == ZoneTopLeft || m_zone == ZoneTopRight);

    if (fromLeft)
        g.setLeft(g.right() - next + 1);
    else
        g.setRight(g.left() + next - 1);
    if (fromTop)
        g.setTop(g.bottom() - next + 1);
    else
        g.setBottom(g.top() + next - 1);

    setSideLength(next);
    move(g.topLeft());
}

void FloatingWindow::endInteraction()
{
    if (!m_dragging && !m_sizing)
        return;
    const bool moved = m_dragging;
    m_dragging = false;
    m_sizing = false;
    m_zone = ZoneNone;

    if (moved)
        move(snapped(frameGeometry()).topLeft());
    clampInsideScreen();
    refreshAnchor();
    unsetCursor();
    emit placementChanged();
}

void FloatingWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const QPoint pos = event->position().toPoint();
    const QPoint global = event->globalPosition().toPoint();
    const Zone zone = zoneAt(pos);
    if (zone != ZoneNone) {
        beginResize(global, zone);
        event->accept();
        return;
    }
    if (!m_draggable) {
        QWidget::mousePressEvent(event);
        return;
    }
    beginDrag(global);
    event->accept();
}

void FloatingWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const QPoint global = event->globalPosition().toPoint();

    if (m_sizing) {
        continueResize(global);
        event->accept();
        return;
    }
    if (m_dragging) {
        continueDrag(global);
        event->accept();
        return;
    }

    switch (zoneAt(pos)) {
    case ZoneTopLeft:
    case ZoneBottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case ZoneTopRight:
    case ZoneBottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case ZoneLeft:
    case ZoneRight:
        setCursor(Qt::SizeHorCursor);
        break;
    case ZoneTop:
    case ZoneBottom:
        setCursor(Qt::SizeVerCursor);
        break;
    default:
        unsetCursor();
        break;
    }
    QWidget::mouseMoveEvent(event);
}

void FloatingWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        endInteraction();
    QWidget::mouseReleaseEvent(event);
}

void FloatingWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    // Double-click on a stretch grip restores the default size.
    if (m_resizable && zoneAt(event->position().toPoint()) != ZoneNone) {
        nudgeSize(m_defaultSide - width());
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void FloatingWindow::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps != 0) {
        const int step = event->modifiers().testFlag(Qt::ControlModifier) ? 24 : 10;
        nudgeSize(steps * step);
    }
    event->accept();
}

void FloatingWindow::leaveEvent(QEvent *event)
{
    if (!m_dragging && !m_sizing)
        unsetCursor();
    QWidget::leaveEvent(event);
}
