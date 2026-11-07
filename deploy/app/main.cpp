#include "managerwindow.h"
#include "paths.h"
#include <QtWidgets>
int main(int argc, char **argv) {
    QCoreApplication::setApplicationVersion(QStringLiteral(ZTL_PRODUCT_VERSION));
    QApplication app(argc, argv);
    app.setOrganizationName("ZTL");
    app.setApplicationName("Z.T.L.ServerManager");
    app.setFont(QFont("Microsoft YaHei UI", 10));
    if (QFileInfo::exists(QCoreApplication::applicationDirPath() + "/.ztl-update.json")) {
        QMessageBox::warning(nullptr, "程序更新未完成",
                             "请重新运行打包安装流程恢复上次更新，个人配置保持不变。");
        return 2;
    }
    Paths::initialize();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, Paths::data("config"));
    QLockFile lock(Paths::data("manager.lock"));
    if (!lock.tryLock(100)) {
        QMessageBox::information(nullptr, "Z.T.L", "维护工具已在运行");
        return 1;
    }
    if (app.arguments().contains("--self-check")) {
        BootstrapProcess p;
        QObject::connect(&p, &BootstrapProcess::completed, &app, [&](const QJsonObject &r, const QString &e) {
            app.exit(e.isEmpty() && r["adapter"] == 1 ? 0 : 2);
        });
        QTimer::singleShot(0, &p, [&] { p.start({{"action", "self-check"}}); });
        return app.exec();
    }
    ManagerWindow window;
    window.show();
    return app.exec();
}
