#include "cloudsettings.h"
#include "cloudconnection.h"
#include "cloudprofile.h"
#include "settingcontrols.h"
#include <QtWidgets>

CloudSettings::CloudSettings(QWidget *parent) : QWidget(parent) {
    auto form = SettingControls::form(this);
    QSettings settings;
    auto setup = new SettingButton("客户端配置向导");
    setup->setObjectName("openSetupWizard");
    form->addRow(setup);
    connect(setup, &QPushButton::clicked, CloudConnection::instance(), &CloudConnection::setupRequested);
    auto enabled = new QCheckBox("启用个人云端");
    enabled->setObjectName("cloudEnabled");
    enabled->setChecked(CloudConnection::enabled());
    form->addRow(enabled);
    auto host = new QLineEdit(settings.value("cloud/host").toString());
    host->setObjectName("cloudHost");
    host->setPlaceholderText("服务器 IP 或域名");
    form->addRow(SettingControls::label("云端地址"), host);
    auto port = new NoWheelSpinBox;
    port->setRange(443, 443);
    port->setValue(443);
    port->setEnabled(false);
    form->addRow(SettingControls::label("HTTPS 端口"), port);
    auto token = new QLineEdit;
    token->setEchoMode(QLineEdit::Password);
    token->setPlaceholderText("留空保留已保存令牌");
    form->addRow(SettingControls::label("访问令牌"), token);
    auto ca = new QLineEdit(settings.value("cloud/ca").toString());
    ca->setReadOnly(true);
    form->addRow(SettingControls::label("信任证书"), ca);
    auto choose = new SettingButton("选择证书");
    form->addRow(choose);
    connect(choose, &QPushButton::clicked, this, [this, ca] {
        auto path = QFileDialog::getOpenFileName(this, "选择云端证书", {}, "证书 (*.crt *.pem)");
        if (!path.isEmpty())
            ca->setText(path);
    });
    auto status =
        new QLabel("连接修改测试成功后一次应用；停用云端可直接在本机生效。尚未应用的修改不会影响当前连接。");
    status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);
    form->addRow(status);
    auto test = new SettingButton("测试并应用");
    test->setObjectName("cloudTestApply");
    form->addRow(test);
    auto attempt = QSharedPointer<quint64>::create(0);
    auto invalidateTest = [test, attempt] {
        ++*attempt;
        test->setEnabled(true);
    };
    for (auto field : {host, token, ca})
        connect(field, &QLineEdit::textChanged, this, invalidateTest);
    connect(enabled, &QCheckBox::toggled, this, [test, attempt](bool checked) {
        ++*attempt;
        test->setEnabled(true);
        test->setText(checked ? "测试并应用" : "停用云端");
    });
    test->setText(enabled->isChecked() ? "测试并应用" : "停用云端");
    connect(test, &QPushButton::clicked, this, [=] {
        const auto operation = ++*attempt;
        // Disabling is a local operation, including when credentials or TLS are unusable.
        if (!enabled->isChecked()) {
            QSettings saved;
            const auto before = saved.value("cloud/enabled");
            saved.setValue("cloud/enabled", false);
            saved.sync();
            if (saved.status() != QSettings::NoError) {
                saved.setValue("cloud/enabled", before);
                status->setText("停用设置保存失败，请检查本地数据目录");
                return;
            }
            CloudConnection::instance()->notifySettingsChanged();
            status->setText("个人云端已停用，原连接配置和待同步修改已保留");
            return;
        }
        CloudProfile candidate;
        candidate.host = host->text().trimmed();
        candidate.enabled = enabled->isChecked();
        candidate.token = token->text().trimmed();
        try {
            if (candidate.token.isEmpty())
                candidate.token = CloudProfile::current().token;
        } catch (...) {
            status->setText("无法读取原令牌，请重新填写");
            return;
        }
        if (!ca->text().isEmpty()) {
            QFile f(QFileInfo(ca->text()).isRelative() ? Paths::data(ca->text()) : ca->text());
            if (!f.open(QIODevice::ReadOnly) || f.size() > 128 * 1024) {
                status->setText("证书无法读取或过大");
                return;
            }
            candidate.ca = f.readAll();
        }
        QString error;
        candidate.url(&error, true);
        if (!error.isEmpty()) {
            status->setText(error);
            return;
        }
        test->setEnabled(false);
        status->setText("正在测试候选连接…");
        QPointer<CloudSettings> guard(this);
        CloudConnection::instance()->testProfile(
            candidate, [=](const QJsonObject &result, const QString &failure) {
                if (!guard || operation != *attempt)
                    return;
                test->setEnabled(true);
                if (!failure.isEmpty()) {
                    status->setText(failure + "；原连接设置保持有效");
                    return;
                }
                QString error;
                const auto before = CloudConnection::identity();
                if (!candidate.apply(&error)) {
                    status->setText(error);
                    return;
                }
                token->clear();
                ca->setText(QSettings().value("cloud/ca").toString());
                host->setText(QSettings().value("cloud/host").toString());
                if (before != CloudConnection::identity())
                    CloudConnection::instance()->notifySettingsChanged();
                status->setText(CloudProfile::healthText(result));
            });
    });
    for (const auto &action : QStringList{"立即同步", "处理同步冲突", "服务器恢复后重新协商同步"}) {
        auto button = new SettingButton(action);
        if (action == "服务器恢复后重新协商同步")
            button->setObjectName("cloudReconnect");
        form->addRow(button);
        connect(button, &QPushButton::clicked, this, [this, action] {
            auto connection = CloudConnection::instance();
            if (action == "立即同步")
                emit connection->syncRequested();
            else if (action == "处理同步冲突")
                emit connection->conflictsRequested();
            else if (QMessageBox::question(this, "重新协商同步",
                                           "确认服务器已恢复备份或更换数据集后使用。将备份同步状态，并保留本"
                                           "地数据和未提交修改。继续？") == QMessageBox::Yes)
                emit connection->reconnectRequested();
        });
    }
    connect(CloudConnection::instance(), &CloudConnection::status, status, &QLabel::setText);
    connect(CloudConnection::instance(), &CloudConnection::settingsChanged, this, [=] {
        QSettings s;
        enabled->setChecked(s.value("cloud/enabled", false).toBool());
        host->setText(s.value("cloud/host").toString());
        ca->setText(s.value("cloud/ca").toString());
    });
}
