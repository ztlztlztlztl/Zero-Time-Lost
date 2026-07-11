#include "mainwindow.h"
#include "logger.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    qInstallMessageHandler(ztlMessageOutput);
    QFile currentLog("./ztl_running.log");
    if (currentLog.size() > 5 * 1024 * 1024) { // 超过 5MB
        QFile::remove("ztl_old.log");
        currentLog.rename("ztl_old.log");
        qDebug() << "创建新一篇 ZTL 日志";
    }
    qInfo() << "=====================================================";
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
