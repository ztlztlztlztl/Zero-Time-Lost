#include "managerwindow.h"
#include "paths.h"
#include <QtWidgets>
namespace {
QLineEdit *edit(const char *name, const QString &hint, bool secret = false) {
    auto p = new QLineEdit;
    p->setObjectName(name);
    p->setPlaceholderText(hint);
    if (secret)
        p->setEchoMode(QLineEdit::Password);
    return p;
}
QLabel *label(const QString &s) {
    auto p = new QLabel(s);
    p->setWordWrap(true);
    p->setTextFormat(Qt::PlainText);
    return p;
}
} // namespace
ManagerWindow::ManagerWindow(QWidget *parent) : QWidget(parent), worker(this), observer(this) {
    setWindowTitle("Z.T.L · 服务器维护");
    resize(920, 900);
    setMinimumSize(700, 650);
    setStyleSheet("QWidget{background:#11161c;color:#dce5ee;} "
                  "QLineEdit,QSpinBox,QComboBox,QPlainTextEdit{background:#19232d;border:1px solid "
                  "#496474;padding:6px;border-radius:4px;} QPushButton{background:#243746;border:1px solid "
                  "#496474;padding:7px 14px;border-radius:4px;} QPushButton:disabled{color:#63717c;} "
                  "QGroupBox{border:1px solid #344956;margin-top:12px;padding-top:12px;} "
                  "QCheckBox::indicator{width:16px;height:16px;border:1px solid #78909e;} "
                  "QCheckBox::indicator:checked{background:#36b6a2;} QScrollArea{border:0;}");
    auto outer = new QVBoxLayout(this);
    auto title = label("服务器部署与维护");
    auto f = title->font();
    f.setPointSize(20);
    f.setBold(true);
    title->setFont(f);
    outer->addWidget(title);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    controls = new QWidget;
    scroll->setWidget(controls);
    outer->addWidget(scroll, 3);
    auto body = new QVBoxLayout(controls);
    auto server = new QGroupBox("SSH 连接 · Ubuntu 22.04 / 24.04");
    auto form = new QFormLayout(server);
    host = edit("managerHost", "服务器 IPv4 或 IPv6");
    port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(22);
    password = edit("managerPassword", "root 密码，仅本次使用", true);
    keyFile = edit("managerKey", "可选：SSH 私钥文件");
    passphrase = edit("managerPassphrase", "私钥口令，仅本次使用", true);
    imagePrefix = edit("managerImagePrefix", "可选：registry.example.com/library，不含协议");
    form->addRow("服务器地址", host);
    form->addRow("SSH 端口", port);
    form->addRow("用户", label("root"));
    form->addRow("密码", password);
    auto keyRow = new QHBoxLayout;
    keyRow->addWidget(keyFile);
    auto keyButton = new QPushButton("选择私钥");
    keyRow->addWidget(keyButton);
    form->addRow("私钥", keyRow);
    form->addRow("私钥口令", passphrase);
    form->addRow("镜像源前缀", imagePrefix);
    body->addWidget(server);
    connect(keyButton, &QPushButton::clicked, this, [this] {
        auto p = QFileDialog::getOpenFileName(this, "选择 SSH 私钥");
        if (!p.isEmpty())
            keyFile->setText(p);
    });
    auto source = new QGroupBox("独立服务端发布包");
    auto sf = new QFormLayout(source);
    release = edit("managerRelease", "选择 ztl-server-版本.zip");
    auto rr = new QHBoxLayout;
    rr->addWidget(release);
    auto choose = new QPushButton("选择并校验");
    choose->setObjectName("chooseRelease");
    rr->addWidget(choose);
    sf->addRow("本地包", rr);
    url = edit("managerReleaseURL", "可选：发布包 HTTPS 下载地址");
    sha = edit("managerReleaseSHA", "可选：外部 SHA-256");
    sf->addRow("下载地址", url);
    sf->addRow("SHA-256", sha);
    auto download = new QPushButton("下载并校验发布包");
    sf->addRow(download);
    releaseInfo = label("维护工具不携带固定版本的服务端程序");
    sf->addRow(releaseInfo);
    body->addWidget(source);
    connect(choose, &QPushButton::clicked, this, [this] {
        auto p = QFileDialog::getOpenFileName(this, "选择服务端发布包", {}, "ZIP 发布包 (*.zip)");
        if (!p.isEmpty()) {
            release->setText(p);
            start("inspect-release");
        }
    });
    connect(download, &QPushButton::clicked, this, [this] { start("download-release"); });
    connect(release, &QLineEdit::textChanged, this, [this] {
        releaseReady = false;
        releaseInfo->setText("发布包尚未校验");
    });
    connect(release, &QLineEdit::editingFinished, this, [this] {
        if (!release->text().isEmpty())
            start("inspect-release");
    });
    auto operations = new QGroupBox("操作");
    auto op = new QVBoxLayout(operations);
    auto detect = new QPushButton("检测服务器与 SSH 指纹");
    detect->setObjectName("managerScan");
    op->addWidget(detect);
    summary = label("首次连接先检测，再核对目标地址与指纹。");
    summary->setObjectName("managerSummary");
    summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    op->addWidget(summary);
    trust = new QCheckBox("已核对目标地址与 SSH 指纹");
    trust->setObjectName("managerTrust");
    op->addWidget(trust);
    mode = new QComboBox;
    mode->setObjectName("managerMode");
    mode->addItems({"日常部署／更新（保留数据，不清理其他业务）", "首次部署（专用服务器，自动清理旧业务）",
                    "清除旧 Z.T.L 后重建（删除云端数据）"});
    op->addWidget(mode);
    auto deploy = new QPushButton("部署所选发布包");
    deploy->setObjectName("managerDeploy");
    op->addWidget(deploy);
    auto row = new QHBoxLayout;
    for (auto pair :
         {qMakePair(QString("status"), QString("服务状态")), qMakePair(QString("logs"), QString("查看日志")),
          qMakePair(QString("restart"), QString("重启服务")),
          qMakePair(QString("backup"), QString("创建备份")),
          qMakePair(QString("list-backups"), QString("刷新备份列表"))}) {
        auto b = new QPushButton(pair.second);
        b->setObjectName("manager_" + pair.first);
        row->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, pair] { start(pair.first); });
    }
    op->addLayout(row);
    auto restoreRow = new QHBoxLayout;
    backups = new QComboBox;
    backups->setObjectName("managerBackups");
    restoreRow->addWidget(backups, 1);
    auto restore = new QPushButton("恢复所选备份");
    restore->setObjectName("managerRestore");
    restoreRow->addWidget(restore);
    op->addLayout(restoreRow);
    auto exportButton = new QPushButton("导出客户端连接文件");
    exportButton->setObjectName("managerExport");
    op->addWidget(exportButton);
    body->addWidget(operations);
    body->addStretch();
    connect(detect, &QPushButton::clicked, this, [this] { start("scan"); });
    connect(deploy, &QPushButton::clicked, this, [this] { start("deploy"); });
    connect(restore, &QPushButton::clicked, this, [this] { start("restore"); });
    connect(exportButton, &QPushButton::clicked, this, [this] { start("export-connection"); });
    auto monitoring = new QHBoxLayout;
    for (const auto &entry : QList<QPair<QString, QString>>{{"status", "查询远端操作进度"},
                                                            {"cancel-operation", "请求停止预检／构建"}}) {
        auto button = new QPushButton(entry.second);
        monitoring->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, entry] {
            if (observer.busy())
                return;
            if (plan.isEmpty() || !trust->isChecked()) {
                append("请先检测并确认服务器 SSH 指纹");
                return;
            }
            observer.start({{"action", entry.first},
                            {"connection", connection()},
                            {"fingerprint", plan["fingerprint"]}});
        });
    }
    outer->addLayout(monitoring);
    connect(&observer, &BootstrapProcess::progress, this, &ManagerWindow::append);
    connect(&observer, &BootstrapProcess::completed, this,
            [this](const QJsonObject &result, const QString &error) {
                if (!error.isEmpty()) {
                    append(error);
                    return;
                }
                const auto operation = result["operation"].toObject();
                if (!operation.isEmpty()) {
                    append("远端操作：" + operation["id"].toString() + " · " + operation["phase"].toString() +
                           " · " + operation["state"].toString());
                    if (operation.contains("cancelAccepted"))
                        append(operation["cancelAccepted"].toBool()
                                   ? "已请求停止，等待远端释放锁"
                                   : "当前步骤不能安全取消，将继续执行至检查点");
                } else
                    append("远端没有正在执行的维护步骤");
            });
    log = new QPlainTextEdit;
    log->setReadOnly(true);
    log->setObjectName("managerLog");
    log->setMaximumBlockCount(1000);
    log->setMinimumHeight(140);
    outer->addWidget(log, 1);
    connect(&worker, &BootstrapProcess::progress, this, &ManagerWindow::append);
    connect(&worker, &BootstrapProcess::completed, this, &ManagerWindow::completed);
    QSettings s;
    host->setText(s.value("server/host").toString());
    port->setValue(s.value("server/port", 22).toInt());
    keyFile->setText(s.value("server/key").toString());
    imagePrefix->setText(s.value("server/imagePrefix").toString());
    release->setText(s.value("release/path").toString());
    url->setText(s.value("release/url").toString());
    auto invalidate = [this] {
        plan = {};
        profile = {};
        trust->setChecked(false);
        summary->setText("连接目标已改变，请重新检测");
    };
    connect(host, &QLineEdit::textChanged, this, invalidate);
    connect(port, &QSpinBox::valueChanged, this, invalidate);
}
QJsonObject ManagerWindow::connection() const {
    return {{"host", host->text().trimmed()},
            {"sshPort", port->value()},
            {"username", "root"},
            {"password", keyFile->text().isEmpty() ? password->text() : QString()},
            {"keyFile", keyFile->text().trimmed()},
            {"passphrase", passphrase->text()}};
}
void ManagerWindow::saveDraft() {
    QSettings s;
    s.setValue("server/host", host->text().trimmed());
    s.setValue("server/port", port->value());
    s.setValue("server/key", keyFile->text());
    s.setValue("server/imagePrefix", imagePrefix->text());
    s.setValue("release/path", release->text());
    s.setValue("release/url", url->text());
    s.sync();
}
void ManagerWindow::append(const QString &s) {
    log->appendPlainText(s);
    const auto path = Paths::data("logs/manager.log");
    QFile f(path);
    if (f.size() > 2 * 1024 * 1024) {
        QFile::remove(path + ".previous");
        QFile::rename(path, path + ".previous");
    }
    if (f.open(QIODevice::Append))
        f.write((QDateTime::currentDateTime().toString(Qt::ISODate) + " " + s + "\n").toUtf8());
}
void ManagerWindow::start(const QString &name) {
    if (!working && name == "export-connection" && !profile["token"].toString().isEmpty()) {
        chooseExportFile();
        return;
    }
    if (working)
        return;
    QJsonObject r{{"action", name}};
    const bool local = name == "inspect-release" || name == "download-release";
    if (!local) {
        if (host->text().trimmed().isEmpty() || (password->text().isEmpty() && keyFile->text().isEmpty())) {
            append("请填写服务器地址与 root SSH 密码或私钥");
            return;
        }
        r["connection"] = connection();
        if (name != "scan") {
            if (plan.isEmpty() || !trust->isChecked()) {
                append("请先检测服务器并核对 SSH 指纹");
                return;
            }
            r["fingerprint"] = plan["fingerprint"];
        }
    }
    if (name == "scan") {
        plan = {};
        profile = {};
        trust->setChecked(false);
    }
    if (name == "inspect-release")
        r["release"] = release->text();
    if (name == "download-release") {
        r["url"] = url->text().trimmed();
        r["sha256"] = sha->text().trimmed();
    }
    if (name == "deploy") {
        if (!releaseReady) {
            append("请先选择并校验服务端发布包");
            return;
        }
        if (mode->currentIndex() > 0) {
            QString prompt = mode->currentIndex() == 2
                                 ? "将删除云端 Z.T.L 数据库、备份、令牌与证书，并清理旧业务，无法撤销。"
                                 : "此服务器须专用于 "
                                   "Z.T.L。将自动清理旧应用及业务数据，保留系统、SSH、运行环境和已有 Z.T.L。";
            if (QMessageBox::warning(this, "确认服务器操作", host->text() + "\n\n" + prompt,
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                return;
        }
        r["release"] = release->text();
        r["plan"] = plan;
        r["cleanupPolicy"] = mode->currentIndex() == 0 ? "none" : "dedicated";
        r["mode"] = mode->currentIndex() == 2 ? "reset" : "preserve";
        r["confirmReset"] = mode->currentIndex() == 2;
        r["imagePrefix"] = imagePrefix->text().trimmed();
    }
    if (name == "restore") {
        if (backups->currentText().isEmpty()) {
            append("请先刷新并选择备份");
            return;
        }
        if (QMessageBox::warning(
                this, "确认恢复",
                host->text() + "\n恢复 " + backups->currentText() +
                    " 将覆盖云端数据库；操作前会自动备份当前数据。客户端可能需要处理恢复后的同步状态。",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        r["backup"] = backups->currentText();
        r["confirmRestore"] = true;
    }
    if (name == "restart" &&
        QMessageBox::question(this, "重启服务", "重启期间连接将短暂中断，继续？",
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    saveDraft();
    action = name;
    working = true;
    controls->setEnabled(false);
    append("正在执行：" + name);
    worker.start(r);
}
void ManagerWindow::completed(const QJsonObject &r, const QString &error) {
    working = false;
    controls->setEnabled(true);
    if (!error.isEmpty()) {
        append(error);
        if (action == "deploy") {
            plan = {};
            profile = {};
            trust->setChecked(false);
        }
        return;
    }
    const auto operation =
        r["operation"].toObject().isEmpty() ? r["maintenance"].toObject() : r["operation"].toObject();
    if (!operation.isEmpty())
        append("远端操作：" + operation["id"].toString() + " · " + operation["phase"].toString() + " · " +
               operation["state"].toString());
    if (action == "scan") {
        plan = r;
        summary->setText("地址：" + host->text() + "\n系统：" + r["os"].toString() + "\nSSH：" +
                         r["fingerprint"].toString() + "\n" +
                         (r["existing"].toBool() ? "已有 Z.T.L，默认保留数据更新" : "尚未部署 Z.T.L"));
        append("检测完成，请核对目标与指纹");
    } else if (action == "inspect-release" || action == "download-release") {
        QSignalBlocker b(release);
        release->setText(r["release"].toString());
        releaseReady = true;
        releaseInfo->setText("已校验：" + r["version"].toString() + " · 协议 " +
                             QString::number(r["protocol"].toInt()));
        append(releaseInfo->text());
        saveDraft();
    } else if (action == "list-backups") {
        backups->clear();
        for (auto v : r["backups"].toArray())
            backups->addItem(v.toString());
        append("备份列表已更新");
    } else if (action == "export-connection" || action == "deploy") {
        profile = r;
        profile["format"] = 1;
        profile["protocol"] = 1;
        if (action == "deploy") {
            append("部署与 HTTPS 验证成功。可导出连接文件供客户端导入。");
            plan = {};
            trust->setChecked(false);
        } else
            chooseExportFile();
    } else {
        if (!r["details"].toString().isEmpty())
            append(r["details"].toString());
        if (!r["healthError"].toString().isEmpty())
            append(r["healthError"].toString());
        if (r["health"].isObject()) {
            const auto health = r["health"].toObject();
            append("HTTPS 正常 · 服务版本：" + health["serverVersion"].toString("未知"));
            const auto jobs = health["jobs"].toObject();
            for (auto it = jobs.begin(); it != jobs.end(); ++it)
                if (!it.value().toObject()["error"].toString().isEmpty())
                    append(it.key() + "：" + it.value().toObject()["error"].toString());
            append("存储统计：" +
                   QString::fromUtf8(
                       QJsonDocument(health["storage"].toObject()).toJson(QJsonDocument::Compact)));
        }
    }
}
void ManagerWindow::chooseExportFile() {
    auto p = QFileDialog::getSaveFileName(this, "连接文件包含访问令牌，请妥善保管", {},
                                          "Z.T.L 连接文件 (*.ztlconn)");
    if (!p.isEmpty())
        append(exportConnectionFile(p) ? "连接文件已导出，不含 SSH 密码"
                                       : "连接文件保存失败，请检查目标目录");
}
bool ManagerWindow::exportConnectionFile(const QString &path) {
    if (profile["token"].toString().isEmpty())
        return false;
    QJsonObject out;
    for (auto key : {"format", "protocol", "host", "port", "token", "ca"})
        out[key] = profile[key];
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    const auto data = QJsonDocument(out).toJson();
    return f.write(data) == data.size() && f.commit();
}
void ManagerWindow::closeEvent(QCloseEvent *e) {
    if (working) {
        if (QMessageBox::question(this, "退出维护工具",
                                  "服务器维护步骤可能仍在运行，并由服务器持锁保护。重新打开后请先查询远端进度"
                                  "，勿重复启动操作。退出本地窗口？",
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            saveDraft();
            e->accept();
        } else
            e->ignore();
    } else {
        saveDraft();
        e->accept();
    }
}
