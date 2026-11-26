// Data types for the background transfer engine.
//
// The v1 app moved files with a blocking loop on the GUI thread. A big folder
// froze the window, Windows marked it "Not responding" and the process was
// killed - that is the "crashes on long moves" report. Every move now runs on a
// worker thread and reports byte-accurate progress.
#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Transfer {

enum class Mode { Move, Copy };

struct Options {
    Mode mode = Mode::Move;
    bool verifySize = true;  // compare each copied file's size before deleting the source
    bool uniqueNames = true; // never overwrite: clash -> "name_copy1.ext"
};

struct Error {
    QString path;
    QString message;
};

struct Progress {
    enum Phase {
        Idle,
        Scanning,     // counting bytes/files so the bar can show a real total
        Transferring, // copying bytes
        Finishing,    // deleting sources / verifying
        Completed,
        Cancelled,
        Failed
    };

    Phase phase = Idle;
    qint64 bytesTotal = 0;
    qint64 bytesDone = 0;
    qint64 filesTotal = 0;
    qint64 filesDone = 0;
    qint64 dirsTotal = 0;
    qint64 dirsDone = 0;
    qint64 bytesPerSecond = 0;
    qint64 msRemaining = -1;
    qint64 msElapsed = 0;
    QString currentFile;
    QString destination;
    int itemIndex = 0;
    int itemCount = 0;

    double fraction() const
    {
        if (bytesTotal <= 0)
            return phase == Completed ? 1.0 : 0.0;
        const double f = double(bytesDone) / double(bytesTotal);
        return qBound(0.0, f, 1.0);
    }
};

struct Result {
    bool ok = false;
    bool cancelled = false;
    int itemsMoved = 0;
    int itemsFailed = 0;
    qint64 bytesMoved = 0;
    qint64 elapsedMs = 0;
    QString sourceLabel;
    QString destination;
    QStringList undoFrom;
    QStringList undoTo;
    QVector<Error> errors;
    QString summary;

    bool hasErrors() const { return !errors.isEmpty(); }
};

} // namespace Transfer

Q_DECLARE_METATYPE(Transfer::Progress)
Q_DECLARE_METATYPE(Transfer::Result)
Q_DECLARE_METATYPE(Transfer::Options)
