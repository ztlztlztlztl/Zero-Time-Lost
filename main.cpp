#include "mainwindow.h"
#include "appcontext.h"
#include "paths.h"
#include "logger.h"
#include <QApplication>
#include <QSettings>
#include <QLockFile>
#include <QMessageBox>
#include <QSurfaceFormat>
int main(int argc,char *argv[]){
    QCoreApplication::setOrganizationName("ZTL");QCoreApplication::setApplicationName("Sidekick");
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);QSurfaceFormat::setDefaultFormat(format);
    QApplication application(argc,argv);
    application.setFont(QFont("Microsoft YaHei UI",10));
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,Paths::root());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QLockFile single(Paths::data("application.lock"));single.setStaleLockTime(30000);
    if(!single.tryLock(100)){QMessageBox::information(nullptr,"副手","已有一个副手实例在运行，或数据目录被其他进程锁定。");return 1;}
    qInstallMessageHandler(ztlMessageOutput);
    AppContext launcher;
    MainWindow window(&launcher);
    QObject::connect(&application,&QCoreApplication::aboutToQuit,&launcher,&AppContext::shutdown);
    ProcessWorker mailCheck;
    if(application.arguments().contains("--self-check-mail")){
        launcher.clock.stop();
        QTimer::singleShot(0,&application,[&]{
            mailCheck.run({{"command","self-check"}},[&](const QJsonObject &result,const QString &error){
                const bool ok=error.isEmpty()&&result["data"].toObject()["adapter"].toInt()==1;
                if(!ok)qWarning()<<"Mail runtime self-check failed:"<<error;
                application.exit(ok?0:3);
            });
        });
    }else if(application.arguments().contains("--self-check")){
        launcher.clock.stop();
        QTimer::singleShot(0,&application,[&]{application.exit(launcher.tasks.ready()?0:2);});
    }else window.show();
    return application.exec();
}
