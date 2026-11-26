#pragma once

#include <QPixmap>
#include <QPoint>
#include <QTimer>

#include "FloatingWindow.h"

// The floating WhatsApp contact icon.
//
// It is stretchable (corner/edge grips, mouse wheel, double-click to reset),
// snaps and anchors to the screen edges like the dial, and opens a chat with
// Rehman Ahmed when clicked.
class WhatsAppBubble : public FloatingWindow
{
    Q_OBJECT
public:
    explicit WhatsAppBubble(QWidget *parent = nullptr);

    void applyWindowFlags();

signals:
    void hideRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void openChat();
    void openCall();
    void copyNumber();
    void setSizePreset(int side);
    void paintEmblem(QPainter &painter, const QPointF &center, qreal size);

    QPixmap m_emblem;
    int m_emblemSide = 0;
    qreal m_pulse = 0.0;
    QTimer m_pulseTimer;
    QPoint m_pressGlobal;
    bool m_moved = false;
    bool m_hover = false;
};
