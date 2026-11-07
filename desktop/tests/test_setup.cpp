#include "cloudconnection.h"
#include "connectionprofile.h"
#include "paths.h"
#include "setupwizard.h"
#include <QtTest>
#include <QtWidgets>
class SetupTests : public QObject {
    Q_OBJECT
    QTemporaryDir root;
    int sequence = 0;
    template <class T> T *widget(SetupWizard &w, const char *s) { return w.findChild<T *>(s); }
    void next(SetupWizard &w) { widget<QPushButton>(w, "setupNext")->click(); }
  private slots:
    void initTestCase() {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        qApp->setFont(QFont("Microsoft YaHei UI", 10));
        QCoreApplication::setOrganizationName("ZTLTests");
        QCoreApplication::setApplicationName("Setup");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root.path());
    }
    void init() {
        qputenv("ZTL_DATA_DIR", root.filePath(QString::number(++sequence)).toUtf8());
        Paths::initialize();
        QSettings().clear();
        qApp->setProperty("ztlSetupActive", true);
    }
    void cleanup() { qApp->setProperty("ztlSetupActive", false); }
    void mandatoryPasswordAndNoDeploymentControls() {
        SetupWizard w(true);
        w.show();
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty()){QTest::qWait(30);w.grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/setup-basics.png");}
        QVERIFY(!widget<QLineEdit>(w, "setupSSHPassword"));
        QVERIFY(!widget<QComboBox>(w, "setupServerMode"));
        next(w);
        QVERIFY(widget<QLabel>(w, "setupMessage")->text().contains("10"));
        widget<QLineEdit>(w, "setupPassword")->setText("test-password-123");
        widget<QLineEdit>(w, "setupConfirm")->setText("bad");
        next(w);
        QVERIFY(widget<QLabel>(w, "setupMessage")->text().contains("不一致"));
        widget<QLineEdit>(w, "setupConfirm")->setText("test-password-123");
        next(w);
        QCOMPARE(widget<QPushButton>(w, "setupNext")->text(), QString("验证连接"));
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty()){QCoreApplication::processEvents();w.grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/setup-server.png");}
        next(w);
        QVERIFY(widget<QLabel>(w, "setupMessage")->text().contains("IP"));
        widget<QLineEdit>(w, "setupHost")->setText("198.51.100.7");
        next(w);
        QVERIFY(!SetupWizard::completed());
        QVERIFY(widget<QLabel>(w, "setupMessage")->text().contains("令牌"));
    }
    void importsOnlyConnectionFieldsAndPreservesVault() {
        QByteArray vault;
        {
            SetupWizard w(true);
            widget<QLineEdit>(w, "setupPassword")->setText("test-password-123");
            widget<QLineEdit>(w, "setupConfirm")->setText("test-password-123");
            next(w);
            QFile f(Paths::data("vault.ztlvault"));
            QVERIFY(f.open(QIODevice::ReadOnly));
            vault = f.readAll();
        }
        QJsonObject p{{"format", 1}, {"protocol", 1},         {"host", "198.51.100.7"},
                      {"port", 443}, {"token", "test-token"}, {"ca", ""}};
        auto path = root.filePath("connection.ztlconn");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(p).toJson());
        file.close();
        SetupWizard w(true);
        QVERIFY(!widget<QLineEdit>(w, "setupPassword")->isEnabled());
        QVERIFY(w.importProfileFile(path));
        QCOMPARE(widget<QLineEdit>(w, "setupHost")->text(), QString("198.51.100.7"));
        QVERIFY(QSettings().value("cloud/token").toString().isEmpty());
        QFile f(Paths::data("vault.ztlvault"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), vault);
        p["password"] = "ssh-secret";
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(p).toJson());
        file.close();
        QVERIFY(!w.importProfileFile(path));
    }
    void rejectsMalformedAndIncompatibleProfiles() {
        auto path = root.filePath("invalid.ztlconn");
        for (auto p : QList<QJsonObject>{{{"format", 1}, {"protocol", 9}},
                                         {{"format", 1},
                                          {"protocol", 1},
                                          {"port", 443},
                                          {"host", "user:password@example.com"},
                                          {"token", "x"}},
                                         {{"format", 1},
                                          {"protocol", 1},
                                          {"port", 443},
                                          {"host", "example.com"},
                                          {"token", "x"},
                                          {"ca", "invalid pem"}}}) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QJsonDocument(p).toJson());
            f.close();
            QString error;
            QVERIFY(ConnectionProfile::read(path, &error).isEmpty());
            QVERIFY(!error.isEmpty());
        }
    }
    void unfinishedSetupDoesNotEnableCloud() {
        QSettings().setValue("cloud/enabled", true);
        QVERIFY(!CloudConnection::enabled());
        qApp->setProperty("ztlSetupActive", false);
        QVERIFY(CloudConnection::enabled());
    }
    void verifiedProfileCompletesSetup() {
        const auto profile = qEnvironmentVariable("ZTL_TEST_CONNECTION_PROFILE");
        if (profile.isEmpty())
            QSKIP("optional real HTTPS profile");
        SetupWizard w(true);
        widget<QLineEdit>(w, "setupPassword")->setText("test-password-123");
        widget<QLineEdit>(w, "setupConfirm")->setText("test-password-123");
        next(w);
        QVERIFY(w.importProfileFile(profile));
        next(w);
        QTRY_COMPARE_WITH_TIMEOUT(widget<QPushButton>(w, "setupNext")->text(), QString("完成并进入 Z.T.L"),
                                  25000);
        next(w);
        QVERIFY(SetupWizard::completed());
        QCOMPARE(w.result(), int(QDialog::Accepted));
        QVERIFY(QSettings().value("cloud/enabled").toBool());
        QVERIFY(!QSettings().value("cloud/token").toString().isEmpty());
        QVERIFY(QFile::exists(Paths::data("cloud/server-ca.crt")));
    }
    void render() {
        auto dest = qEnvironmentVariable("ZTL_SETUP_SCREENSHOTS");
        if (dest.isEmpty())
            QSKIP("optional screenshots");
        QDir().mkpath(dest);
        SetupWizard w(true);
        w.show();
        QTest::qWait(100);
        QVERIFY(w.grab().save(dest + "/desktop-basics.png"));
        widget<QLineEdit>(w, "setupPassword")->setText("test-password-123");
        widget<QLineEdit>(w, "setupConfirm")->setText("test-password-123");
        next(w);
        QTest::qWait(100);
        QVERIFY(w.grab().save(dest + "/desktop-connection.png"));
    }
};
QTEST_MAIN(SetupTests)
#include "test_setup.moc"
