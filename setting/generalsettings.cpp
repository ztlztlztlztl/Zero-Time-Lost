#include "generalsettings.h"
#include "autostart.h"
#include "settingcontrols.h"
#include "paths.h"
#include "configmanager.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"
GeneralSettings::GeneralSettings(Section section,QWidget *parent):QWidget(parent){
    retry.setInterval(5000);connect(&retry,&QTimer::timeout,this,[this]{QSettings pending;syncSettings(pending);});
    auto form=SettingControls::form(this);QSettings settings;
    auto toggle=[&](const QString &label,const QString &key,bool fallback){
        auto control=new QCheckBox;control->setObjectName(key);control->setChecked(settings.value(key,fallback).toBool());control->setText(control->isChecked()?"on":"off");
        form->addRow(SettingControls::label(label),control);
        connect(control,&QCheckBox::toggled,this,[this,control,key](bool enabled){control->setText(enabled?"on":"off");QSettings s;s.setValue(key,enabled);syncSettings(s);emit applied();});
    };
    auto number=[&](const QString &label,const QString &key,int fallback,int minimum,int maximum,const QString &suffix){
        auto control=new NoWheelSpinBox;control->setObjectName(key);control->setRange(minimum,maximum);control->setValue(settings.value(key,fallback).toInt());control->setSuffix(suffix);
        form->addRow(SettingControls::label(label),control);
        connect(control,&QSpinBox::valueChanged,this,[this,key](int value){QSettings s;s.setValue(key,value);syncSettings(s);emit applied();});
    };
    if(section==Home){
        auto key=new QLineEdit(settings.value("weatherKey").toString().trimmed());key->setObjectName("weatherKey");key->setEchoMode(QLineEdit::Password);
        key->setPlaceholderText("高德 Web 服务类型 Key");key->setToolTip("回车或移开焦点后应用。请使用高德 Web 服务类型 Key。");form->addRow(SettingControls::label("高德天气 Key"),key);
        connect(key,&QLineEdit::editingFinished,this,[this,key]{const auto value=key->text().trimmed();key->setText(value);QSettings s;if(s.value("weatherKey").toString()==value)return;s.setValue("weatherKey",value);syncSettings(s);emit applied();});
        toggle("背景动画","animation",true);
    }else if(section==DDL){
        number("静默开始","quietStart",23,0,23," 时");number("静默结束（相同=关闭）","quietEnd",7,0,23," 时");
    }else if(section==Note){
        number("秘密闲置关闭（0=关闭）","idleMinutes",5,0,120," 分钟");
        for(const auto &name:{"初始化／升级秘密加密","修改密码","备份","恢复备份"}){auto button=new SettingButton(name);form->addRow(button);connect(button,&QPushButton::clicked,this,[this,name]{emit noteAction(name);});}
    }else{
        toggle("关闭窗口时隐藏到托盘","closeToTray",true);
        auto startup=new QCheckBox("随 Windows 启动到托盘");startup->setObjectName("autostart");startup->setChecked(AutoStart::enabled());form->addRow(SettingControls::label("开机自启动"),startup);
        connect(startup,&QCheckBox::toggled,this,[this,startup](bool checked){QString error;if(!AutoStart::setEnabled(checked,&error)){QSignalBlocker b(startup);startup->setChecked(!checked);emit saveStatus(error);}});
        auto folder=new SettingButton("打开数据目录");form->addRow(folder);connect(folder,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::root()));});
    auto import=new SettingButton("从旧版本目录导入设置和离线邮件");form->addRow(import);
    connect(import,&QPushButton::clicked,this,[this]{
        const auto path=QFileDialog::getExistingDirectory(this,"选择含 config / database 的旧运行目录");if(path.isEmpty())return;
        if(QMessageBox::question(this,"导入旧数据","将替换首页/邮件/DDL偏好并合并账号、离线邮件。原目录不修改。继续？")!=QMessageBox::Yes)return;
        auto read=[](const QString &p){QFile file(p);if(!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024)return QJsonObject();return QJsonDocument::fromJson(file.readAll()).object();};
        auto settings=read(path+"/config/settings.json");
        if(!settings.isEmpty()){
            const auto home=settings["home"].toObject();
            ConfigManager::getInstance()->setHomeweather(HomeWeatherConfig(home["weather"].toObject()));
            ConfigManager::getInstance()->setMailBasical(MailBasicalConfig(settings["mail"].toObject()));
            ConfigManager::getInstance()->setDeadlineBasical(DeadlineBasicalConfig(settings["deadline"].toObject()));
        }
        int accounts=0;try{
            for(auto value:read(path+"/config/mails.json")["accounts"].toArray()){
                const auto account=MailAccount(value.toObject());if(account.address.isEmpty())continue;
                MailAccountManager::getInstance()->setMailAccount(account);++accounts;
            }
        }catch(...){QMessageBox::warning(this,"账号保护失败","账号导入中断，请检查 Windows 当前用户凭据保护");return;}
        bool mail=true;if(QFileInfo::exists(path+"/database/mail.db"))mail=MailManager::getInstance()->importLegacy(path+"/database/mail.db");
        emit applied();QMessageBox::information(this,"导入结果",QString("已合并 %1 个账号；离线邮件：%2。旧邮件不能按旧序号联网定位，将重新同步真实 UID。").arg(accounts).arg(mail?"完成/无文件":"失败"));
    });
    }
}
void GeneralSettings::syncSettings(QSettings &settings){
    settings.sync();
    if(settings.status()!=QSettings::NoError){
        if(!retry.isActive())emit saveStatus("设置文件保存失败，修改已保留在内存中；请检查数据目录权限和磁盘空间。程序将在 5 秒后重试，退出前请等待保存成功。");
        retry.start();
    }else if(retry.isActive()){retry.stop();emit saveStatus("设置已保存");}
}
