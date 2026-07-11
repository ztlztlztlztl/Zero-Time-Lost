#ifndef LOGGER_H
#define LOGGER_H

#include <QMessageLogContext>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <iostream>

// 日志
void ztlMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 日志文件常驻
    static QFile logFile("ztl_running.log");
    static QTextStream logStream(&logFile);
    static bool isFileOpen = false;
    if (!isFileOpen) {
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            isFileOpen = true;
        }
    }
    // 时间戳
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    // 日志等级
    QString levelStr;
    switch (type) {
    case QtDebugMsg:    levelStr = "[DBUG]"; break;
    case QtInfoMsg:     levelStr = "[INFO]"; break;
    case QtWarningMsg:  levelStr = "[WARN]"; break;
    case QtCriticalMsg: levelStr = "[CRIT]"; break;
    case QtFatalMsg:    levelStr = "[FATAL]"; break;
    }

    // 日志行
    QString logLine = QString("%1 %2 %3\n").arg(timeStr).arg(levelStr).arg(msg);

    if (isFileOpen) {
        logStream << logLine;
        logStream.flush();
    }
    std::cout << logLine.toLocal8Bit().constData() << std::flush;
    if (type == QtFatalMsg) abort();
}












#endif // LOGGER_H
