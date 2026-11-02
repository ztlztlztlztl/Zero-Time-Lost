#include "paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
QString Paths::root() {
    const QString overridePath = qEnvironmentVariable("ZTL_DATA_DIR");
    const QString path = overridePath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : overridePath;
    QDir().mkpath(path);
    return path;
}
QString Paths::data(const QString &relative) {
    QString clean = relative;
    if (clean.startsWith("./")) clean.remove(0, 2);
    return QDir(root()).filePath(clean);
}
QString Paths::scripts() {
    const QString configured = qEnvironmentVariable("ZTL_SCRIPTS_DIR");
    return configured.isEmpty() ? QCoreApplication::applicationDirPath() + "/scripts" : configured;
}
QString Paths::python() {
    // Explicit overrides remain for developer tests; end users always receive the bundled runtime.
    const auto configured = qEnvironmentVariable("ZTL_PYTHON");
    if (!configured.isEmpty()) return configured;
    return QCoreApplication::applicationDirPath() + "/runtime/python.exe";
}
