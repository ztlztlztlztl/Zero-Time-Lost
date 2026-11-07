#pragma once
#include "paths.h"
#include <QtCore>
namespace ZtlLog {
struct Sink {
    QMutex mutex;
    QByteArray buffer;
    QFile file;
    void flush() {
        if (buffer.isEmpty())
            return;
        const auto path = Paths::data("logs/ztl_running.log");
        if (file.fileName() != path) {
            file.close();
            file.setFileName(path);
        }
        if (file.size() > 5 * 1024 * 1024) {
            file.close();
            QFile::remove(path + ".old");
            QFile::rename(path, path + ".old");
        }
        if (!file.isOpen() && !file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            return;
        const auto written = file.write(buffer);
        if (written > 0)
            buffer.remove(0, written);
        file.flush();
    }
};
inline Sink &sink() {
    static Sink value;
    return value;
}
inline void flush() {
    auto &s = sink();
    QMutexLocker lock(&s.mutex);
    s.flush();
}
} // namespace ZtlLog
inline void ztlMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    if (type == QtDebugMsg && !qEnvironmentVariableIsSet("ZTL_DEBUG_LOG"))
        return;
    auto &sink = ZtlLog::sink();
    QMutexLocker lock(&sink.mutex);
    auto line = (QDateTime::currentDateTime().toString(Qt::ISODate) + " [" + QString::number(type) + "] " +
                 QString::fromUtf8(context.category) + ": " + message + "\n")
                    .toUtf8();
    if (line.size() > 32768)
        line = line.left(32768) + " [truncated]\n";
    if (sink.buffer.size() + line.size() > 256 * 1024)
        sink.flush();
    if (sink.buffer.size() + line.size() <= 256 * 1024)
        sink.buffer.append(line);
    if (type >= QtWarningMsg || sink.buffer.size() >= 32768)
        sink.flush();
    if (type == QtFatalMsg) {
        QFile crash(Paths::data("crash/fatal-" +
                                QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + ".log"));
        if (crash.open(QIODevice::WriteOnly))
            crash.write(message.toUtf8());
        abort();
    }
}
inline void ztlInstallLogger() {
    qInstallMessageHandler(ztlMessageOutput);
    auto timer = new QTimer(qApp);
    timer->setInterval(1000);
    QObject::connect(timer, &QTimer::timeout, qApp, [] { ZtlLog::flush(); });
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, [] { ZtlLog::flush(); });
    timer->start();
}
