#include "TransferWorker.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QThread>

#include <utility>

namespace {

constexpr qint64 kChunkSize = 1024 * 1024;      // 1 MiB read/write blocks
constexpr int kEmitIntervalMs = 60;             // max ~16 UI updates per second
constexpr int kSpeedIntervalMs = 400;
constexpr qint64 kSpaceSlackBytes = 64LL * 1024 * 1024;
constexpr int kMaxErrorsKept = 40;

// Windows silently fails on paths longer than MAX_PATH unless they carry the
// \\?\ prefix. Long files are exactly the ones people sort, so translate.
QString ioPath(const QString &path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (native.size() < 240 || native.startsWith(QStringLiteral("\\\\?\\")))
        return path;
    if (native.startsWith(QStringLiteral("\\\\")))
        return QStringLiteral("\\\\?\\UNC\\") + native.mid(2);
    return QStringLiteral("\\\\?\\") + native;
#else
    return path;
#endif
}

QString nearestExisting(const QString &path)
{
    QString p = QFileInfo(path).absoluteFilePath();
    int guard = 0;
    while (!p.isEmpty() && !QFileInfo::exists(p) && guard++ < 64) {
        const int slash = p.lastIndexOf(QLatin1Char('/'));
        if (slash <= 0)
            break;
        p = p.left(slash);
    }
    return p;
}

bool ensureDirectory(const QString &path)
{
    const QFileInfo info(path);
    if (info.exists())
        return info.isDir();
    return QDir().mkpath(path);
}

} // namespace

TransferWorker::TransferWorker(QStringList sources, QString destination,
                               Transfer::Options options, QObject *parent)
    : QObject(parent)
    , m_sources(std::move(sources))
    , m_destination(std::move(destination))
    , m_options(options)
{
    m_cancel.storeRelease(0);
}

void TransferWorker::requestCancel()
{
    m_cancel.storeRelease(1);
}

bool TransferWorker::cancelled() const
{
    return m_cancel.loadAcquire() != 0;
}

void TransferWorker::emitProgress(bool force)
{
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    if (!force && now - m_lastEmitMs < kEmitIntervalMs)
        return;
    m_lastEmitMs = now;
    m_progress.msElapsed = now;
    emit progress(m_progress);
}

void TransferWorker::noteError(const QString &path, const QString &message)
{
    if (m_result.errors.size() >= kMaxErrorsKept) {
        m_errorOverflow = true;
        return;
    }
    m_result.errors.append(Transfer::Error{ path, message });
}

void TransferWorker::updateSpeed()
{
    const qint64 now = m_clock.elapsed();
    if (m_lastSpeedMs == 0) {
        m_lastSpeedMs = now;
        m_lastSpeedBytes = m_progress.bytesDone;
        return;
    }
    const qint64 dt = now - m_lastSpeedMs;
    if (dt < kSpeedIntervalMs)
        return;

    const qint64 delta = m_progress.bytesDone - m_lastSpeedBytes;
    const double instant = double(delta) * 1000.0 / double(qMax<qint64>(1, dt));
    if (m_progress.bytesPerSecond > 0)
        m_progress.bytesPerSecond =
            qint64(0.55 * double(m_progress.bytesPerSecond) + 0.45 * instant);
    else
        m_progress.bytesPerSecond = qint64(instant);

    m_lastSpeedMs = now;
    m_lastSpeedBytes = m_progress.bytesDone;

    const qint64 remaining = m_progress.bytesTotal - m_progress.bytesDone;
    if (m_progress.bytesPerSecond > 0 && remaining > 0) {
        m_progress.msRemaining =
            qint64(double(remaining) / double(m_progress.bytesPerSecond) * 1000.0);
    } else {
        m_progress.msRemaining = remaining > 0 ? -1 : 0;
    }
}

