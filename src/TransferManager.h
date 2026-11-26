#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>

#include "Transfer.h"

class QThread;
class TransferWorker;

// Single owner of the background transfer. Only one job runs at a time; the
// dial asks for another job and gets a busy flag back, so the UI can never
// start two writers into the same folder.
class TransferManager : public QObject
{
    Q_OBJECT
public:
    static TransferManager &instance();

    bool busy() const { return m_busy; }
    bool start(const QStringList &sources, const QString &destination,
               const Transfer::Options &options = Transfer::Options());
    void cancel();

    Transfer::Options options() const { return m_options; }
    Transfer::Progress lastProgress() const { return m_progress; }
    Transfer::Result lastResult() const { return m_result; }

signals:
    void started(const QStringList &sources, const QString &destination, bool showWindow);
    void progress(const Transfer::Progress &progress);
    void finished(const Transfer::Result &result);

private slots:
    void onWorkerFinished(const Transfer::Result &result);

private:
    TransferManager();

    QThread *m_thread = nullptr;
    TransferWorker *m_worker = nullptr;
    bool m_busy = false;
    Transfer::Options m_options;
    Transfer::Progress m_progress;
    Transfer::Result m_result;
};
