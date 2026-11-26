#pragma once

#include <QDialog>
#include <QString>

#include "Transfer.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QGridLayout;

// The "like Windows" transfer window: a real progress bar, transferred size vs
// total size, speed, time left, file counts and a working Cancel button.
class TransferDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TransferDialog(QWidget *parent = nullptr);

    void begin(const QStringList &sources, const QString &destination, bool copyMode);
    void updateProgress(const Transfer::Progress &progress);
    void finish(const Transfer::Result &result);
    void present();

signals:
    void cancelRequested();
    void openDestinationRequested(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void buildUi();
    QLabel *addRow(QGridLayout *grid, int row, const QString &caption);
    void setElided(QLabel *label, const QString &text);
    void refreshElided();
    void applyRunningState(bool running);
    void storeFullText(QLabel *label, const QString &text);

    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_destinationLabel = nullptr;
    QLabel *m_barCaption = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_progressValue = nullptr;
    QLabel *m_totalValue = nullptr;
    QLabel *m_transferredValue = nullptr;
    QLabel *m_speedValue = nullptr;
    QLabel *m_etaValue = nullptr;
    QLabel *m_filesValue = nullptr;
    QLabel *m_currentValue = nullptr;
    QLabel *m_footerLabel = nullptr;

    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_backgroundButton = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    QString m_fullDestination;
    QString m_fullCurrent;
    QString m_destinationPath;
    QString m_itemsLabel;
    bool m_running = false;
    bool m_copyMode = false;
    int m_itemCount = 0;
    Transfer::Result m_lastResult;
    bool m_hasResult = false;
    bool m_placedOnce = false;
};
