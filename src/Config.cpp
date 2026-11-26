#include "Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr int kConfigVersion = 2;

QString configFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        dir = QDir(QDir::homePath()).filePath(QStringLiteral(".ams-sortie"));
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("sortie.json"));
}

void clampSizes(AppConfig &cfg)
{
    cfg.orbSize = qBound(ConfigStore::kMinOrbSize, cfg.orbSize, ConfigStore::kMaxOrbSize);
    cfg.whatsAppSize =
        qBound(ConfigStore::kMinBubbleSize, cfg.whatsAppSize, ConfigStore::kMaxBubbleSize);
    if (cfg.petals.size() > 64)
        cfg.petals = cfg.petals.mid(0, 64);
}

// Reads a legacy "petals_config.json" (v1 layout) into cfg. Returns true on success.
bool migrateLegacy(AppConfig &cfg)
{
    QStringList candidates;
    candidates << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("petals_config.json"));

    // The v1 build used organisation "Sortie" + application "Sortie".
    const QString oldDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                           + QStringLiteral("/Sortie/Sortie");
    candidates << QDir(oldDir).filePath(QStringLiteral("petals_config.json"));

    for (const QString &path : candidates) {
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject())
            continue;
        const QJsonObject o = doc.object();

        cfg.orbSize = o.value(QStringLiteral("widget_size")).toInt(cfg.orbSize);
        cfg.alwaysOnTop = o.value(QStringLiteral("always_on_top")).toBool(cfg.alwaysOnTop);
        if (o.contains(QStringLiteral("x")) && o.contains(QStringLiteral("y"))) {
            cfg.orbGeometry = QRect(QPoint(o.value(QStringLiteral("x")).toInt(),
                                           o.value(QStringLiteral("y")).toInt()),
                                    QSize(cfg.orbSize, cfg.orbSize));
            cfg.hasOrbGeometry = true;
        }
        const QJsonArray arr = o.value(QStringLiteral("folders")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject po = v.toObject();
            Petal p;
            p.path = po.value(QStringLiteral("path")).toString();
            p.name = po.value(QStringLiteral("name")).toString();
            if (p.name.isEmpty())
                p.name = QFileInfo(p.path).fileName();
            if (!p.path.isEmpty())
                cfg.petals.append(p);
        }
        return true;
    }
    return false;
}

} // namespace

namespace ConfigStore {

static AppConfig g_config;
static bool g_loaded = false;

QString filePath() { return configFilePath(); }

QString folderPath() { return QFileInfo(configFilePath()).absolutePath(); }

QString logFolderPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (dir.isEmpty())
        dir = folderPath();
    dir = QDir(dir).filePath(QStringLiteral("logs"));
    QDir().mkpath(dir);
    return dir;
}

