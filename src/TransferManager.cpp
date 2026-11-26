#include "TransferManager.h"

#include "Config.h"
#include "Logging.h"
#include "TransferWorker.h"

#include <QThread>

TransferManager &TransferManager::instance()
{
    static TransferManager manager;
    return manager;
}

TransferManager::TransferManager()
{
    qRegisterMetaType<Transfer::Progress>("Transfer::Progress");
    qRegisterMetaType<Transfer::Result>("Transfer::Result");
}

bool TransferManager::start(const QStringList &sources, const QString &destination,
                            const Transfer::Options &options)
{
    if (m_busy || sources.isEmpty() || destination.isEmpty())
        return false;

    m_busy = true;
    m_options = options;
    m_progress = Transfer::Progress();
    m_progress.destination = destination;
    m_progress.itemCount = sources.size();
    m_result = Transfer::Result();

    // Deliberately parentless: if the user quits while a job is stuck on a slow
    // network share, leaking the thread object is safer than destroying a running QThread.
    m_thread = new QThread();
    m_thread->setObjectName(QStringLiteral("sortie-transfer"));
    m_worker = new TransferWorker(sources, destination, options);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &TransferWorker::run);
    connect(m_worker, &TransferWorker::progress, this, &TransferManager::progress);
    connect(m_worker, &TransferWorker::finished, this, &TransferManager::onWorkerFinished);

    qInfo().noquote() << QStringLiteral("transfer start: %1 item(s) -> %2")
                             .arg(sources.size())
                             .arg(destination);

    const bool showWindow = ConfigStore::data().showProgressWindow;
    emit started(sources, destination, showWindow);
    m_thread->start();
    return true;
}

void TransferManager::cancel()
{
    if (m_worker)
        m_worker->requestCancel();
}

void TransferManager::onWorkerFinished(const Transfer::Result &result)
{
    m_result = result;
    m_progress.phase = result.cancelled ? Transfer::Progress::Cancelled
                                        : (result.hasErrors() ? Transfer::Progress::Failed
                                                              : Transfer::Progress::Completed);
    m_busy = false;

    // Stop the thread before touching the objects that live in it.
    if (m_thread) {
        m_thread->quit();
        if (!m_thread->wait(6000)) {
            qWarning() << "transfer thread did not stop in time; terminating";
            m_thread->terminate();
            m_thread->wait(2000);
        }
    }
    delete m_worker;
    m_worker = nullptr;
    if (m_thread) {
        m_thread->deleteLater();
        m_thread = nullptr;
    }

    for (const Transfer::Error &e : result.errors)
        qWarning().noquote() << QStringLiteral("transfer error: %1 %2").arg(e.path, e.message);
    qInfo().noquote() << QStringLiteral("transfer finished: %1").arg(result.summary);

    emit finished(result);
}
