#include "uipalette.h"
#include "uistyle.h"
#include "setupwizard.h"
#include "cloudconnection.h"
#include "cloudprofile.h"
#include "configmanager.h"
#include "connectionprofile.h"
#include "cryptotool.h"
#include "mailaccountmanager.h"
#include "paths.h"
#include "vaultcodec.h"
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QtWidgets>
namespace {
QLabel *text(const QString &s) {
    auto l = new QLabel(s);
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    return l;
}
QLineEdit *input(const QString &name, const QString &hint, bool secret = false) {
    auto e = new QLineEdit;
    e->setObjectName(name);
    e->setPlaceholderText(hint);
    if (secret)
        e->setEchoMode(QLineEdit::Password);
    return e;
}
bool flush(QObject *o) {
    for (auto e : o->findChildren<HelpJsonIOEngine *>())
        if (!e->flush())
            return false;
    return true;
}
} // namespace
bool SetupWizard::completed() { return QSettings().value("setup/completed", false).toBool(); }
bool SetupWizard::required() { return !completed(); }
SetupWizard::SetupWizard(bool firstRun, QWidget *parent)
    : QDialog(parent), initial(firstRun), vault(Paths::data("vault.ztlvault")), notes(&vault, this) {
    setObjectName("setupWizard");
    setWindowTitle("Z.T.L · 客户端初始化");
    resize(800, 720);
    setMinimumSize(650, 560);
    setStyleSheet(UiStyle::sheet+UiPalette::style(
        "QDialog{background:#11161c;color:#e6edf3;} QLabel,QCheckBox,QGroupBox{color:#dce5ee;} "
        "QLineEdit,QSpinBox{background:#19232d;color:#edf4fa;border:1px solid "
        "#496474;padding:7px;border-radius:5px;} QPushButton{background:#243746;color:#edf4fa;padding:8px "
        "18px;border:1px solid #496474;border-radius:5px;} QPushButton:disabled{color:#63717c;} "
        "QGroupBox{border:1px solid #344956;margin-top:12px;padding-top:12px;} "
        "QScrollArea{border:0;background:transparent;}"));
    if (initial) {
        ConfigManager::getInstance()->initialize();
        MailAccountManager::getInstance()->initialize();
    }
    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(26, 22, 26, 20);
    auto brand=new QLabel("Z.T.L");brand->setStyleSheet(UiPalette::style("font:24px 'Bahnschrift';color:#80BECE;background:transparent;"));outer->addWidget(brand);
    auto progress=new QHBoxLayout;progress->setSpacing(20);
    const QStringList captions{"1  个人设置","2  连接服务器","3  完成"};
    for(int i=0;i<captions.size();++i){auto label=text(captions[i]);label->setObjectName("setupStep"+QString::number(i));steps.append(label);progress->addWidget(label,1);}
    outer->addLayout(progress);
    heading = text("");
    heading->setProperty("role","title");
    auto f = heading->font();
    f.setPointSize(19);
    f.setBold(true);
    heading->setFont(f);
    outer->addWidget(heading);
    pages = new QStackedWidget;
    outer->addWidget(pages, 1);
    auto makePage = [this] {
        auto scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto body = new QWidget;
        body->setStyleSheet(UiPalette::style("background:transparent;"));
        scroll->setWidget(body);
        pages->addWidget(scroll);
        auto l = new QVBoxLayout(body);
        l->setSpacing(12);
        return l;
    };
    auto basics = makePage();
    basics->addWidget(text("先设置 Note 主密码，再连接个人服务器。天气和邮箱可以稍后补充。"));
    auto secretGroup = new QGroupBox("Note 主密码 · 必填");
    auto secretForm = new QFormLayout(secretGroup);
    password = input("setupPassword", "至少 10 个字符", true);
    confirmation = input("setupConfirm", "再次输入主密码", true);
    secretForm->addRow("主密码", password);
    secretForm->addRow("确认密码", confirmation);
    secretForm->addRow(text("当前秘密密库只保存在本机；主密码不上传服务器。请自行记住，程序没有找回功能。"));
    basics->addWidget(secretGroup);
    auto weather = new QGroupBox("天气 · 可稍后配置");
    auto weatherForm = new QFormLayout(weather);
    weatherKey = input("setupWeatherKey", "高德 Web 服务类型 Key", true);
    cityName = input("setupCityName", "例如：北京海淀");
    cityCode = input("setupCityCode", "例如：110108");
    weatherForm->addRow("高德 Key", weatherKey);
    weatherForm->addRow("城市名称", cityName);
    weatherForm->addRow("城市行政区编码", cityCode);
    basics->addWidget(weather);
    auto mail = new QGroupBox("邮箱 · 可稍后配置更多账号");
    auto mailForm = new QFormLayout(mail);
    mailName = input("setupMailName", "支持中文");
    mailAddress = input("setupMailAddress", "邮箱地址");
    mailCode = input("setupMailCode", "邮箱提供的 IMAP 授权码", true);
    mailServer = input("setupMailServer", "例如：imap.qq.com");
    mailPort = new QSpinBox;
    mailPort->setRange(1, 65535);
    mailPort->setValue(993);
    mailForm->addRow("账号名", mailName);
    mailForm->addRow("邮箱地址", mailAddress);
    mailForm->addRow("IMAP 服务器", mailServer);
    mailForm->addRow("TLS 端口", mailPort);
    mailForm->addRow("授权码", mailCode);
    basics->addWidget(mail);

    auto server = makePage();
    server->addWidget(
        text("请先使用独立的 Z.T.L 服务器维护工具部署服务，再导入连接文件，或手动填写以下信息。本程序仅通过 "
             "HTTPS 使用云端。"));
    auto importButton = new QPushButton("导入服务器连接文件");
    importButton->setObjectName("setupImport");
    server->addWidget(importButton);
    auto form = new QFormLayout;
    host = input("setupHost", "服务器 IP 或域名");
    token = input("setupToken", "访问令牌；已保存时留空沿用", true);
    caPath = input("setupCA", "私有 CA 证书路径；公有证书可留空");
    form->addRow("服务器地址", host);
    form->addRow("HTTPS 端口", text("443"));
    form->addRow("访问令牌", token);
    auto row = new QHBoxLayout;
    row->addWidget(caPath);
    auto choose = new QPushButton("选择证书");
    row->addWidget(choose);
    form->addRow("信任证书", row);
    server->addLayout(form);
    server->addStretch();
    connect(importButton, &QPushButton::clicked, this, [this] {
        auto p = QFileDialog::getOpenFileName(this, "导入服务器连接文件", {}, "连接文件 (*.ztlconn *.json)");
        if (!p.isEmpty())
            importProfileFile(p);
    });
    connect(choose, &QPushButton::clicked, this, [this] {
        auto p = QFileDialog::getOpenFileName(this, "选择 CA 证书", {}, "证书 (*.crt *.pem)");
        if (!p.isEmpty())
            caPath->setText(p);
    });
    auto done = makePage();
    done->addWidget(text("服务器连接已验证。完成后进入 "
                         "Z.T.L；普通纸条、日历及共享设置将按现有同步规则处理，秘密密库仅保存在本机。"));
    done->addStretch();
    message = text("");
    message->setObjectName("setupMessage");
    outer->addWidget(message);
    auto buttons = new QHBoxLayout;
    cancel = new QPushButton(initial ? "退出，稍后继续" : "关闭");
    back = new QPushButton("上一步");
    forward = new QPushButton("下一步");
    forward->setProperty("role","primary");
    forward->setObjectName("setupNext");
    buttons->addWidget(cancel);
    buttons->addStretch();
    buttons->addWidget(back);
    buttons->addWidget(forward);
    outer->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, this, &SetupWizard::reject);
    connect(back, &QPushButton::clicked, this, [this] { page(step == 2 ? 1 : 0); });
    connect(forward, &QPushButton::clicked, this, &SetupWizard::next);
    QSettings s;
    host->setText(s.value("cloud/host", s.value("setup/host")).toString());
    caPath->setText(s.value("cloud/ca").toString());
    weatherKey->setText(s.value("weatherKey").toString());
    const auto w = ConfigManager::getInstance()->getHomeweather();
    if (!w.savedCities.isEmpty()) {
        const auto city = w.savedCities[qBound(0, w.currentCityIndex, w.savedCities.size() - 1)];
        cityName->setText(city.name);
        cityCode->setText(city.code);
    }
    if (vault.exists()) {
        password->setEnabled(false);
        confirmation->setEnabled(false);
        password->setPlaceholderText("密库已创建，保留现有主密码");
    }
    page(0);
}
void SetupWizard::page(int index) {
    step = index;
    const QColor stepColors[]{UiPalette::Home,UiPalette::Settings,UiPalette::Note};
    for(int i=0;i<steps.size();++i)steps[i]->setStyleSheet(QString("background:transparent;color:%1;border-bottom:2px solid %2;padding:8px 0;").arg((i==index?UiPalette::Text:UiPalette::Muted).name(),(i==index?stepColors[i]:UiPalette::Line).name()));
    pages->setCurrentIndex(index);
    message->clear();
    back->setEnabled(index > 0);
    heading->setText(QStringList{"本地初始化", "连接个人服务器", "准备完成"}[index]);
    forward->setText(index == 2 ? "完成并进入 Z.T.L" : index == 1 ? "验证连接" : "下一步");
}
void SetupWizard::setBusy(bool busy) {
    working = busy;
    pages->setEnabled(!busy);
    back->setEnabled(!busy && step > 0);
    forward->setEnabled(!busy);
    cancel->setEnabled(!busy);
}
bool SetupWizard::importProfileFile(const QString &path) {
    QString error;
    const auto p = ConnectionProfile::read(path, &error);
    if (p.isEmpty()) {
        message->setText(error);
        return false;
    }
    host->setText(p["host"].toString());
    token->setText(p["token"].toString());
    importedCa = p["ca"].toString().toUtf8();
    caPath->setText(importedCa.isEmpty() ? "" : "[已导入 CA 证书]");
    message->setText("连接信息已导入，验证成功后保存");
    return true;
}
bool SetupWizard::saveBasics() {
    if (!vault.exists()) {
        if (password->text().size() < 10) {
            message->setText("请设置至少 10 个字符的 Note 主密码");
            return false;
        }
        if (password->text() != confirmation->text()) {
            message->setText("两次主密码不一致");
            return false;
        }
        if (!notes.create(password->text())) {
            message->setText(notes.error());
            return false;
        }
        notes.lock();
        password->clear();
        confirmation->clear();
        password->setEnabled(false);
        confirmation->setEnabled(false);
        password->setPlaceholderText("密库已创建，保留现有主密码");
    }
    if (!VaultCodec::ready(vault.read())) {
        message->setText("已有密库无法读取，请先检查 data/ 或从备份恢复；向导不会覆盖已有密库");
        return false;
    }
    const auto name = cityName->text().trimmed(), code = cityCode->text().trimmed();
    if (name.isEmpty() != code.isEmpty() ||
        (!code.isEmpty() && !QRegularExpression("^[0-9]{6}$").match(code).hasMatch())) {
        message->setText("城市名称和六位行政区编码请一起填写，或一起留空");
        return false;
    }
    const bool hasMail = !mailAddress->text().trimmed().isEmpty() || !mailCode->text().isEmpty() ||
                         !mailServer->text().trimmed().isEmpty() || !mailName->text().trimmed().isEmpty();
    if (hasMail && (!mailAddress->text().contains('@') || mailCode->text().isEmpty() ||
                    mailServer->text().trimmed().isEmpty())) {
        message->setText("添加邮箱请完整填写地址、IMAP 服务器和授权码；也可以全部留空");
        return false;
    }
    QSettings s;
    s.setValue("weatherKey", weatherKey->text().trimmed());
    s.sync();
    if (s.status() != QSettings::NoError) {
        message->setText("天气 Key 保存失败");
        return false;
    }
    if (!code.isEmpty()) {
        auto weather = ConfigManager::getInstance()->getHomeweather();
        int found = -1;
        for (int i = 0; i < weather.savedCities.size(); ++i)
            if (weather.savedCities[i].code == code) {
                found = i;
                weather.savedCities[i].name = name;
                break;
            }
        if (found < 0) {
            found = weather.savedCities.size();
            weather.savedCities.append({name, code});
        }
        weather.currentCityIndex = found;
        ConfigManager::getInstance()->setHomeweather(weather);
    }
    if (hasMail) {
        MailAccount account;
        account.name = mailName->text().trimmed();
        if (account.name.isEmpty())
            account.name = mailAddress->text().trimmed();
        account.address = mailAddress->text().trimmed();
        account.code = mailCode->text();
        account.server = mailServer->text().trimmed();
        account.port = QString::number(mailPort->value());
        try {
            MailAccountManager::getInstance()->setMailAccount(account);
        } catch (...) {
            message->setText("邮箱授权码保护保存失败，请重试");
            return false;
        }
    }
    if (!flush(ConfigManager::getInstance()) || !flush(MailAccountManager::getInstance())) {
        message->setText("业务设置保存失败，请检查 data/ 后重试");
        return false;
    }
    return true;
}

