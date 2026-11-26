#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

class QScreen;

// Shared behaviour for the two desktop ornaments (the dial and the WhatsApp
// bubble):
//   * drag anywhere, snap to the edges of the work area
//   * stretch from all four corners and edges (Windows-style sizing grips)
//   * anchor: the window remembers which screen edge it is glued to and follows
//     that edge when the resolution or the taskbar changes
//   * never allowed to end up off-screen
class FloatingWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingWindow(QWidget *parent = nullptr);

    void setSizeLimits(int minSide, int maxSide);
    void setDefaultSide(int side);
    void setResizable(bool on) { m_resizable = on; }
    bool isResizable() const { return m_resizable; }
    void setDraggable(bool on) { m_draggable = on; }
    bool isDraggable() const { return m_draggable; }
    void setSnapEnabled(bool on) { m_snapEnabled = on; }
    bool snapEnabled() const { return m_snapEnabled; }

    int sideLength() const { return width(); }
    void setSideLength(int side, bool notify = true);
    void nudgeSize(int delta);

    void setAnchorSpec(const QString &spec);
    QString anchorSpec() const { return m_anchor; }
    void refreshAnchor();

    void restorePlacement(const QRect &savedGeometry, bool hasGeometry, int preferredSize,
                          const QPoint &fallbackTopLeft);

    // True when (x, y) is close enough to a border that a stretch grip applies.
    bool isOnGrip(const QPoint &pos) const;

signals:
    void placementChanged();          // user finished moving/resizing
    void sideLengthChanged(int side); // emitted live while stretching

protected:
    enum Zone {
        ZoneNone = 0,
        ZoneLeft,
        ZoneRight,
        ZoneTop,
        ZoneBottom,
        ZoneTopLeft,
        ZoneTopRight,
        ZoneBottomLeft,
        ZoneBottomRight
    };

    Zone zoneAt(const QPoint &pos) const;
    void beginDrag(const QPoint &globalPos);
    void beginResize(const QPoint &globalPos, Zone zone);
    void continueDrag(const QPoint &globalPos);
    void continueResize(const QPoint &globalPos);
    void endInteraction();
    bool interacting() const { return m_zone != ZoneNone; }

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void watchScreens();
    void onScreenGeometryChanged();
    QScreen *screenFor(const QRect &rect) const;
    void clampInsideScreen();
    QRect snapped(const QRect &rect) const;

    bool m_resizable = true;
    bool m_draggable = true;
    bool m_snapEnabled = true;
    bool m_dragging = false;
    bool m_sizing = false;
    Zone m_zone = ZoneNone;
    QPoint m_pressGlobal;
    QRect m_pressGeometry;
    int m_minSide = 44;
    int m_maxSide = 600;
    int m_defaultSide = 190;
    QString m_anchor;
    int m_marginLeft = 0;
    int m_marginRight = 0;
    int m_marginTop = 0;
    int m_marginBottom = 0;
};
