#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

// A "petal" is a saved destination folder shown around the dial.
struct Petal {
    QString name;
    QString path;
};

// One level of undo for the last successful move.
struct UndoRecord {
    bool valid = false;
    QStringList sources;    // where the items were before the move
    QStringList targets;    // where they are now
    QString destination;    // folder the items were moved into
};

struct AppConfig {
    // floating dial
    QRect orbGeometry;
    bool hasOrbGeometry = false;
    int orbSize = 190;
    bool alwaysOnTop = true;
    bool lockPosition = false;
    bool snapToEdges = true;
    QString orbAnchor;              // "l", "rb", "t", ... (screen anchor, see FloatingWindow)
    int lastPetal = -1;             // last used petal, for quick-move with Enter

    // whatsapp bubble
    bool whatsAppVisible = true;
    int whatsAppSize = 66;
    QPoint whatsAppPos;
    bool hasWhatsAppPos = false;
    QString whatsAppAnchor;

    // behaviour
    bool showProgressWindow = true;   // open the detailed transfer window for every job
    bool startWithWindows = false;

    QList<Petal> petals;
    UndoRecord undo;
};

namespace ConfigStore {

constexpr int kMinOrbSize = 120;
constexpr int kMaxOrbSize = 460;
constexpr int kDefaultOrbSize = 190;
constexpr int kMinBubbleSize = 44;
constexpr int kMaxBubbleSize = 160;
constexpr int kDefaultBubbleSize = 66;

// Loaded once, shared by every window. Never written next to the .exe:
// an installed copy lives in Program Files, which is read-only for users.
AppConfig &data();
bool save();
void reload();
QString filePath();
QString folderPath();
QString logFolderPath();

// Fills geometry defaults the first time the app runs on this machine.
void applyDefaultsForScreen(const QRect &available);

} // namespace ConfigStore
