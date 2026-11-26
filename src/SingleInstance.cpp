#include "SingleInstance.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
    const QString lockDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QLockFile lock(QDir(lockDir).filePath(key + QStringLiteral(".lock")));
    lock.setStaleLockTime(0);

    if (lock.tryLock(250)) {
        lock.unlock(); // the QLocalServer is what actually keeps us unique
        m_primary = true;
    } else if (lock.error() == QLockFile::LockFailedError) {
        // Another instance holds the lock; it is either alive or was killed.
        if (lock.removeStaleLockFile() && lock.tryLock(250)) {
            lock.unlock();
            m_primary = true;
        }
    } else {
        m_primary = true;
    }

    if (!m_primary)
        return;

    QLocalServer::removeServer(m_key);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(m_key)) {
        // Cannot listen (unusual) - stay usable rather than refusing to start.
        m_primary = true;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            socket->readAll();
            socket->disconnectFromServer();
            emit raised();
        }
    });
}

void SingleInstance::notifyPrimary()
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (socket.waitForConnected(500)) {
        socket.write("show");
        socket.flush();
        socket.waitForBytesWritten(300);
        socket.disconnectFromServer();
    }
}