bool TransferWorker::sameVolume(const QString &a, const QString &b)
{
    const QFileInfo fa(a);
    const QFileInfo fb(b);
    QStorageInfo sa(fa.isDir() ? fa.absoluteFilePath() : fa.absolutePath());
    QStorageInfo sb(fb.isDir() ? fb.absoluteFilePath() : fb.absolutePath());
    if (!sa.isValid() || !sb.isValid())
        return false;
    sa.refresh();
    sb.refresh();
    if (!sa.device().isEmpty() && sa.device() == sb.device())
        return true;
    const QString ra = QDir::cleanPath(sa.rootPath());
    const QString rb = QDir::cleanPath(sb.rootPath());
    return !ra.isEmpty() && ra == rb;
}

QString TransferWorker::makeUniqueTarget(const QString &destDir, const QFileInfo &source)
{
    const QDir dir(destDir);
    const QString baseName = source.fileName();
    QString candidate = dir.absoluteFilePath(baseName);
    if (!QFileInfo::exists(candidate))
        return candidate;

    const bool isDir = source.isDir();
    const QString base = isDir ? baseName : source.completeBaseName();
    const QString suffix = isDir ? QString() : source.suffix();

    for (int i = 1; i < 100000; ++i) {
        const QString name = suffix.isEmpty()
                                 ? QStringLiteral("%1_copy%2").arg(base).arg(i)
                                 : QStringLiteral("%1_copy%2.%3").arg(base).arg(i).arg(suffix);
        candidate = dir.absoluteFilePath(name);
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

// ---------------------------------------------------------------- scanning

void TransferWorker::scan(const QString &source)
{
    const QFileInfo info(source);
    if (!info.exists()) {
        noteError(source, QStringLiteral("Source no longer exists."));
        m_itemTotals.append(Totals());
        return;
    }

    Totals totals;
    if (info.isDir() && !info.isSymLink()) {
        QDirIterator it(source,
                        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancelled())
                return;
            it.next();
            const QFileInfo entry = it.fileInfo();
            if (entry.isDir()) {
                ++totals.dirs;
                ++m_progress.dirsTotal;
            } else {
                ++totals.files;
                ++m_progress.filesTotal;
                totals.bytes += entry.size();
                m_progress.bytesTotal += entry.size();
            }
            m_progress.currentFile = entry.fileName();
            updateSpeed();
            emitProgress();
        }
    } else {
        ++totals.files;
        ++m_progress.filesTotal;
        totals.bytes = info.size();
        m_progress.bytesTotal += info.size();
        emitProgress();
    }

    m_itemTotals.append(totals);
}

// ---------------------------------------------------------------- copying

bool TransferWorker::copyFile(const QString &source, const QString &target, qint64 expectedSize)
{
    QFile in(ioPath(source));
    if (!in.open(QIODevice::ReadOnly)) {
        noteError(source, QStringLiteral("Cannot read the file (in use or access denied)."));
        return false;
    }

    const QString tempPath = target + QStringLiteral(".sortie-part");
    QFile out(ioPath(tempPath));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        noteError(target, QStringLiteral("Cannot write to the destination (disk full or access denied)."));
        return false;
    }

    const int bufferSize = int(qBound<qint64>(64 * 1024, expectedSize, kChunkSize));
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    qint64 written = 0;
    bool ok = true;
    while (true) {
        if (cancelled()) {
            ok = false;
            break;
        }
        const qint64 got = in.read(buffer.data(), buffer.size());
        if (got < 0) {
            noteError(source, QStringLiteral("Read error while copying."));
            ok = false;
            break;
        }
        if (got == 0)
            break;

        qint64 offset = 0;
        while (offset < got) {
            const qint64 put = out.write(buffer.constData() + offset, got - offset);
            if (put <= 0) {
                noteError(target, QStringLiteral("Write failed (disk full or access denied)."));
                ok = false;
                break;
            }
            offset += put;
        }
        if (!ok)
            break;

        written += got;
        m_progress.bytesDone += got;
        m_progress.currentFile = QFileInfo(target).fileName();
        updateSpeed();
        emitProgress();
    }

    out.flush();
    const bool flushed = out.error() == QFileDevice::NoError;
    out.close();
    in.close();

    if (!ok || !flushed || (m_options.verifySize && written != expectedSize)) {
        if (ok && m_options.verifySize && written != expectedSize)
            noteError(source, QStringLiteral("Size check failed - the copy was discarded, original kept."));
        else if (ok)
            noteError(target, QStringLiteral("The copy did not finish cleanly."));
        QFile::remove(ioPath(tempPath));
        return false;
    }

    // Publish the file only once it is complete and verified.
    if (QFileInfo::exists(target) && !(m_options.uniqueNames))
        QFile::remove(ioPath(target));
    if (!QFile::rename(ioPath(tempPath), ioPath(target))) {
        if (!QFile::rename(tempPath, target)) {
            QFile::remove(ioPath(tempPath));
            noteError(target, QStringLiteral("Could not finalise the copied file."));
            return false;
        }
    }

    QFile finished(ioPath(target));
    if (finished.open(QIODevice::ReadWrite)) {
        finished.setFileTime(QFileInfo(source).lastModified(), QFileDevice::FileModificationTime);
        finished.close();
    }

    ++m_progress.filesDone;
    return true;
}

