#include "bootstrapprocess.h"
#include "paths.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>

BootstrapProcess::BootstrapProcess(QObject *parent) : QObject(parent) {
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, this, [this] {
        failure = "初始化操作超时，请重新检测服务器状态后重试";
        process.kill();
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, &BootstrapProcess::consume);
    connect(&process, &QProcess::readyReadStandardError, this, [this] { process.readAllStandardError(); });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            timeout.stop();
            emit completed({}, "内置部署运行时无法启动，请使用完整安装包");
        }
    });
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                timeout.stop();
                consume();
                if (failure.isEmpty() && (!terminal || code != 0 || status != QProcess::NormalExit))
                    failure = "部署工作器异常结束，请重新检测后重试";
                emit completed(result, failure);
                result = {};
                buffer.clear();
            });
}
void BootstrapProcess::start(QJsonObject request) {
    if (busy())
        return;
    if (!QFileInfo::exists(Paths::scripts() + "/bootstrap_worker.py")) {
        emit completed({}, "缺少自部署脚本，请使用完整安装包");
        return;
    }
    result = {};
    failure.clear();
    buffer.clear();
    terminal = false;
    request["version"] = 1;
    request["dataDir"] = Paths::root();
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto &key : {"TEMP", "TMP", "TMPDIR"})
        env.insert(key, Paths::data("tmp"));
    env.insert("PYTHONDONTWRITEBYTECODE", "1");
    env.insert("PYTHONUTF8", "1");
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(Paths::root());
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(&process, &QProcess::started, this, [this, request, connection] {
        disconnect(*connection);
        process.write(QJsonDocument(request).toJson(QJsonDocument::Compact));
        process.closeWriteChannel();
    });
    // Clear a failed-start handler before the next request as well.
    connect(
        &process, &QProcess::errorOccurred, this,
        [connection](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart)
                QObject::disconnect(*connection);
        },
        Qt::SingleShotConnection);
    timeout.start(request["action"] == "deploy" ? 60 * 60 * 1000 : 180000);
    process.start(Paths::python(), {"-B", "-u", Paths::scripts() + "/bootstrap_worker.py"});
}
void BootstrapProcess::consume() {
    buffer += process.readAllStandardOutput();
    if (buffer.size() > 4 * 1024 * 1024) {
        failure = "部署响应过大，已停止等待，请重新检测";
        process.kill();
        return;
    }
    while (buffer.contains('\n')) {
        const auto end = buffer.indexOf('\n');
        auto line = buffer.left(end);
        buffer.remove(0, end + 1);
        QJsonParseError parse;
        auto document = QJsonDocument::fromJson(line, &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject() ||
            document.object()["version"] != 1) {
            failure = "部署工作器响应无效";
            continue;
        }
        const auto object = document.object();
        const auto kind = object["event"].toString();
        if (kind == "progress")
            emit progress(object["message"].toString());
        else if (kind == "result" && !terminal) {
            result = object["data"].toObject();
            terminal = true;
        } else if (kind == "error" && !terminal) {
            failure = object["message"].toString("部署失败");
            terminal = true;
        } else
            failure = "部署工作器返回了无效事件";
    }
}