void SetupWizard::next() {
    if (working)
        return;
    if (step == 0) {
        if (saveBasics())
            page(1);
    } else if (step == 1)
        requestExisting();
    else
        finishSetup();
}
bool SetupWizard::saveCloud(const QJsonObject &r) {
    const auto clear = r["token"].toString();
    if (clear.isEmpty() || r["host"].toString().isEmpty()) {
        message->setText("服务器未返回有效连接信息");
        return false;
    }
    CloudProfile profile;profile.host=r["host"].toString();profile.token=clear;profile.ca=r["ca"].toString().toUtf8();profile.enabled=QSettings().value("cloud/enabled",false).toBool();QString error;
    if(!profile.apply(&error)){message->setText(error);return false;}
    token->clear();
    token->setPlaceholderText("已保存，留空沿用");
    caPath->setText(QSettings().value("cloud/ca").toString());
    return true;
}
void SetupWizard::requestExisting() {
    auto raw = host->text().trimmed();
    QHostAddress address;
    if (address.setAddress(raw) && address.protocol() == QAbstractSocket::IPv6Protocol)
        raw = "[" + raw + "]";
    QUrl url(raw.contains("://") ? raw : "https://" + raw);
    if (!url.isValid() || url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty() ||
        url.hasQuery() || url.hasFragment() || (!url.path().isEmpty() && url.path() != "/")) {
        message->setText("请填写服务器 IP 或域名，HTTPS 端口固定为 443");
        return;
    }
    url.setPort(443);
    url.setPath("/health");
    auto clear = token->text().trimmed();
    if (clear.isEmpty())
        clear = CryptoTool::unprotect(QSettings().value("cloud/token").toString());
    if (clear.isEmpty()) {
        message->setText("请填写访问令牌");
        return;
    }
    QByteArray ca;
    const auto path = caPath->text().trimmed();
    if (path == "[已导入 CA 证书]")
        ca = importedCa;
    else if (!path.isEmpty()) {
        QFile file(QFileInfo(path).isRelative() ? Paths::data(path) : path);
        if (!file.open(QIODevice::ReadOnly) || (ca = file.readAll()).isEmpty() ||
            QSslCertificate::fromData(ca).isEmpty()) {
            message->setText("证书不存在或无效，请重新选择");
            return;
        }
    }
    CloudProfile candidate;candidate.host=url.host();candidate.token=clear;candidate.ca=ca;
    setBusy(true);message->setText("正在验证 HTTPS 和访问令牌…");QPointer<SetupWizard> guard(this);
    CloudConnection::instance()->testProfile(candidate,[guard,candidate](const QJsonObject &result,const QString &error){
        if(!guard)return;guard->setBusy(false);
        if(!error.isEmpty()){guard->message->setText(error);return;}
        if(guard->saveCloud({{"host",candidate.host},{"token",candidate.token},{"ca",QString::fromUtf8(candidate.ca)}}))guard->page(2);
    });
}

void SetupWizard::finishSetup() {
    QSettings s;
    if (!saveBasics() || s.value("cloud/host").toString().isEmpty() ||
        s.value("cloud/token").toString().isEmpty()) {
        message->setText("请先验证服务器连接");
        return;
    }
    s.setValue("cloud/enabled", true);
    s.setValue("setup/completed", true);
    s.sync();
    if (s.status() != QSettings::NoError) {
        message->setText("设置保存失败，请检查 data/ 目录");
        return;
    }
    notes.lock();
    accept();
}
void SetupWizard::reject() {
    if (!working) {
        notes.lock();
        QDialog::reject();
    }
}
void SetupWizard::closeEvent(QCloseEvent *e) {
    if (working)
        e->ignore();
    else
        QDialog::closeEvent(e);
}
