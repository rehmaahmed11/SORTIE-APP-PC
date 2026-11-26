#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

// One dial per desktop: launching the app again just brings the running dial up
// instead of starting a second copy that fights over the config file.
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);

    bool isPrimary() const { return m_primary; }
    void notifyPrimary();

signals:
    void raised();

private:
    QString m_key;
    bool m_primary = false;
    QLocalServer *m_server = nullptr;
};