bool TransferWorker::copyTree(const QString &sourceRoot, const QString &targetRoot, Totals *copied)
{
    if (!ensureDirectory(targetRoot)) {
        noteError(targetRoot, QStringLiteral("Cannot create the destination folder."));
        return false;
    }

    const QDir root(sourceRoot);
    bool allGood = true;

    QDirIterator it(sourceRoot,
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (cancelled())
            return false;
        it.next();

        const QFileInfo entry = it.fileInfo();
        const QString relative = root.relativeFilePath(it.filePath());
        const QString target = QDir(targetRoot).filePath(relative);
        m_progress.currentFile = relative;

        if (entry.isDir()) {
            if (!ensureDirectory(target)) {
                noteError(target, QStringLiteral("Cannot create a sub-folder."));
                allGood = false;
            } else if (!entry.isSymLink()) {
                if (copied)
                    ++copied->dirs;
                ++m_progress.dirsDone;
            }
            emitProgress();
            continue;
        }

        // A symlink to a file is copied as its content; a broken link is skipped.
        if (entry.isSymLink() && !QFileInfo(entry.symLinkTarget()).exists()) {
            noteError(it.filePath(), QStringLiteral("Broken shortcut skipped."));
            allGood = false;
            continue;
        }

        if (!ensureDirectory(QFileInfo(target).absolutePath())) {
            noteError(target, QStringLiteral("Cannot create a sub-folder."));
            allGood = false;
            continue;
        }

        if (!copyFile(it.filePath(), target, entry.size())) {
            if (cancelled())
                return false;
            allGood = false;
            continue;
        }
        if (copied)
            ++copied->files;
    }
    return allGood;
}

bool TransferWorker::moveOnSameVolume(const QString &source, const QString &target, bool isDir)
{
    if (isDir)
        return QDir().rename(source, target);
    return QFile::rename(ioPath(source), ioPath(target));
}

void TransferWorker::rollback(const QString &path, bool isDir)
{
    if (path.isEmpty())
        return;
    if (isDir)
        QDir(path).removeRecursively();
    else
        QFile::remove(ioPath(path));
}

bool TransferWorker::ensureSpace(qint64 needed)
{
    if (needed <= 0)
        return true;
    // Moving inside one volume is a rename: no extra space required.
    if (m_options.mode == Transfer::Mode::Move && !m_sources.isEmpty()
        && sameVolume(m_sources.first(), m_destination))
        return true;

    const QStorageInfo storage(nearestExisting(m_destination));
    if (!storage.isValid())
        return true; // cannot tell; let the copy itself report the real error
    if (storage.bytesAvailable() <= 0)
        return true;

    if (needed + kSpaceSlackBytes > storage.bytesAvailable()) {
        noteError(m_destination,
                  QStringLiteral("Not enough free space on %1: need %2, only %3 available.")
                      .arg(storage.rootPath().isEmpty() ? QStringLiteral("the destination")
                                                        : storage.rootPath())
                      .arg(QStringLiteral("%1 MB").arg((needed + kSpaceSlackBytes) / (1024 * 1024)))
                      .arg(QStringLiteral("%1 MB").arg(storage.bytesAvailable() / (1024 * 1024))));
        return false;
    }
    return true;
}

