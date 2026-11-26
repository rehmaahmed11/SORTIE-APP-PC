#pragma once

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "Transfer.h"

// Runs one move/copy job. Lives in its own QThread; never touches widgets.
class TransferWorker : public QObject
{
    Q_OBJECT
public:
    TransferWorker(QStringList sources, QString destination, Transfer::Options options,
                   QObject *parent = nullptr);

    // Thread-safe: called straight from the GUI thread while run() is busy.
    void requestCancel();

public slots:
    void run();

signals:
    void progress(const Transfer::Progress &progress);
    void finished(const Transfer::Result &result);

private:
    struct Totals {
        qint64 bytes = 0;
        qint64 files = 0;
        qint64 dirs = 0;
    };

    bool cancelled() const;
    void emitProgress(bool force = false);
    void scan(const QString &source);
    bool copyFile(const QString &source, const QString &target, qint64 expectedSize);
    bool copyTree(const QString &sourceRoot, const QString &targetRoot, Totals *copied);
    bool moveOnSameVolume(const QString &source, const QString &target, bool isDir);
    void rollback(const QString &path, bool isDir);
    void noteError(const QString &path, const QString &message);
    void updateSpeed();
    bool ensureSpace(qint64 needed);
    void finish(Transfer::Progress::Phase phase);

    static QString makeUniqueTarget(const QString &destDir, const QFileInfo &source);
    static bool sameVolume(const QString &a, const QString &b);

    QStringList m_sources;
    QString m_destination;
    Transfer::Options m_options;
    QAtomicInt m_cancel;
    Transfer::Progress m_progress;
    Transfer::Result m_result;
    QElapsedTimer m_clock;
    QVector<Totals> m_itemTotals;
    qint64 m_lastEmitMs = 0;
    qint64 m_lastSpeedMs = 0;
    qint64 m_lastSpeedBytes = 0;
    bool m_errorOverflow = false;
};
