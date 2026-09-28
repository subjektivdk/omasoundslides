#include "exportfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace ExportFile {

QString createTemporary(const QString &target, QString *error)
{
    const QFileInfo info(target);
    QTemporaryFile file(info.absoluteDir().filePath(QStringLiteral(".omasoundslides-XXXXXX.mp4")));
    file.setAutoRemove(false);
    if (!file.open()) {
        *error = QStringLiteral("Cannot write in %1: %2").arg(info.absolutePath(), file.errorString());
        return {};
    }
    const QString path = file.fileName();
    file.close();
    return path;
}

bool finish(const QString &temporary, const QString &target, QString *error)
{
    // rename(2) replaces the target atomically; QFile::rename won't replace.
    if (std::rename(QFile::encodeName(temporary).constData(), QFile::encodeName(target).constData()) != 0) {
        *error = QStringLiteral("Cannot save %1: %2").arg(target, QString::fromLocal8Bit(std::strerror(errno)));
        QFile::remove(temporary);
        return false;
    }
    return true;
}

}