void TransferWorker::finish(Transfer::Progress::Phase phase)
{
    m_progress.phase = phase;
    m_progress.bytesPerSecond = 0;
    m_progress.msRemaining = 0;
    emitProgress(true);

    m_result.elapsedMs = m_clock.elapsed();
    m_result.bytesMoved = m_progress.bytesDone;

    const QString verb = m_options.mode == Transfer::Mode::Move ? QStringLiteral("Moved")
                                                                : QStringLiteral("Copied");
    if (phase == Transfer::Progress::Cancelled) {
        m_result.cancelled = true;
        m_result.summary = QStringLiteral("Cancelled - %1 of %2 items handled. Nothing was lost.")
                               .arg(m_result.itemsMoved)
                               .arg(m_sources.size());
    } else if (m_result.itemsFailed > 0) {
        m_result.summary = QStringLiteral("%1 %2 of %3 items - %4 could not be handled.")
                               .arg(verb)
                               .arg(m_result.itemsMoved)
                               .arg(m_sources.size())
                               .arg(m_result.itemsFailed);
    } else {
        m_result.summary = QStringLiteral("%1 %2 %3.")
                               .arg(verb)
                               .arg(m_result.itemsMoved)
                               .arg(m_result.itemsMoved == 1 ? QStringLiteral("item")
                                                             : QStringLiteral("items"));
    }
    if (m_errorOverflow)
        m_result.errors.append(Transfer::Error{ QString(), QStringLiteral("More errors were suppressed.") });

    emit finished(m_result);
}

// ---------------------------------------------------------------- main entry

