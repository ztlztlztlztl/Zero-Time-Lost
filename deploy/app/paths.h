#pragma once
#include <QCoreApplication>
#include <QDir>
namespace Paths {
inline QString root() {
    auto p = qEnvironmentVariable("ZTL_MANAGER_DATA_DIR");
    if (p.isEmpty())
        p = QCoreApplication::applicationDirPath() + "/data";
    QDir().mkpath(p);
    return p;
}
inline QString data(const QString &p) { return QDir(root()).filePath(p); }
inline QString scripts() {
    auto p = qEnvironmentVariable("ZTL_MANAGER_SCRIPTS_DIR");
    return p.isEmpty() ? QCoreApplication::applicationDirPath() + "/scripts" : p;
}
inline QString python() {
    auto p = qEnvironmentVariable("ZTL_MANAGER_PYTHON");
    return p.isEmpty() ? QCoreApplication::applicationDirPath() + "/runtime/python.exe" : p;
}
inline void initialize() {
    for (auto p : {"tmp", "logs", "config", "downloads", "releases"})
        QDir().mkpath(data(p));
    for (auto key : {"TEMP", "TMP", "TMPDIR"})
        qputenv(key, QDir::toNativeSeparators(data("tmp")).toUtf8());
}
} // namespace Paths
