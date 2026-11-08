#include "generalsettings.h"
#include "autostart.h"
#include "settingcontrols.h"
#include "paths.h"
#include "cloudconnection.h"
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
        connect(key,&QLineEdit::editingFinished,this,[this,key]{const auto value=key->text().trimmed();key->setText(value);QSettings s;if(s.value("weatherKey").toString()==value)return;s.setValue("weatherKey",value);syncSettings(s);emit CloudConnection::instance()->syncRequested();emit applied();});
        toggle("界面动效","animation",true);
    }else if(section==DDL){
        number("静默开始","quietStart",23,0,23," 时");number("静默结束（相同=关闭）","quietEnd",7,0,23," 时");
    }else if(section==Note){
        number("秘密闲置关闭（0=关闭）","idleMinutes",5,0,120," 分钟");
        auto password=new SettingButton("初始化");password->setObjectName("secretPasswordAction");form->addRow(password);
        connect(password,&QPushButton::clicked,this,[this]{emit noteAction("密码设置");});
        for(const auto &name:{"备份","恢复备份"}){auto button=new SettingButton(name);form->addRow(button);connect(button,&QPushButton::clicked,this,[this,name]{emit noteAction(name);});}
    }else{
        toggle("关闭窗口时隐藏到托盘","closeToTray",true);
        auto startup=new QCheckBox("随 Windows 启动到托盘");startup->setObjectName("autostart");startup->setChecked(AutoStart::enabled());form->addRow(SettingControls::label("开机自启动"),startup);
        connect(startup,&QCheckBox::toggled,this,[this,startup](bool checked){QString error;if(!AutoStart::setEnabled(checked,&error)){QSignalBlocker b(startup);startup->setChecked(!checked);emit saveStatus(error);}});
        auto folder=new SettingButton("打开数据目录");form->addRow(folder);connect(folder,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::root()));});
        for(auto action:{"完整本地备份","恢复完整本地备份","存储统计","配置文件恢复"}){auto button=new SettingButton(action);form->addRow(button);connect(button,&QPushButton::clicked,this,[this,action]{emit noteAction(action);});}
        form->addRow(new QLabel("Z.T.L · "+QCoreApplication::applicationVersion()));
    }
}
void GeneralSettings::syncSettings(QSettings &settings){
    settings.sync();
    if(settings.status()!=QSettings::NoError){
        if(!retry.isActive())emit saveStatus("设置文件保存失败，修改已保留在内存中；请检查数据目录权限和磁盘空间。程序将在 5 秒后重试，退出前请等待保存成功。");
        retry.start();
    }else if(retry.isActive()){retry.stop();emit saveStatus("设置已保存");}
}
