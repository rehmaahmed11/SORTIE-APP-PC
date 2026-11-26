#include "AboutDialog.h"
#include "Autostart.h"
#include "Brand.h"
#include "Config.h"
#include "Logging.h"
#include "OrbWidget.h"
#include "SingleInstance.h"
#include "Theme.h"
#include "TransferDialog.h"
#include "TransferManager.h"
#include "WhatsAppBubble.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QScopedPointer>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
    // Crisp on every scaling factor Windows can throw at us (125 %, 150 %, mixed DPI).
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName(Brand::productName());
    app.setApplicationDisplayName(Brand::productName());
    app.setOrganizationName(Brand::company());
    app.setOrganizationDomain(QStringLiteral("ams.local"));
    app.setApplicationVersion(Brand::version());
    app.setDesktopFileName(QStringLiteral("ams-sortie"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app.ico")));
    app.setQuitOnLastWindowClosed(false);   // the dial lives in the tray
    app.setStyle(QStringLiteral("Fusion")); // predictable dark rendering on Windows
    app.setStyleSheet(Theme::appStyle());

    const QRect screenArea = QGuiApplication::primaryScreen()
                                 ? QGuiApplication::primaryScreen()->availableGeometry()
                                 : QRect(0, 0, 1280, 720);
    ConfigStore::applyDefaultsForScreen(screenArea);
    ConfigStore::save();

    Log::init();

    // One dial per desktop.
    SingleInstance guard(QStringLiteral("AMSSortie.Instance.v1"));
    if (!guard.isPrimary()) {
        guard.notifyPrimary();
        qInfo() << "another instance is already running - asked it to come up";
        return 0;
    }

    // Keep the registry in step with the saved preference.
    if (Autostart::isEnabled() != ConfigStore::data().startWithWindows)
        Autostart::setEnabled(ConfigStore::data().startWithWindows);

    OrbWidget orb;
    WhatsAppBubble bubble;
    TransferDialog transferWindow;

    orb.show();
    if (ConfigStore::data().whatsAppVisible) {
        bubble.show();
        bubble.raise();
    }

    TransferManager &manager = TransferManager::instance();

    QObject::connect(&manager, &TransferManager::started, &transferWindow,
                     [&transferWindow](const QStringList &sources, const QString &destination,
                                       bool showWindow) {
                         const bool copyMode =
                             TransferManager::instance().options().mode == Transfer::Mode::Copy;
                         transferWindow.begin(sources, destination, copyMode);
                         if (!showWindow)
                             return;
                         // A 2-file move must not flash a window: only surface the progress
                         // window when the job is still running a moment later.
                         QTimer::singleShot(350, &transferWindow, [&transferWindow]() {
                             if (TransferManager::instance().busy())
                                 transferWindow.present();
                         });
                     });
    QObject::connect(&manager, &TransferManager::progress, &transferWindow,
                     &TransferDialog::updateProgress);
    QObject::connect(&manager, &TransferManager::finished, &transferWindow,
                     &TransferDialog::finish);
    QObject::connect(&transferWindow, &TransferDialog::cancelRequested, &manager,
                     &TransferManager::cancel);
    QObject::connect(&transferWindow, &TransferDialog::openDestinationRequested, &app,
                     [](const QString &path) {
                         if (!path.isEmpty())
                             QDesktopServices::openUrl(QUrl::fromLocalFile(path));
                     });

    QObject::connect(&orb, &OrbWidget::transferWindowRequested, &transferWindow,
                     [&transferWindow]() { transferWindow.present(); });

    QObject::connect(&orb, &OrbWidget::whatsAppToggled, &bubble, [&bubble](bool visible) {
        bubble.setVisible(visible);
        if (visible) {
            bubble.show();
            bubble.raise();
        }
    });

    QObject::connect(&bubble, &WhatsAppBubble::hideRequested, &orb, []() {
        AppConfig &cfg = ConfigStore::data();
        cfg.whatsAppVisible = false;
        ConfigStore::save();
    });

    QObject::connect(&guard, &SingleInstance::raised, &orb, &OrbWidget::bringUp);

    QObject::connect(&orb, &OrbWidget::quitRequested, &app, [&]() {
        if (manager.busy()) {
            QMessageBox box(&orb);
            box.setStyleSheet(Theme::appStyle());
            box.setWindowTitle(QStringLiteral("A move is running"));
            box.setIcon(QMessageBox::Question);
            box.setText(QStringLiteral("A transfer is still running."));
            box.setInformativeText(QStringLiteral(
                "Quit anyway?\n\nThe job is cancelled and every original file is kept - "
                "nothing already moved is lost."));
            box.setStandardButtons(QMessageBox::Cancel | QMessageBox::Yes);
            box.setDefaultButton(QMessageBox::Cancel);
            if (box.exec() != QMessageBox::Yes)
                return;

            manager.cancel();
            QElapsedTimer wait;
            wait.start();
            while (manager.busy() && wait.elapsed() < 5000)
                QApplication::processEvents(QEventLoop::AllEvents, 100);
        }
        orb.savePlacement();
        ConfigStore::save();
        QApplication::quit();
    });

    QScopedPointer<QSystemTrayIcon> tray;
    QScopedPointer<QMenu> trayMenu;
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        trayMenu.reset(new QMenu);
        trayMenu->setStyleSheet(Theme::appStyle());

        QAction *show = trayMenu->addAction(QStringLiteral("Show AMS Sortie"));
        QObject::connect(show, &QAction::triggered, &orb, &OrbWidget::bringUp);
        trayMenu->addSeparator();
        orb.buildMenu(*trayMenu);

        tray.reset(new QSystemTrayIcon(QIcon(QStringLiteral(":/assets/icons/app.ico"))));
        tray->setToolTip(QStringLiteral("%1 %2 - %3 - %4")
                             .arg(Brand::productName(), Brand::version(), Brand::author(),
                                  Brand::phoneDisplay()));
        tray->setContextMenu(trayMenu.data());
        QObject::connect(tray.data(), &QSystemTrayIcon::activated, &orb,
                         [&orb](QSystemTrayIcon::ActivationReason reason) {
                             if (reason == QSystemTrayIcon::Trigger
                                 || reason == QSystemTrayIcon::DoubleClick)
                                 orb.bringUp();
                         });
        tray->show();
    }

    qInfo().noquote() << QStringLiteral("dial ready at %1,%2 (%3 px)")
                             .arg(orb.pos().x())
                             .arg(orb.pos().y())
                             .arg(orb.width());

    const int result = app.exec();

    ConfigStore::save();
    QObject::disconnect(&manager, nullptr, nullptr, nullptr);
    return result;
}
