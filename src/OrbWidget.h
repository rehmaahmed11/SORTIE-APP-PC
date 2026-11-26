#pragma once

#include <QElapsedTimer>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include "FloatingWindow.h"
#include "Transfer.h"

class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QMenu;
class QPainter;
class PetalPicker;

// The floating dial. It shows the brand, the petal ring, a live progress ring
// and the result of the last job. All heavy work happens in TransferManager.
class OrbWidget : public FloatingWindow
{
    Q_OBJECT
public:
    explicit OrbWidget(QWidget *parent = nullptr);
    ~OrbWidget() override;

    void buildMenu(QMenu &menu);
    bool canUndo() const;
    bool hasPending() const { return !m_pending.isEmpty(); }

public slots:
    void addPetalInteractive();
    void showAbout();
    void showTransferWindow();
    void openConfigFolder();
    void openLogFolder();
    void undoLastMove();
    void bringUp();
    void cancelPending();
    void applyWindowFlags();
    void savePlacement();

signals:
    void whatsAppToggled(bool visible);
    void transferWindowRequested();
    void quitRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Hit {
        int petal = -1;
        bool addBadge = false;
    };

    Hit hitAt(const QPoint &pos) const;
    int visiblePetalCount() const;
    QRectF bodyRect() const;
    qreal ringRadius() const;
    QPointF badgeCenter() const;

    void showPicker(const QPoint &globalPos);
    void beginPending(const QStringList &files, bool copyMode);
    void movePendingTo(int petalIndex);
    void openPetal(int index);
    void renamePetal(int index);
    void removePetal(int index);
    void managePetal(int index);
    void setStatus(const QString &title, const QString &detail, bool error);
    void clearStatus();
    void onTransferStarted(const QStringList &sources, const QString &destination);
    void onTransferProgress(const Transfer::Progress &progress);
    void onTransferFinished(const Transfer::Result &result);

    void paintIdle(QPainter &painter);
    void paintChoose(QPainter &painter);
    void paintBusy(QPainter &painter);
    void paintStatus(QPainter &painter);
    void paintPetals(QPainter &painter);
    void paintProgressRing(QPainter &painter);
    void paintAddBadge(QPainter &painter);

    QStringList m_pending;
    bool m_pendingCopy = false;
    QPoint m_pendingAt;
    QPointer<PetalPicker> m_picker;

    int m_hoverPetal = -1;
    bool m_hoverAdd = false;
    int m_pressedPetal = -1;
    bool m_pressedAdd = false;
    bool m_dragOver = false;

    bool m_busy = false;
    Transfer::Progress m_progress;
    Transfer::Result m_lastResult;
    bool m_hasResult = false;

    QString m_statusTitle;
    QString m_statusDetail;
    bool m_statusError = false;
    QTimer m_statusTimer;
    QTimer m_animTimer;
    qreal m_animPhase = 0.0;

    QString m_tooltipCache;
};
