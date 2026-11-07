#include "managerwindow.h"
#include "paths.h"
#include <QtTest>
#include <QtWidgets>
class ManagerTests : public QObject {
    Q_OBJECT
    QTemporaryDir data;
  private slots:
    void initTestCase() {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        qApp->setFont(QFont("Microsoft YaHei UI", 10));
        qputenv("ZTL_MANAGER_DATA_DIR", data.path().toUtf8());
        qputenv("ZTL_MANAGER_SCRIPTS_DIR", ZTL_MANAGER_FIXTURES);
        Paths::initialize();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, data.path());
    }
    void defaultUpdateCannotRunBeforeDetection() {
        ManagerWindow w;
        w.show();
        QCOMPARE(w.findChild<QComboBox *>("managerMode")->currentIndex(), 0);
        w.findChild<QPushButton *>("managerDeploy")->click();
        QVERIFY(w.findChild<QPlainTextEdit *>("managerLog")->toPlainText().contains("请填写"));
    }
    void inspectDeployAndExportWithoutLeakingSshPassword() {
        ManagerWindow w;
        w.show();
        auto host = w.findChild<QLineEdit *>("managerHost");
        host->setText("198.51.100.7");
        w.findChild<QLineEdit *>("managerPassword")->setText("ssh-only-in-memory");
        auto release = w.findChild<QLineEdit *>("managerRelease");
        release->setText("test-server.zip");
        QMetaObject::invokeMethod(release, "editingFinished");
        QTRY_VERIFY_WITH_TIMEOUT(
            w.findChild<QPlainTextEdit *>("managerLog")->toPlainText().contains("test-2.0"), 10000);
        w.findChild<QPushButton *>("managerScan")->click();
        QTRY_VERIFY_WITH_TIMEOUT(w.findChild<QLabel *>("managerSummary")->text().contains("SHA256:fixture"),
                                 10000);
        w.findChild<QCheckBox *>("managerTrust")->setChecked(true);
        w.findChild<QPushButton *>("managerDeploy")->click();
        QTRY_VERIFY_WITH_TIMEOUT(
            w.findChild<QPlainTextEdit *>("managerLog")->toPlainText().contains("部署与 HTTPS 验证成功"),
            10000);
        auto path = data.filePath("connection.ztlconn");
        QVERIFY(w.exportConnectionFile(path));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const auto bytes = f.readAll();
        QVERIFY(!bytes.contains("ssh-only-in-memory"));
        const auto p = QJsonDocument::fromJson(bytes).object();
        QCOMPARE(p["host"].toString(), QString("198.51.100.7"));
        QCOMPARE(p["format"].toInt(), 1);
        QCOMPARE(p.keys().size(), 6);
        for (auto key : QSettings().allKeys())
            QVERIFY(!QSettings().value(key).toString().contains("ssh-only-in-memory"));
        host->setText("198.51.100.8");
        QVERIFY(!w.findChild<QCheckBox *>("managerTrust")->isChecked());
        QVERIFY(!w.exportConnectionFile(data.filePath("wrong.ztlconn")));
    }
    void screenshot() {
        auto path = qEnvironmentVariable("ZTL_MANAGER_QA_IMAGE");
        if (path.isEmpty())
            QSKIP("optional screenshot");
        ManagerWindow w;
        w.show();
        QTest::qWait(150);
        QVERIFY(w.grab().save(path));
    }
};
QTEST_MAIN(ManagerTests)
#include "test_manager.moc"
