#pragma once
#include "cryptotool.h"
#include "paths.h"
#include <QtCore>
// A durable roll-forward journal. Readers reload only after every replacement succeeds.
namespace FileBatch {
inline QString journal() { return Paths::data("config/shared-commit.pending"); }
inline bool allowed(const QString &path) {
    const auto root = QDir(Paths::root()).absolutePath() + "/",
               target = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (!target.startsWith(root, Qt::CaseInsensitive))
        return false;
    for (auto current = QFileInfo(target); current.absoluteFilePath() != QDir(Paths::root()).absolutePath();
         current = QFileInfo(current.absolutePath()))
        if (current.isSymLink())
            return false;
    return true;
}
inline bool write(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
inline bool persist(const QJsonObject &payload) {
    try {
        const auto bytes =
            CryptoTool::protect(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)))
                .toUtf8();
        return !bytes.isEmpty() && write(journal(), bytes);
    } catch (...) {
        return false;
    }
}
inline bool recover() {
    QFile file(journal());
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024)
        return false;
    QJsonParseError parse;
    QJsonDocument doc;
    try {
        doc = QJsonDocument::fromJson(CryptoTool::unprotect(QString::fromUtf8(file.readAll())).toUtf8(),
                                      &parse);
    } catch (...) {
        return false;
    }
    file.close();
    if (parse.error != QJsonParseError::NoError || !doc.isObject() || doc.object().isEmpty())
        return false;
    const auto payload = doc.object();
    const auto files = payload["format"] == 2
                           ? payload[payload["mode"] == "rollback" ? "previous" : "targets"].toObject()
                           : payload;
    if (files.isEmpty())
        return false;
    for (auto it = files.begin(); it != files.end(); ++it)
        if (!allowed(it.key()) || (!it.value().isString() && !it.value().isNull()))
            return false;
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (it.value().isNull()) {
            if (QFile::exists(it.key()) && !QFile::remove(it.key()))
                return false;
        } else if (!write(it.key(), QByteArray::fromBase64(it.value().toString().toLatin1())))
            return false;
    }
    return QFile::remove(journal());
}
inline bool commit(const QMap<QString, QByteArray> &files) {
    if (QFile::exists(journal()) && !recover())
        return false;
    QJsonObject targets, previous;
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (!allowed(it.key()))
            return false;
        targets[it.key()] = QString::fromLatin1(it.value().toBase64());
        QFile original(it.key());
        if (original.exists()) {
            if (!original.open(QIODevice::ReadOnly) || original.size() > 32 * 1024 * 1024)
                return false;
            previous[it.key()] = QString::fromLatin1(original.readAll().toBase64());
        } else
            previous[it.key()] = QJsonValue::Null;
    }
    QJsonObject payload{{"format", 2}, {"mode", "apply"}, {"targets", targets}, {"previous", previous}};
    if (!persist(payload))
        return false;
    if (recover())
        return true;
    // Keep live objects on the old snapshot when any replacement fails. If rollback also
    // fails, its durable journal is completed before startup or another settings write.
    payload["mode"] = "rollback";
    if (persist(payload))
        recover();
    return false;
}
} // namespace FileBatch
