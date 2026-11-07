#include "vaultrepository.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
bool VaultRepository::exists() const { return QFileInfo::exists(file); }
QJsonObject VaultRepository::read() const {
    const QFileInfo info(file);
    if (!cached.isEmpty() && stamp == info.lastModified() && size == info.size() &&
        metadataStamp == info.fileTime(QFileDevice::FileMetadataChangeTime))
        return cached;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 32 * 1024 * 1024) {
        cached = {};
        ++generation;
        message = "无法读取密库或文件过大";
        return {};
    }
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(f.readAll(), &e);
    if (e.error != QJsonParseError::NoError || !doc.isObject()) {
        cached = {};
        ++generation;
        message = "密库格式损坏";
        return {};
    }
    message.clear();
    cached = doc.object();
    stamp = info.lastModified();
    size = info.size();
    metadataStamp = info.fileTime(QFileDevice::FileMetadataChangeTime);
    ++generation;
    return cached;
}
bool VaultRepository::write(const QJsonObject &obj) {
    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile f(file);
    const auto data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (data.size() > 32 * 1024 * 1024) {
        message = "密库超过32 MiB，请精简条目";
        return false;
    }
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        message = "密库写入失败，旧文件保持不变";
        return false;
    }
    cached = {};
    size = -1;
    ++generation;
    message.clear();
    return true;
}
bool VaultRepository::backup(const QString &destination) const {
    if (QFileInfo(destination).absoluteFilePath() == QFileInfo(file).absoluteFilePath()) {
        message = "请选择其他备份位置";
        return false;
    }
    QFile source(file);
    QSaveFile target(destination);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly)) {
        message = "无法打开备份文件";
        return false;
    }
    const auto bytes = source.readAll();
    if (target.write(bytes) != bytes.size() || !target.commit()) {
        message = "备份失败";
        return false;
    }
    return true;
}
