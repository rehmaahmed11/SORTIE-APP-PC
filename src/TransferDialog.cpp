#include "TransferDialog.h"

#include "Brand.h"
#include "Format.h"
#include "Theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr char kFullText[] = "fullText";

QString itemLabel(const QStringList &sources)
{
    if (sources.size() == 1) {
        const QString name = QFileInfo(sources.first()).fileName();
        return name.isEmpty() ? QStringLiteral("1 item") : name;
    }
    return QStringLiteral("%1 items").arg(sources.size());
}
} // namespace

TransferDialog::TransferDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("%1 - transferring").arg(Brand::productName()));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app.ico")));
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                   | Qt::WindowMinMaxButtonsHint);
    setMinimumWidth(560);
    setModal(false);
    setStyleSheet(Theme::appStyle());
    buildUi();
    resize(620, 430);
}

void TransferDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(16);

    // ---- header
    auto *header = new QHBoxLayout;
    header->setSpacing(14);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setPixmap(QIcon(QStringLiteral(":/assets/icons/icon_move.svg")).pixmap(40, 40));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    header->addWidget(m_iconLabel, 0, Qt::AlignTop);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(3);
    m_titleLabel = new QLabel(QStringLiteral("Preparing..."), this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setFont(Theme::font(19, true));
    m_destinationLabel = new QLabel(this);
    m_destinationLabel->setObjectName(QStringLiteral("dimLabel"));
    m_destinationLabel->setFont(Theme::font(12));
    titles->addWidget(m_titleLabel);
    titles->addWidget(m_destinationLabel);
    header->addLayout(titles, 1);
    root->addLayout(header);

    // ---- progress
    auto *progressBox = new QVBoxLayout;
    progressBox->setSpacing(7);

    auto *progressHeader = new QHBoxLayout;
    m_barCaption = new QLabel(QStringLiteral("TRANSFERRED"), this);
    m_barCaption->setObjectName(QStringLiteral("faintLabel"));
    m_barCaption->setFont(Theme::font(10, true, 1.2));
    m_progressValue = new QLabel(QStringLiteral("0%"), this);
    m_progressValue->setObjectName(QStringLiteral("valueLabel"));
    m_progressValue->setFont(Theme::font(12, true));
    m_progressValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    progressHeader->addWidget(m_barCaption, 0);
    progressHeader->addStretch(1);
    progressHeader->addWidget(m_progressValue, 0);
    progressBox->addLayout(progressHeader);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 1000);
    m_bar->setValue(0);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(22);
    m_bar->setStyleSheet(Theme::progressStyle());
    progressBox->addWidget(m_bar);
    root->addLayout(progressBox);

    // ---- statistics
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(9);
    grid->setColumnStretch(1, 1);

    m_totalValue = addRow(grid, 0, QStringLiteral("Total size"));
    m_transferredValue = addRow(grid, 1, QStringLiteral("Transferred"));
    m_speedValue = addRow(grid, 2, QStringLiteral("Speed"));
    m_etaValue = addRow(grid, 3, QStringLiteral("Time left"));
    m_filesValue = addRow(grid, 4, QStringLiteral("Files"));
    m_currentValue = addRow(grid, 5, QStringLiteral("Current item"));

    root->addLayout(grid);
    root->addStretch(1);

    // ---- footer / buttons
    m_footerLabel = new QLabel(this);
    m_footerLabel->setObjectName(QStringLiteral("faintLabel"));
    m_footerLabel->setFont(Theme::font(11));
    m_footerLabel->setWordWrap(true);
    root->addWidget(m_footerLabel);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);

    m_openButton = new QPushButton(QStringLiteral("Open destination"), this);
    m_openButton->setCursor(Qt::PointingHandCursor);
    m_openButton->hide();
    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        if (!m_destinationPath.isEmpty())
            emit openDestinationRequested(m_destinationPath);
    });

    m_backgroundButton = new QPushButton(QStringLiteral("Run in background"), this);
    m_backgroundButton->setCursor(Qt::PointingHandCursor);
    connect(m_backgroundButton, &QPushButton::clicked, this, &QWidget::hide);

    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancelButton->setObjectName(QStringLiteral("dangerButton"));
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        m_cancelButton->setEnabled(false);
        m_cancelButton->setText(QStringLiteral("Cancelling..."));
        m_footerLabel->setText(QStringLiteral("Cancelling - the current file is finished or rolled "
                                              "back, then the job stops."));
        emit cancelRequested();
    });

    m_closeButton = new QPushButton(QStringLiteral("Close"), this);
    m_closeButton->setObjectName(QStringLiteral("primaryButton"));
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->hide();
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::hide);

    buttons->addWidget(m_openButton, 0);
    buttons->addStretch(1);
    buttons->addWidget(m_backgroundButton, 0);
    buttons->addWidget(m_cancelButton, 0);
    buttons->addWidget(m_closeButton, 0);
    root->addLayout(buttons);
}

