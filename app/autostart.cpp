#include "autostart.h"
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
namespace {
const QString runKey="HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}
QString AutoStart::command(){return "\""+QDir::toNativeSeparators(QCoreApplication::applicationFilePath())+"\" --autostart";}
bool AutoStart::enabled(){
#ifdef Q_OS_WIN
    return QSettings(runKey,QSettings::NativeFormat).value("Z.T.L").toString()==command();
#else
    return false;
#endif
}
bool AutoStart::setEnabled(bool enabled,QString *error){
#ifdef Q_OS_WIN
    QSettings s(runKey,QSettings::NativeFormat);if(enabled)s.setValue("Z.T.L",command());else s.remove("Z.T.L");s.sync();
    if(s.status()==QSettings::NoError)return true;
    if(error)*error="无法修改 Windows 当前用户启动项";
#else
    Q_UNUSED(enabled)
    if(error)*error="当前平台不支持 Windows 自启动";
#endif
    return false;
}
