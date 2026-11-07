#include "localdataservice.h"
#include "paths.h"
#include <QCoreApplication>
#include <QDirIterator>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QThread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

QString StorageReport::text() const {
    if (cancelled)
        return "存储统计已取消";
    auto size = [](qint64 bytes) { return QString::number(bytes / 1048576.0, 'f', 2) + " MiB"; };
    QStringList lines{"总计：" + size(total), "文件数：" + QString::number(files),
                      "根目录文件：" + size(rootBytes)};
    for (auto it = directories.begin(); it != directories.end(); ++it)
        lines << it.key() + "：" + size(it.value());
    if (skippedLinks)
        lines << QString("跳过 %1 个链接，避免重复统计或访问数据目录外的文件").arg(skippedLinks);
    if (unreadable)
        lines << QString("%1 项无法读取，以上为可读取部分：").arg(unreadable) << errors;
    return lines.join('\n');
}

StorageReport LocalDataService::scan(const QString &root, const std::atomic_bool &cancel) {
    StorageReport report;
    const QDir base(root);
    QStringList pending{base.absolutePath()};
    auto fail = [&](const QString &path) {
        ++report.unreadable;
        if (report.errors.size() < 10)
            report.errors << QDir::toNativeSeparators(path);
    };
    while (!pending.isEmpty()) {
        if (cancel.load()) {
            report.cancelled = true;
            return report;
        }
        const auto directory = pending.takeLast();
        const QFileInfo info(directory);
        if (info.isSymbolicLink() || info.isJunction()) {
            ++report.skippedLinks;
            continue;
        }
        if (!info.isDir() || !info.isReadable()) {
            fail(directory);
            continue;
        }
        QDirIterator entries(directory,
                             QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (entries.hasNext()) {
            if (cancel.load()) {
                report.cancelled = true;
                return report;
            }
            entries.next();
            const auto file = entries.fileInfo();
            if (file.isSymbolicLink() || file.isJunction()) {
                ++report.skippedLinks;
                continue;
            }
            const auto relative = base.relativeFilePath(file.absoluteFilePath());
            const auto slash = relative.indexOf('/');
            if (file.isDir()) {
                if (slash < 0)
                    report.directories.insert(relative, 0);
                pending.append(file.absoluteFilePath());
            } else if (!file.isFile() || !file.isReadable()) {
                fail(file.absoluteFilePath());
            } else {
                ++report.files;
                report.total += file.size();
                if (slash < 0)
                    report.rootBytes += file.size();
                else
                    report.directories[relative.left(slash)] += file.size();
            }
        }
    }
    return report;
}

LocalDataService::LocalDataService(QWidget *parent) : QObject(parent), window(parent) {}
LocalDataService::~LocalDataService() {
    cancelStatistics();
    if (worker)
        worker->wait();
}
bool LocalDataService::startStatistics(const QString &root) {
    if (worker)
        return false;
    cancelled = std::make_shared<std::atomic_bool>(false);
    auto result = std::make_shared<StorageReport>();
    worker = QThread::create([root, stop = cancelled, result] { *result = scan(root, *stop); });
    worker->setParent(this);
    connect(worker, &QThread::finished, this, [this, result] {
        auto finished = worker;
        worker = nullptr;
        finished->deleteLater();
        const bool discarded = cancelled->load() || result->cancelled;
        if (progress) {
            progress->close();
            progress->deleteLater();
            progress = nullptr;
            if (!discarded) {
                auto message = new QMessageBox(QMessageBox::Information, "本地存储统计", result->text(),
                                               QMessageBox::Ok, window);
                message->setTextFormat(Qt::PlainText);
                message->setAttribute(Qt::WA_DeleteOnClose);
                message->open();
            }
        }
        emit statisticsReady(discarded ? QString("存储统计已取消") : result->text());
    });
    worker->start();
    return true;
}
void LocalDataService::cancelStatistics() {
    if (cancelled)
        cancelled->store(true);
}
void LocalDataService::showStatistics() {
    if (worker) {
        if (progress) {
            progress->show();
            progress->raise();
        }
        return;
    }
    progress = new QProgressDialog("正在统计本地数据…", "取消", 0, 0, window);
    progress->setWindowTitle("本地存储统计");
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setMinimumDuration(0);
    connect(progress, &QProgressDialog::canceled, this, &LocalDataService::cancelStatistics);
    progress->show();
    startStatistics(Paths::root());
}
bool LocalDataService::launchBackup(bool restore, const QString &archive) {
    QProcess helper;
    helper.setProgram(Paths::python());
    helper.setArguments({Paths::scripts() + "/local_backup.py", restore ? "restore" : "backup", Paths::root(),
                         archive, "--wait-pid", QString::number(QCoreApplication::applicationPid()),
                         "--relaunch", QCoreApplication::applicationFilePath()});
#ifdef Q_OS_WIN
    helper.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
#endif
    return helper.startDetached();
}