QLabel *TransferDialog::addRow(QGridLayout *grid, int row, const QString &caption)
{
    auto *captionLabel = new QLabel(caption, this);
    captionLabel->setObjectName(QStringLiteral("dimLabel"));
    captionLabel->setFont(Theme::font(12));

    auto *valueLabel = new QLabel(QStringLiteral("--"), this);
    valueLabel->setObjectName(QStringLiteral("valueLabel"));
    valueLabel->setFont(Theme::font(12, true));
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    grid->addWidget(captionLabel, row, 0, Qt::AlignLeft | Qt::AlignVCenter);
    grid->addWidget(valueLabel, row, 1, Qt::AlignRight | Qt::AlignVCenter);
    return valueLabel;
}

void TransferDialog::storeFullText(QLabel *label, const QString &text)
{
    label->setProperty(kFullText, text);
    setElided(label, text);
}

void TransferDialog::setElided(QLabel *label, const QString &text)
{
    const int available = qMax(80, label->width() - 6);
    label->setText(label->fontMetrics().elidedText(text, Qt::ElideMiddle, available));
    label->setToolTip(text);
}

void TransferDialog::refreshElided()
{
    if (m_destinationLabel)
        setElided(m_destinationLabel, m_destinationLabel->property(kFullText).toString());
    if (m_currentValue)
        setElided(m_currentValue, m_currentValue->property(kFullText).toString());
    if (m_totalValue)
        setElided(m_totalValue, m_totalValue->property(kFullText).toString());
}

void TransferDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    refreshElided();
}

void TransferDialog::applyRunningState(bool running)
{
    m_running = running;
    m_cancelButton->setVisible(running);
    m_cancelButton->setEnabled(running);
    m_cancelButton->setText(QStringLiteral("Cancel"));
    m_backgroundButton->setVisible(running);
    m_closeButton->setVisible(!running);
    m_openButton->setVisible(!running);
}

void TransferDialog::begin(const QStringList &sources, const QString &destination, bool copyMode)
{
    m_copyMode = copyMode;
    m_itemCount = sources.size();
    m_itemsLabel = itemLabel(sources);
    m_destinationPath = destination;
    m_hasResult = false;
    m_lastResult = Transfer::Result();

    setWindowTitle(QStringLiteral("%1 - %2").arg(Brand::productName(),
                                                  copyMode ? QStringLiteral("copying")
                                                           : QStringLiteral("moving")));
    m_titleLabel->setText(QStringLiteral("%1 %2")
                              .arg(copyMode ? QStringLiteral("Copying") : QStringLiteral("Moving"),
                                   m_itemsLabel));

    m_fullDestination = QStringLiteral("to  %1").arg(QDir::toNativeSeparators(destination));
    storeFullText(m_destinationLabel, m_fullDestination);

    m_bar->setRange(0, 0);          // indeterminate while the job is measured
    m_bar->setValue(0);
    m_progressValue->setText(QStringLiteral("--"));
    m_totalValue->setText(QStringLiteral("measuring..."));
    m_transferredValue->setText(QStringLiteral("--"));
    m_speedValue->setText(QStringLiteral("--"));
    m_etaValue->setText(QStringLiteral("--"));
    m_filesValue->setText(m_itemCount == 1 ? QStringLiteral("1 item")
                                           : QStringLiteral("%1 items").arg(m_itemCount));
    m_fullCurrent.clear();
    m_currentValue->setText(QStringLiteral("--"));
    m_currentValue->setToolTip(QString());
    m_footerLabel->setText(copyMode
                               ? QStringLiteral("Copying: the originals stay where they are.")
                               : QStringLiteral("Nothing is deleted until the copy is verified."));
    applyRunningState(true);
}

