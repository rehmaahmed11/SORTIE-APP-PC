#include "AboutDialog.h"

#include "Brand.h"
#include "Config.h"
#include "Logging.h"
#include "Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("About %1").arg(Brand::productName()));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app.ico")));
    setStyleSheet(Theme::appStyle());
    setMinimumWidth(520);
    resize(540, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 26, 28, 22);
    root->setSpacing(14);

    // ---- brand header
    auto *header = new QHBoxLayout;
    header->setSpacing(18);

    auto *logo = new QLabel(this);
    logo->setFixedSize(76, 76);
    logo->setPixmap(QIcon(QStringLiteral(":/assets/icons/app.png")).pixmap(72, 72));
    logo->setAlignment(Qt::AlignCenter);
    header->addWidget(logo, 0, Qt::AlignTop);

    auto *titles = new QVBoxLayout;
    titles->setSpacing(4);

    auto *product = new QLabel(QStringLiteral("AMS SORTIE"), this);
    product->setFont(Theme::font(24, true, 1.4));
    product->setStyleSheet(QStringLiteral("color:#00E5B0;"));
    titles->addWidget(product);

    auto *tagline = new QLabel(QStringLiteral("%1  -  version %2")
                                   .arg(Brand::tagline(), Brand::version()),
                               this);
    tagline->setObjectName(QStringLiteral("dimLabel"));
    tagline->setFont(Theme::font(12));
    titles->addWidget(tagline);

    auto *madeBy = new QLabel(
        QStringLiteral("Made by <b>%1</b> for <b>%2</b>").arg(Brand::author(), Brand::company()),
        this);
    madeBy->setFont(Theme::font(13));
    madeBy->setTextFormat(Qt::RichText);
    titles->addWidget(madeBy);

    titles->addSpacing(2);
    auto *phone = new QLabel(
        QStringLiteral("<a style='color:#00E5B0;text-decoration:none' href='%1'>%2</a>")
            .arg(Brand::callLink(), Brand::phoneDisplay()),
        this);
    phone->setFont(Theme::font(15, true));
    phone->setTextFormat(Qt::RichText);
    phone->setOpenExternalLinks(true);
    phone->setCursor(Qt::PointingHandCursor);
    titles->addWidget(phone);

    header->addLayout(titles, 1);
    root->addLayout(header);

    auto *rule = new QFrame(this);
    rule->setFrameShape(QFrame::HLine);
    rule->setStyleSheet(QStringLiteral("color:#2A343D; background:#2A343D; max-height:1px;"));
    root->addWidget(rule);

    // ---- what this build does
    auto *highlights = new QLabel(
        QStringLiteral(
            "<div style='color:#93A3B1; font-size:12px; line-height:150%'>"
            "<b style='color:#E8EDF2'>This build</b><br>"
            "&bull; Moves run on a background thread, so big folders no longer freeze or crash the dial.<br>"
            "&bull; A Windows-style transfer window shows the progress bar, transferred vs total size, "
            "speed, time left and file counts.<br>"
            "&bull; Cancellable at any time; every copy is verified before the original is removed.<br>"
            "&bull; Undo last move, long-path support and disk-space checks before a job starts.<br>"
            "&bull; Stretchable dial and WhatsApp bubble with real size grips and screen anchors."
            "</div>"),
        this);
    highlights->setTextFormat(Qt::RichText);
    highlights->setWordWrap(true);
    root->addWidget(highlights);

    root->addStretch(1);

    // ---- paths
    auto *paths = new QLabel(
        QStringLiteral("<div style='color:#62717D; font-size:11px'>Settings: %1<br>Logs: %2</div>")
            .arg(QDir::toNativeSeparators(ConfigStore::filePath()),
                 QDir::toNativeSeparators(Log::folderPath())),
        this);
    paths->setTextFormat(Qt::RichText);
    paths->setWordWrap(true);
    paths->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(paths);

    auto *copyright = new QLabel(Brand::copyrightLine(), this);
    copyright->setObjectName(QStringLiteral("faintLabel"));
    copyright->setFont(Theme::font(11));
    root->addWidget(copyright);

    // ---- actions
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);

    auto *chat = new QPushButton(QStringLiteral("Chat on WhatsApp"), this);
    chat->setObjectName(QStringLiteral("primaryButton"));
    chat->setCursor(Qt::PointingHandCursor);
    connect(chat, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(Brand::whatsAppLink(
            QStringLiteral("Hi Rehman, I am using AMS Sortie %1").arg(Brand::version()))));
    });

    auto *call = new QPushButton(QStringLiteral("Call"), this);
    call->setCursor(Qt::PointingHandCursor);
    connect(call, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(Brand::callLink()));
    });

    auto *copy = new QPushButton(QStringLiteral("Copy number"), this);
    copy->setCursor(Qt::PointingHandCursor);
    connect(copy, &QPushButton::clicked, this, []() {
        QApplication::clipboard()->setText(Brand::phoneDisplay());
    });

    auto *close = new QPushButton(QStringLiteral("Close"), this);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    buttons->addWidget(chat, 0);
    buttons->addWidget(call, 0);
    buttons->addWidget(copy, 0);
    buttons->addStretch(1);
    buttons->addWidget(close, 0);
    root->addLayout(buttons);
}
