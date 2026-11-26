#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "Config.h"

// The "where do these go?" chooser.
//
// v1 used a blocking QMenu::exec() with nested sub-menus. This is a lightweight
// popup that never blocks the caller: it reports the choice through a signal and
// closes itself, so the transfer can start immediately.
class PetalPicker : public QWidget
{
    Q_OBJECT
public:
    PetalPicker(const QStringList &items, const QList<Petal> &petals, bool copyMode,
                QWidget *parent = nullptr);

    void popupAt(const QPoint &globalPos);

signals:
    void petalChosen(int index);
    void addFolderRequested();
    void dismissed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    int rowCount() const;
    int addRowIndex() const { return int(m_petals.size()); }
    int rowAt(const QPoint &pos) const;
    QRect rowRect(int row) const;
    void choose(int row);
    void updateHover(int row);
    QString itemsLabel() const;

    QStringList m_items;
    QList<Petal> m_petals;
    bool m_copyMode = false;
    int m_hover = -1;
    int m_pressed = -1;
    bool m_dismissed = false;
};