void TransferDialog::updateProgress(const Transfer::Progress &progress)
{
    if (!m_running)
        applyRunningState(true);

    if (progress.bytesTotal > 0) {
        if (m_bar->maximum() != 1000)
            m_bar->setRange(0, 1000);
        m_bar->setValue(int(progress.fraction() * 1000.0));
        m_progressValue->setText(QStringLiteral("%1%").arg(int(progress.fraction() * 100.0)));
        m_totalValue->setText(Format::bytes(progress.bytesTotal));
        m_transferredValue->setText(QStringLiteral("%1 of %2")
                                        .arg(Format::bytes(progress.bytesDone),
                                             Format::bytes(progress.bytesTotal)));
    } else {
        if (m_bar->maximum() != 0)
            m_bar->setRange(0, 0);
        m_progressValue->setText(QStringLiteral("measuring"));
        m_totalValue->setText(QStringLiteral("measuring..."));
        m_transferredValue->setText(QStringLiteral("%1 so far")
                                        .arg(Format::bytes(progress.bytesDone)));
    }

    m_speedValue->setText(Format::speed(progress.bytesPerSecond));
    m_etaValue->setText(progress.msRemaining > 0 ? Format::duration(progress.msRemaining)
                                                 : QStringLiteral("--"));

    if (progress.filesTotal > 0) {
        m_filesValue->setText(QStringLiteral("%1 of %2")
                                  .arg(progress.filesDone)
                                  .arg(progress.filesTotal));
    } else if (progress.itemCount > 0) {
        m_filesValue->setText(QStringLiteral("%1 of %2 items")
                                  .arg(qMin(progress.itemIndex + 1, progress.itemCount))
                                  .arg(progress.itemCount));
    }

    if (!progress.currentFile.isEmpty() && progress.currentFile != m_fullCurrent) {
        m_fullCurrent = progress.currentFile;
        storeFullText(m_currentValue, m_fullCurrent);
    }
}

void TransferDialog::finish(const Transfer::Result &result)
{
    m_lastResult = result;
    m_hasResult = true;
    applyRunningState(false);

    if (result.cancelled) {
        m_titleLabel->setText(QStringLiteral("Cancelled"));
        m_footerLabel->setText(result.summary);
    } else if (result.itemsFailed > 0) {
        m_titleLabel->setText(QStringLiteral("Finished with problems"));
        m_footerLabel->setText(result.summary);
    } else {
        m_titleLabel->setText(QStringLiteral("Finished"));
        m_footerLabel->setText(QStringLiteral("%1 - took %2")
                                   .arg(result.summary, Format::duration(result.elapsedMs)));
    }

    if (m_bar->maximum() == 0)
        m_bar->setRange(0, 1000);
    m_bar->setValue(result.cancelled ? m_bar->value() : 1000);
    m_progressValue->setText(result.cancelled
                                 ? QStringLiteral("stopped")
                                 : QStringLiteral("100%"));
    m_speedValue->setText(QStringLiteral("--"));
    m_etaValue->setText(QStringLiteral("--"));
    m_totalValue->setText(Format::bytes(result.bytesMoved));
    m_transferredValue->setText(QStringLiteral("%1 moved").arg(Format::bytes(result.bytesMoved)));
    m_openButton->setVisible(!result.destination.isEmpty());

    if (result.hasErrors()) {
        QStringList lines;
        const int shown = qMin(5, int(result.errors.size()));
        for (int i = 0; i < shown; ++i) {
            const Transfer::Error &error = result.errors.at(i);
            lines << (error.path.isEmpty()
                          ? error.message
                          : QStringLiteral("%1: %2").arg(QFileInfo(error.path).fileName(),
                                                         error.message));
        }
        if (result.errors.size() > shown)
            lines << QStringLiteral("and %1 more (see the log)").arg(result.errors.size() - shown);
        m_footerLabel->setText(m_footerLabel->text() + QLatin1Char('\n') + lines.join(QLatin1Char('\n')));
    }

    QApplication::alert(this);
}

void TransferDialog::present()
{
    if (!m_placedOnce) {
        if (const QScreen *screen = QGuiApplication::primaryScreen()) {
            const QRect area = screen->availableGeometry();
            move(area.center() - QPoint(width() / 2, height() / 2));
        }
        m_placedOnce = true;
    }
    show();
    raise();
    activateWindow();
    if (isMinimized())
        setWindowState(windowState() & ~Qt::WindowMinimized);
}

void TransferDialog::closeEvent(QCloseEvent *event)
{
    if (m_running) {
        // The job keeps going: hiding is the friendly option, quitting mid-copy is not.
        hide();
        event->ignore();
        return;
    }
    hide();
    event->accept();
}

void TransferDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_running) {
        hide();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}
