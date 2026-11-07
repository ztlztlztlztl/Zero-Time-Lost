#include "processworker.h"
#include "paths.h"
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QJsonDocument>
#include <QFileInfo>
#include <memory>
ProcessWorker::~ProcessWorker() { stop(); }
void ProcessWorker::stop() {
    const auto active = running;
    running.clear();
    for (auto p : active) {
        p->disconnect(this);
        p->kill();
        p->waitForFinished(1500);
        delete p;
    }
}
void ProcessWorker::run(const QJsonObject &request, Callback callback) {
    if(!QFileInfo::exists(Paths::scripts()+"/mail_worker.py")){
        callback({},"缺少邮件脚本，请重新构建项目或使用完整部署包");return;
    }
    auto p = new QProcess(this);
    auto env=QProcessEnvironment::systemEnvironment();
    for(const auto &key:{"TEMP","TMP","TMPDIR"})env.insert(key,Paths::data("tmp"));
    env.insert("PYTHONDONTWRITEBYTECODE","1");p->setProcessEnvironment(env);p->setWorkingDirectory(Paths::root());
    running.insert(p);
    auto output = std::make_shared<QByteArray>();
    auto finished = std::make_shared<bool>(false);
    auto timeout = new QTimer(p);
    timeout->setSingleShot(true);
    auto finish = [this,p,callback,finished,timeout](QJsonObject obj, QString error) {
        if (*finished) return;
        *finished = true;
        timeout->stop();
        running.remove(p);
        if (p->state() != QProcess::NotRunning) { p->kill(); p->waitForFinished(500); }
        p->deleteLater();
        if(!error.isEmpty())qWarning()<<"[Mail]"<<error;
        callback(obj, error);
    };
    connect(p, &QProcess::started, this, [p,request] {
        p->write(QJsonDocument(request).toJson(QJsonDocument::Compact));
        p->closeWriteChannel();
    });
    connect(p, &QProcess::readyReadStandardOutput, this, [p,output,finish] {
        output->append(p->readAllStandardOutput());
        if (output->size() > 16 * 1024 * 1024) finish({}, "响应过大");
    });
    connect(p, &QProcess::readyReadStandardError, this, [p] { p->readAllStandardError(); });
    connect(p, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) finish({}, "内置邮件运行时缺失或无法启动，请重新构建项目或使用完整部署包");
        else if (e == QProcess::Crashed) finish({}, "邮件进程异常终止");
    });
    connect(timeout, &QTimer::timeout, this, [finish] { finish({}, "邮件请求超时，请重试"); });
    connect(p, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this,
            [p,output,finish](int code, QProcess::ExitStatus status) {
        output->append(p->readAllStandardOutput());
        if (output->size() > 16 * 1024 * 1024) return finish({}, "响应过大");
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(*output, &error);
        if (status != QProcess::NormalExit || code != 0 || error.error != QJsonParseError::NoError || !doc.isObject())
            return finish({}, "邮件脚本未返回有效响应");
        const auto obj = doc.object();
        if(obj["version"].toInt()!=1)return finish({},"邮件协议版本不兼容");
        finish(obj, obj["ok"].toBool() ? QString() : obj["error"].toString("请求失败"));
    });
    timeout->start(45000);
    p->start(Paths::python(), {"-B", "-u", Paths::scripts() + "/mail_worker.py"});
}
