#include "cloudsettings.h"
#include "cloudconnection.h"
#include "cryptotool.h"
#include "settingcontrols.h"
#include "paths.h"
#include <QtWidgets>
#include <QSslCertificate>

CloudSettings::CloudSettings(QWidget *parent):QWidget(parent){
    auto form=SettingControls::form(this);QSettings settings;
    auto enabled=new QCheckBox("启用个人云端");enabled->setChecked(CloudConnection::enabled());form->addRow(enabled);
    auto host=new QLineEdit(settings.value("cloud/host").toString());host->setPlaceholderText("服务器 IP 或域名");host->setObjectName("cloudHost");form->addRow(SettingControls::label("云端地址"),host);
    auto port=new NoWheelSpinBox;port->setRange(1,65535);port->setValue(settings.value("cloud/port",443).toInt());form->addRow(SettingControls::label("HTTPS 端口"),port);
    auto token=new QLineEdit;token->setEchoMode(QLineEdit::Password);token->setPlaceholderText(settings.value("cloud/token").toString().isEmpty()?"填写部署时生成的访问令牌":"已保存；填写新令牌可替换");form->addRow(SettingControls::label("访问令牌"),token);
    auto ca=new QLineEdit(settings.value("cloud/ca").toString());ca->setReadOnly(true);ca->setPlaceholderText("IP 私有证书：选择部署提供的 .crt 文件");form->addRow(SettingControls::label("信任证书"),ca);
    auto choose=new SettingButton("选择证书");form->addRow(choose);
    auto status=new QLabel("离线修改保存在本机；启用后会将纸条、日历及邮件设置同步到个人服务器。");status->setWordWrap(true);form->addRow(status);
    auto test=new SettingButton("测试连接");auto sync=new SettingButton("立即同步");auto conflicts=new SettingButton("处理同步冲突");
    for(auto button:{test,sync,conflicts})form->addRow(button);
    auto persist=[=]{QSettings s;s.setValue("cloud/host",host->text().trimmed());s.setValue("cloud/port",port->value());s.setValue("cloud/ca",ca->text().trimmed());
        if(!token->text().isEmpty()){
            const auto encrypted=CryptoTool::protect(token->text().trimmed());if(encrypted.isEmpty()){status->setText("令牌保存失败，请重试");return;}
            s.setValue("cloud/token",encrypted);token->clear();token->setPlaceholderText("已保存；填写新令牌可替换");
        }
        s.setValue("cloud/enabled",enabled->isChecked());s.sync();if(s.status()!=QSettings::NoError){status->setText("保存失败，请检查数据目录");return;}status->setText("设置已应用");CloudConnection::instance()->notifySettingsChanged();
    };
    connect(host,&QLineEdit::editingFinished,this,persist);connect(token,&QLineEdit::editingFinished,this,persist);connect(port,&QSpinBox::valueChanged,this,persist);connect(enabled,&QCheckBox::toggled,this,persist);
    connect(choose,&QPushButton::clicked,this,[this,ca,persist]{auto source=QFileDialog::getOpenFileName(this,"选择云端证书",{},"证书 (*.crt *.pem)");if(source.isEmpty())return;
        auto certs=QSslCertificate::fromPath(source);if(certs.isEmpty()){QMessageBox::warning(this,"证书无效","所选文件不包含有效证书");return;}
        auto destination=Paths::data("cloud/server-ca.crt");QDir().mkpath(QFileInfo(destination).absolutePath());QFile input(source);QSaveFile output(destination);
        if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly)){QMessageBox::warning(this,"保存失败","无法复制证书到 data/cloud");return;}
        auto bytes=input.readAll();if(output.write(bytes)!=bytes.size()||!output.commit()){QMessageBox::warning(this,"保存失败","证书未保存");return;}ca->setText("cloud/server-ca.crt");persist();
    });
    connect(test,&QPushButton::clicked,this,[status]{QPointer<QLabel> label(status);CloudConnection::instance()->request("/health",{},[label](const QJsonObject &r,const QString &error){if(label)label->setText(error.isEmpty()&&r["protocol"]==1?"连接成功，个人云端运行正常":error.isEmpty()?"服务器协议不兼容":error);});});
    connect(sync,&QPushButton::clicked,CloudConnection::instance(),&CloudConnection::syncRequested);
    connect(conflicts,&QPushButton::clicked,CloudConnection::instance(),&CloudConnection::conflictsRequested);
    connect(CloudConnection::instance(),&CloudConnection::status,status,&QLabel::setText);
}