void TransferWorker::run()
{
    m_clock.start();
    m_lastSpeedMs = 0;
    m_lastEmitMs = 0;

    m_progress.phase = Transfer::Progress::Scanning;
    m_progress.destination = m_destination;
    m_progress.itemCount = m_sources.size();
    m_result.destination = m_destination;
    m_result.sourceLabel = m_sources.size() == 1 ? QFileInfo(m_sources.first()).fileName()
                                                 : QStringLiteral("%1 items").arg(m_sources.size());
    emitProgress(true);

    // ---- Phase 1: measure, so the bar can show "transferred of total".
    for (int i = 0; i < m_sources.size(); ++i) {
        if (cancelled()) {
            finish(Transfer::Progress::Cancelled);
            return;
        }
        m_progress.itemIndex = i;
        m_progress.currentFile = QFileInfo(m_sources.at(i)).fileName();
        scan(m_sources.at(i));
        emitProgress(true);
    }
    if (cancelled()) {
        finish(Transfer::Progress::Cancelled);
        return;
    }

    if (!ensureSpace(m_progress.bytesTotal)) {
        m_result.itemsFailed = m_sources.size();
        finish(Transfer::Progress::Failed);
        return;
    }

    // ---- Phase 2: move/copy.
    m_progress.phase = Transfer::Progress::Transferring;
    m_progress.bytesDone = 0;
    m_progress.filesDone = 0;
    m_progress.dirsDone = 0;
    emitProgress(true);

    QDir destinationDir(m_destination);
    if (!destinationDir.exists() && !destinationDir.mkpath(QStringLiteral("."))) {
        noteError(m_destination, QStringLiteral("Cannot create the destination folder."));
        m_result.itemsFailed = m_sources.size();
        finish(Transfer::Progress::Failed);
        return;
    }

    const QString destinationAbs = QDir::cleanPath(QFileInfo(destinationDir.absolutePath()).absoluteFilePath());
    const QString destinationNative = QDir::toNativeSeparators(destinationAbs);

    bool stop = false;
    for (int i = 0; i < m_sources.size() && !stop; ++i) {
        if (cancelled()) {
            stop = true;
            break;
        }

        const QString source = m_sources.at(i);
        const QFileInfo sourceInfo(source);
        const Totals expected = m_itemTotals.value(i);
        m_progress.itemIndex = i;
        m_progress.currentFile = sourceInfo.fileName();

        if (!sourceInfo.exists()) {
            noteError(source, QStringLiteral("Source no longer exists."));
            ++m_result.itemsFailed;
            continue;
        }

        const QString sourceAbs = QDir::cleanPath(sourceInfo.absoluteFilePath());
        const QString sourceNative = QDir::toNativeSeparators(sourceAbs);

        if (QDir::cleanPath(sourceInfo.absolutePath()) == destinationAbs) {
            noteError(source, QStringLiteral("That item is already in this folder."));
            ++m_result.itemsFailed;
            continue;
        }
        if (sourceInfo.isDir()
            && (destinationAbs == sourceAbs || destinationNative.startsWith(sourceNative + QLatin1Char('\\'))
                || destinationAbs.startsWith(sourceAbs + QLatin1Char('/')))) {
            noteError(source, QStringLiteral("A folder cannot be moved inside itself."));
            ++m_result.itemsFailed;
            continue;
        }

        if (!sourceInfo.isReadable()) {
            noteError(source, QStringLiteral("Access denied."));
            ++m_result.itemsFailed;
            continue;
        }

        const QString target = makeUniqueTarget(destinationAbs, sourceInfo);
        if (target.isEmpty()) {
            noteError(source, QStringLiteral("No free file name could be found."));
            ++m_result.itemsFailed;
            continue;
        }

        const bool sameDisk = sameVolume(sourceAbs, destinationAbs);
        bool handled = false;

        // Fast path: an instant rename on the same volume.
        if (m_options.mode == Transfer::Mode::Move && sameDisk) {
            if (moveOnSameVolume(sourceAbs, target, sourceInfo.isDir())) {
                m_progress.bytesDone += expected.bytes;
                m_progress.filesDone += expected.files;
                m_progress.dirsDone += expected.dirs;
                handled = true;
            }
        }

        if (!handled) {
            Totals copied;
            bool copiedOk = false;
            if (sourceInfo.isDir()) {
                copiedOk = copyTree(sourceAbs, target, &copied);
                // Verify the tree size before we delete anything.
                if (copiedOk && m_options.verifySize) {
                    Totals verify;
                    QDirIterator verifyIt(target,
                                          QDir::Files | QDir::Hidden | QDir::System,
                                          QDirIterator::Subdirectories);
                    while (verifyIt.hasNext()) {
                        verifyIt.next();
                        ++verify.files;
                        verify.bytes += verifyIt.fileInfo().size();
                    }
                    if (verify.files != copied.files || verify.bytes != copied.bytes) {
                        noteError(target, QStringLiteral("Size check failed - copied folder does not match."));
                        copiedOk = false;
                    }
                }
            } else {
                copiedOk = copyFile(sourceAbs, target, sourceInfo.size());
            }

            if (cancelled()) {
                rollback(target, sourceInfo.isDir());
                stop = true;
                break;
            }

            if (!copiedOk) {
                rollback(target, sourceInfo.isDir());
                ++m_result.itemsFailed;
                continue;
            }

            if (m_options.mode == Transfer::Mode::Move) {
                m_progress.phase = Transfer::Progress::Finishing;
                bool removed = sourceInfo.isDir() ? QDir(sourceAbs).removeRecursively()
                                                  : QFile::remove(ioPath(sourceAbs));
                m_progress.phase = Transfer::Progress::Transferring;
                if (!removed) {
                    noteError(source,
                              QStringLiteral("Copied to the new folder, but the original could not be "
                                             "deleted (in use by another program)."));
                }
            }
        }

        ++m_result.itemsMoved;
        m_result.undoFrom << sourceAbs;
        m_result.undoTo << target;
        m_progress.currentFile = QFileInfo(target).fileName();
        updateSpeed();
        emitProgress(true);
    }

    m_result.ok = !cancelled() && m_result.itemsFailed == 0 && m_result.itemsMoved > 0;

    if (cancelled() || stop) {
        finish(Transfer::Progress::Cancelled);
        return;
    }
    finish(m_result.itemsFailed > 0 ? Transfer::Progress::Failed
                                    : Transfer::Progress::Completed);
}