void reload()
{
    g_config = AppConfig();
    g_loaded = true;

    QFile f(filePath());
    if (!f.open(QIODevice::ReadOnly)) {
        if (!migrateLegacy(g_config))
            return;
        clampSizes(g_config);
        save();
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    const QJsonObject o = doc.object();

    g_config.orbSize = o.value(QStringLiteral("orb_size")).toInt(kDefaultOrbSize);
    const QJsonArray orbGeom = o.value(QStringLiteral("orb_geometry")).toArray();
    if (orbGeom.size() == 4) {
        g_config.orbGeometry = QRect(orbGeom.at(0).toInt(), orbGeom.at(1).toInt(),
                                     orbGeom.at(2).toInt(), orbGeom.at(3).toInt());
        g_config.hasOrbGeometry = g_config.orbGeometry.width() > 0;
    }
    g_config.alwaysOnTop = o.value(QStringLiteral("always_on_top")).toBool(true);
    g_config.lockPosition = o.value(QStringLiteral("lock_position")).toBool(false);
    g_config.snapToEdges = o.value(QStringLiteral("snap_to_edges")).toBool(true);
    g_config.orbAnchor = o.value(QStringLiteral("orb_anchor")).toString();
    g_config.lastPetal = o.value(QStringLiteral("last_petal")).toInt(-1);

    g_config.whatsAppVisible = o.value(QStringLiteral("whatsapp_visible")).toBool(true);
    g_config.whatsAppSize = o.value(QStringLiteral("whatsapp_size")).toInt(kDefaultBubbleSize);
    const QJsonArray waPos = o.value(QStringLiteral("whatsapp_pos")).toArray();
    if (waPos.size() == 2) {
        g_config.whatsAppPos = QPoint(waPos.at(0).toInt(), waPos.at(1).toInt());
        g_config.hasWhatsAppPos = true;
    }
    g_config.whatsAppAnchor = o.value(QStringLiteral("whatsapp_anchor")).toString();

    g_config.showProgressWindow = o.value(QStringLiteral("show_progress_window")).toBool(true);
    g_config.startWithWindows = o.value(QStringLiteral("start_with_windows")).toBool(false);

    const QJsonArray petals = o.value(QStringLiteral("petals")).toArray();
    for (const QJsonValue &v : petals) {
        const QJsonObject po = v.toObject();
        Petal p;
        p.name = po.value(QStringLiteral("name")).toString();
        p.path = po.value(QStringLiteral("path")).toString();
        if (p.name.isEmpty())
            p.name = QFileInfo(p.path).fileName();
        if (!p.path.isEmpty())
            g_config.petals.append(p);
    }

    const QJsonObject undo = o.value(QStringLiteral("undo")).toObject();
    if (undo.value(QStringLiteral("valid")).toBool(false)) {
        const QJsonArray from = undo.value(QStringLiteral("sources")).toArray();
        const QJsonArray to = undo.value(QStringLiteral("targets")).toArray();
        if (from.size() == to.size() && !from.isEmpty()) {
            g_config.undo.valid = true;
            g_config.undo.destination = undo.value(QStringLiteral("destination")).toString();
            for (int i = 0; i < from.size(); ++i) {
                g_config.undo.sources << from.at(i).toString();
                g_config.undo.targets << to.at(i).toString();
            }
        }
    }

    clampSizes(g_config);
    g_config.hasOrbGeometry = g_config.hasOrbGeometry && !g_config.orbGeometry.isNull();
}

AppConfig &data()
{
    if (!g_loaded)
        reload();
    return g_config;
}

bool save()
{
    clampSizes(g_config);

    QJsonArray orbGeom;
    orbGeom.append(g_config.orbGeometry.x());
    orbGeom.append(g_config.orbGeometry.y());
    orbGeom.append(g_config.orbGeometry.width());
    orbGeom.append(g_config.orbGeometry.height());

    QJsonObject o;
    o[QStringLiteral("version")] = kConfigVersion;
    o[QStringLiteral("orb_size")] = g_config.orbSize;
    o[QStringLiteral("orb_geometry")] = orbGeom;
    o[QStringLiteral("always_on_top")] = g_config.alwaysOnTop;
    o[QStringLiteral("lock_position")] = g_config.lockPosition;
    o[QStringLiteral("snap_to_edges")] = g_config.snapToEdges;
    o[QStringLiteral("orb_anchor")] = g_config.orbAnchor;
    o[QStringLiteral("last_petal")] = g_config.lastPetal;
    o[QStringLiteral("whatsapp_visible")] = g_config.whatsAppVisible;
    o[QStringLiteral("whatsapp_size")] = g_config.whatsAppSize;
    o[QStringLiteral("whatsapp_pos")] = QJsonArray{ g_config.whatsAppPos.x(), g_config.whatsAppPos.y() };
    o[QStringLiteral("whatsapp_anchor")] = g_config.whatsAppAnchor;
    o[QStringLiteral("show_progress_window")] = g_config.showProgressWindow;
    o[QStringLiteral("start_with_windows")] = g_config.startWithWindows;

    QJsonArray petals;
    for (const Petal &p : g_config.petals) {
        QJsonObject po;
        po[QStringLiteral("name")] = p.name;
        po[QStringLiteral("path")] = p.path;
        petals.append(po);
    }
    o[QStringLiteral("petals")] = petals;

    QJsonObject undo;
    undo[QStringLiteral("valid")] = g_config.undo.valid && !g_config.undo.targets.isEmpty();
    undo[QStringLiteral("destination")] = g_config.undo.destination;
    QJsonArray from, to;
    for (const QString &s : g_config.undo.sources)
        from.append(s);
    for (const QString &s : g_config.undo.targets)
        to.append(s);
    undo[QStringLiteral("sources")] = from;
    undo[QStringLiteral("targets")] = to;
    o[QStringLiteral("undo")] = undo;

    // QSaveFile writes to a temporary file and renames on commit, so a crash
    // during save can never leave a truncated config behind.
    QSaveFile f(filePath());
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}

void applyDefaultsForScreen(const QRect &available)
{
    AppConfig &cfg = data();
    if (!cfg.hasOrbGeometry) {
        const QRect area = available.isValid() ? available : QRect(0, 0, 1280, 720);
        const int size = qBound(kMinOrbSize, cfg.orbSize, kMaxOrbSize);
        cfg.orbGeometry = QRect(area.right() - size - 48, area.bottom() - size - 72, size, size);
        cfg.hasOrbGeometry = true;
        cfg.orbAnchor = QStringLiteral("rb");
    }
    if (!cfg.hasWhatsAppPos) {
        const QRect area = available.isValid() ? available : QRect(0, 0, 1280, 720);
        const int size = qBound(kMinBubbleSize, cfg.whatsAppSize, kMaxBubbleSize);
        cfg.whatsAppPos = QPoint(area.left() + 32, area.bottom() - size - 48);
        cfg.hasWhatsAppPos = true;
        if (cfg.whatsAppAnchor.isEmpty())
            cfg.whatsAppAnchor = QStringLiteral("lb");
    }
}

} // namespace ConfigStore
