#include "mainwindow.h"
#include "appcontext.h"
#include "paths.h"
#include "logger.h"
#include "setupwizard.h"
#include "filebatch.h"
#include "dataguard.h"
#include "uistyle.h"
#include <QProcess>
#include <QApplication>
#include <QSettings>
#include <QLockFile>
#include <QMessageBox>
#include <QSurfaceFormat>
#include <QSystemTrayIcon>
int main(int argc,char *argv[]){
    QCoreApplication::setApplicationVersion(QStringLiteral(ZTL_PRODUCT_VERSION));
    QCoreApplication::setOrganizationName("ZTL");QCoreApplication::setApplicationName("Z.T.L");
    QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);QSurfaceFormat::setDefaultFormat(format);
    QApplication application(argc,argv);
    application.setFont(QFont("DengXian",11));
    application.setStyleSheet(UiStyle::sheet);
    const auto dataRoot=QDir::cleanPath(QFileInfo(qEnvironmentVariable("ZTL_DATA_DIR",QCoreApplication::applicationDirPath()+"/data")).absoluteFilePath());
    DataGuard maintenance(dataRoot);
    if(!maintenance.locked()){QMessageBox::information(nullptr,"Z.T.L","数据目录正在使用或执行备份／恢复，请稍后再启动。");return 1;}
    if(QFileInfo::exists(dataRoot+".restore.json")){
        const auto code=QProcess::execute(Paths::python(),{"-B",Paths::scripts()+"/local_backup.py","recover",dataRoot,"--startup-recovery"});
        if(code!=0){QMessageBox::warning(nullptr,"恢复尚未完成","无法完成上次数据恢复，原数据已保留。请检查安装目录权限及 data.previous 目录。");return 2;}
    }
    if(QFileInfo::exists(QCoreApplication::applicationDirPath()+"/.ztl-update.json")){QMessageBox::warning(nullptr,"程序更新未完成","请重新运行打包安装流程恢复上次更新，个人数据保持不变。");return 2;}
    Paths::initialize();
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,Paths::data("config"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QLockFile single(Paths::data("application.lock"));single.setStaleLockTime(30000);
    if(!single.tryLock(100)){QMessageBox::information(nullptr,"Z.T.L","已有一个 Z.T.L 实例在运行，或数据目录被其他进程锁定。");return 1;}
    if(!FileBatch::recover()){QMessageBox::warning(nullptr,"配置提交尚未完成","共享设置提交日志无法恢复。请检查 data/config 的文件权限、磁盘空间，修复后重新启动；原提交日志已保留。");return 2;}
    ztlInstallLogger();
    bool initializedNow=false;
    if(!application.arguments().contains("--self-check")&&!application.arguments().contains("--self-check-mail")&&SetupWizard::required()){
        application.setProperty("ztlSetupActive",true);
        application.setQuitOnLastWindowClosed(false);
        SetupWizard wizard(true);
        if(wizard.exec()!=QDialog::Accepted)return 0;
        initializedNow=true;
        application.setQuitOnLastWindowClosed(true);
        application.setProperty("ztlSetupActive",false);
    }
    AppContext launcher;
    MainWindow window(&launcher);
    QObject::connect(&application,&QCoreApplication::aboutToQuit,&window,&MainWindow::saveWindowState);
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
    }else if(initializedNow||!application.arguments().contains("--autostart")||!QSystemTrayIcon::isSystemTrayAvailable())window.show();
    const auto result=application.exec();launcher.shutdown();ZtlLog::flush();return result;
}
