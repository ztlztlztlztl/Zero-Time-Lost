#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QJsonDocument>
#include <QFontDatabase>
#include <QSslSocket>
#include "cloudsync.h"
#include "cloudconnection.h"
#include "configmanager.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"
#include "paths.h"
#include "ddldepartment.h"
#include "notedepartment.h"
#include "cryptotool.h"
#include "cloudsettings.h"
#include "uistyle.h"

class CloudTests:public QObject {
    Q_OBJECT
    QTemporaryDir root;
    void prepare(){QFile::remove(Paths::data("cloud/state.json"));QSettings().setValue("cloud/enabled",true);}
    static QJsonObject remote(QJsonObject payload,int rev=1,bool deleted=false){return {{"kind","calendar"},{"id",payload["id"]},{"payload",deleted?QJsonObject():payload},{"rev",rev},{"deleted",deleted}};}
private slots:
    void initTestCase(){
        if(!qEnvironmentVariable("ZTL_TEST_TLS_BACKEND").isEmpty())QVERIFY(QSslSocket::setActiveBackend(qEnvironmentVariable("ZTL_TEST_TLS_BACKEND")));
        if(qEnvironmentVariableIsSet("ZTL_REQUIRE_PACKAGED_TLS"))QVERIFY(!QSslSocket::availableBackends().contains("openssl"));
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        qApp->setFont(QFont("Microsoft YaHei UI",10));
        qputenv("ZTL_DATA_DIR",root.path().toUtf8());Paths::initialize();
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,root.path());
        QCoreApplication::setOrganizationName("ZTLTests");QCoreApplication::setApplicationName("Cloud");
        ConfigManager::getInstance()->initialize();MailAccountManager::getInstance()->initialize();MailManager::getInstance()->initialize();
    }
    void offlineQueueSurvivesRestartAndDoesNotMutateInflight(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(ddl.save({{"id","local"},{"title","first"},{"repeat","none"},{"hasDue",false}}));
        QString operation;QJsonObject first;
        {CloudSync sync(&tasks,&ddl,&notes);sync.capture();first=sync.state["pending"].toObject()["calendar:local"].toObject();operation=first["op"].toString();QVERIFY(!operation.isEmpty());
         QVERIFY(ddl.update("local",{{"title","edited while offline"}}));sync.capture();QCOMPARE(sync.state["pending"].toObject()["calendar:local"].toObject(),first);}
        {CloudSync sync(&tasks,&ddl,&notes);QCOMPARE(sync.state["pending"].toObject()["calendar:local"].toObject()["op"].toString(),operation);
         auto shadow=sync.state["shadow"].toObject();auto ack=first;ack["rev"]=4;shadow["calendar:local"]=ack;sync.state["shadow"]=shadow;
         auto pending=sync.state["pending"].toObject();pending.remove("calendar:local");sync.state["pending"]=pending;sync.capture();
         auto next=sync.state["pending"].toObject()["calendar:local"].toObject();QCOMPARE(next["base"].toInteger(),4);QCOMPARE(next["payload"].toObject()["title"].toString(),QString("edited while offline"));QVERIFY(next["op"]!=first["op"]);}
    }
    void applyJournalRecoversAndDeletedRecordDoesNotResurrect(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(ddl.save({{"id","remote"},{"title","label"},{"type","label"},{"hasDue",true},{"due","2032-02-29T00:00:00"},{"repeat","none"}}));
        auto payload=tasks.find("remote");
        {CloudSync sync(&tasks,&ddl,&notes);auto r=remote(payload,9,true);sync.state["journal"]=QJsonObject{{"record",r},{"before",sync.snapshot()["calendar:remote"]},{"cursor",9}};QVERIFY(sync.save());}
        {CloudSync sync(&tasks,&ddl,&notes);QVERIFY(tasks.find("remote").isEmpty());QCOMPARE(sync.state["cursor"].toInteger(),9);sync.capture();QVERIFY(!sync.state["pending"].toObject().contains("calendar:remote"));}
    }
    void interruptedApplyPreservesNewLocalEditAsConflict(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(ddl.save({{"id","both"},{"title","original"},{"hasDue",false},{"repeat","none"}}));
        {CloudSync sync(&tasks,&ddl,&notes);auto p=tasks.find("both");p["title"]="remote";
         sync.state["journal"]=QJsonObject{{"record",remote(p,2)},{"before",sync.snapshot()["calendar:both"]},{"cursor",2}};QVERIFY(sync.save());}
        QVERIFY(ddl.update("both",{{"title","new local edit"}}));
        {CloudSync sync(&tasks,&ddl,&notes);QVERIFY(sync.state["conflicts"].toObject().contains("calendar:both"));QCOMPARE(tasks.find("both")["title"].toString(),QString("new local edit"));}
    }
    void crashAfterRemoteDeleteReplaysWithoutFalseConflict(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(ddl.save({{"id","deleted"},{"title","will be deleted"},{"hasDue",false},{"repeat","none"}}));
        {CloudSync sync(&tasks,&ddl,&notes);auto p=tasks.find("deleted");auto r=remote(p,12,true);
         sync.state["journal"]=QJsonObject{{"record",r},{"before",sync.snapshot()["calendar:deleted"]},{"cursor",12}};QVERIFY(sync.save());QVERIFY(tasks.applyChanges({}, {"deleted"}));}
        {CloudSync sync(&tasks,&ddl,&notes);QVERIFY(sync.healthy);QVERIFY(sync.state["conflicts"].toObject().isEmpty());QCOMPARE(sync.state["cursor"].toInteger(),12);}
    }
    void noteWhitelistAndOrderBeforeEntries(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(notes.save({{"id","website"},{"kind",1},{"title","web"},{"targetType","website"},{"target","https://example.test"}}));
        QVERIFY(notes.save({{"id","file"},{"kind",1},{"title","local"},{"targetType","file"},{"target","D:/local.txt"}}));
        CloudSync sync(&tasks,&ddl,&notes);auto data=sync.snapshot();QVERIFY(data.contains("note:website"));QVERIFY(!data.contains("note:file"));
        QVERIFY(!notes.applyCloudEntry("file",{},true));
        QVERIFY(notes.applyCloudOrder(2,{"not-arrived-yet"}));QCOMPARE(notes.order(2),QStringList{"not-arrived-yet"});
    }
    void remoteDatesAndRecurrenceRemainUnchanged(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);CloudSync sync(&tasks,&ddl,&notes);
        QJsonObject interval{{"id","interval"},{"type","interval"},{"title","original zone"},{"hasDue",true},{"start","2032-02-29T12:30:00-12:00"},{"due","2032-02-29T13:30:00-12:00"},{"zone","Etc/GMT+12"},{"repeat","weekly"},{"weekdays",QJsonArray{7}},{"pauseWindows",QJsonArray{QJsonObject{{"start",1},{"end",2}}}}};
        auto r=remote(interval);QVERIFY(sync.apply(r));QCOMPARE(tasks.find("interval"),interval);
        QJsonObject label{{"id","label"},{"type","label"},{"title","date only"},{"hasDue",true},{"due","2032-02-29T00:00:00+14:00"},{"labelDate","2032-02-29"},{"repeat","none"}};
        QVERIFY(sync.apply(remote(label,2)));QCOMPARE(tasks.find("label")["labelDate"].toString(),QString("2032-02-29"));
    }
    void unreadableRepositoryNeverQueuesDeletions(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);
        QFile corrupt(dir.filePath("vault.notes.json"));QVERIFY(corrupt.open(QIODevice::WriteOnly));corrupt.write("broken JSON");corrupt.close();
        VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);CloudSync sync(&tasks,&ddl,&notes);
        sync.state["shadow"]=QJsonObject{{"note:important",QJsonObject{{"kind","note"},{"id","important"},{"rev",5},{"deleted",false},{"payload",QJsonObject{{"id","important"},{"kind",2}}}}}};
        sync.capture();QVERIFY(!sync.healthy);QVERIFY(sync.state["pending"].toObject().isEmpty());
    }
    void invalidConfigurationIsDistinguishedFromFirstLaunch(){
        QTemporaryDir dir;HelpJsonIOEngine fresh(dir.path(),"new.json");fresh.read();QVERIFY(fresh.readable());
        QFile invalid(dir.filePath("broken.json"));QVERIFY(invalid.open(QIODevice::WriteOnly));invalid.write("invalid");invalid.close();
        HelpJsonIOEngine broken(dir.path(),"broken.json");broken.read();QVERIFY(!broken.readable());broken.endWrite({{"replacement",true}});QVERIFY(broken.readable());
    }
    void mailFlagsArriveBeforeSummaryAndLocalReadSurvivesRefresh(){
        const QJsonObject flags{{"address","test@example.test"},{"uid","42:9"},{"read",true},{"starred",true},{"hidden",false},{"subject","subject"},{"sender","sender"},{"timestamp",1}};
        auto mail=MailManager::getInstance();QVERIFY(mail->applyCloudFlags(flags));
        QJsonObject data{{"messages",QJsonArray{QJsonObject{{"id","42:9"},{"subject","subject"},{"from","sender"},{"date",1},{"isRead",false}}}},{"validity","42"},{"cursor",9}};
        QVERIFY(mail->saveBatch("test@example.test",data));auto list=mail->query("","test@example.test",2,10);QCOMPARE(list.size(),1);
        mail->deleteMail("test@example.test","42:9");QVERIFY(mail->saveBatch("test@example.test",data));QVERIFY(mail->isDeleted("test@example.test","42:9"));
    }
    void httpsIntegration(){
        const auto configPath=qEnvironmentVariable("ZTL_CLOUD_TEST_CONFIG");if(configPath.isEmpty())QSKIP("Run scripts/test_cloud_integration.py for HTTPS integration");
        QFile file(configPath);QVERIFY(file.open(QIODevice::ReadOnly));const auto config=QJsonDocument::fromJson(file.readAll()).object();
        QSettings settings;settings.setValue("cloud/host","127.0.0.1");settings.setValue("cloud/port",config["port"].toInt());settings.setValue("cloud/token",CryptoTool::protect(config["token"].toString()));settings.setValue("cloud/ca",config["ca"].toString());
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(ddl.save({{"id","live"},{"title","first"},{"hasDue",true},{"due","2032-01-01T09:00:00"},{"repeat","daily"},{"instances",QJsonObject{{"2032-01-01",QJsonObject{{"done",true}}}}}}));
        auto call=[&](const QString &path,const QJsonObject &body=QJsonObject(),bool post=false){QEventLoop loop;QJsonObject result;QString failure;
            CloudConnection::instance()->request(path,body,[&](const QJsonObject &r,const QString &error){result=r;failure=error;loop.quit();},post);
            QTimer::singleShot(20000,&loop,&QEventLoop::quit);loop.exec();if(!failure.isEmpty())QTest::qFail(qPrintable(failure),__FILE__,__LINE__);return result;
        };
        {
            CloudSync sync(&tasks,&ddl,&notes);sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);QVERIFY(sync.state["conflicts"].toObject().isEmpty());QVERIFY(sync.state["pending"].toObject().isEmpty());
            const auto baseline=sync.state["shadow"].toObject()["calendar:live"].toObject();QVERIFY(baseline["rev"].toInteger()>0);
            auto p=tasks.find("live");p["title"]="remote edit";
            QJsonObject request{{"op","remote1"},{"kind","calendar"},{"id","live"},{"base",baseline["rev"]},{"deleted",false},{"payload",p}};
            QVERIFY(call("/v1/mutate",request,true)["ok"].toBool());
            QVERIFY(ddl.update("live",{{"title","local edit"}}));sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);
            QVERIFY(sync.state["conflicts"].toObject().contains("calendar:live"));QCOMPARE(tasks.find("live")["title"].toString(),QString("local edit"));
            QVERIFY(sync.resolveConflict("calendar:live",false));QCOMPARE(tasks.find("live")["title"].toString(),QString("remote edit"));
            sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);QVERIFY(sync.state["conflicts"].toObject().isEmpty());
            auto current=sync.state["shadow"].toObject()["calendar:live"].toObject();request["base"]=current["rev"];request["op"]="remote-delete";request["deleted"]=true;request["payload"]=QJsonObject();
            QVERIFY(call("/v1/mutate",request,true)["ok"].toBool());sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);QVERIFY(tasks.find("live").isEmpty());
            QVERIFY(ddl.save({{"id","offline-live"},{"title","queued offline"},{"hasDue",false},{"repeat","none"}}));
            settings.setValue("cloud/port",1);sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);QVERIFY(sync.state["pending"].toObject().contains("calendar:offline-live"));
        }
        settings.setValue("cloud/port",config["port"].toInt());
        {CloudSync sync(&tasks,&ddl,&notes);sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);QVERIFY(sync.state["pending"].toObject().isEmpty());QVERIFY(sync.state["conflicts"].toObject().isEmpty());QVERIFY(tasks.find("live").isEmpty());}
        CloudSettings widget;widget.setStyleSheet(UiStyle::sheet);widget.resize(800,760);widget.show();QTest::qWait(80);
        auto screenshot=qEnvironmentVariable("ZTL_CLOUD_QA_IMAGE");if(!screenshot.isEmpty())QVERIFY(widget.grab().save(screenshot));
    }
    void deployedServerHealth(){
        const auto configPath=qEnvironmentVariable("ZTL_DEPLOYED_CLOUD_CONFIG");if(configPath.isEmpty())QSKIP("Deployed host connection file not provided");
        QFile file(configPath);QVERIFY(file.open(QIODevice::ReadOnly));const auto config=QJsonDocument::fromJson(file.readAll()).object();
        QSettings settings;settings.setValue("cloud/host",config["host"].toString());settings.setValue("cloud/port",config["port"].toInt());settings.setValue("cloud/token",CryptoTool::protect(config["token"].toString()));settings.setValue("cloud/ca",config["ca"].toString());
        QEventLoop loop;QString failure;QJsonObject result;
        CloudConnection::instance()->request("/health",{},[&](const QJsonObject &r,const QString &error){result=r;failure=error;loop.quit();});
        QTimer::singleShot(20000,&loop,&QEventLoop::quit);loop.exec();QVERIFY2(failure.isEmpty(),qPrintable(failure));QCOMPARE(result["protocol"].toInt(),1);QVERIFY(!result["dataset"].toString().isEmpty());
        settings.setValue("cloud/token",CryptoTool::protect("intentionally-invalid-test-token"));result={};failure.clear();
        CloudConnection::instance()->request("/health",{},[&](const QJsonObject &r,const QString &error){result=r;failure=error;loop.quit();});
        loop.exec();QVERIFY(failure.contains("令牌"));settings.setValue("cloud/enabled",false);
    }
    void certificateAddedAfterFailedConnection(){
        const auto configPath=qEnvironmentVariable("ZTL_CLOUD_TEST_CONFIG").isEmpty()?qEnvironmentVariable("ZTL_DEPLOYED_CLOUD_CONFIG"):qEnvironmentVariable("ZTL_CLOUD_TEST_CONFIG");
        if(configPath.isEmpty())QSKIP("HTTPS fixture not provided");
        QFile file(configPath);QVERIFY(file.open(QIODevice::ReadOnly));const auto config=QJsonDocument::fromJson(file.readAll()).object();
        QSettings settings;settings.setValue("cloud/host",config["host"].toString("127.0.0.1"));settings.setValue("cloud/port",config["port"].toInt());settings.setValue("cloud/token",CryptoTool::protect(config["token"].toString()));settings.remove("cloud/ca");
        auto connection=CloudConnection::instance();connection->notifySettingsChanged();
        QEventLoop loop;QString failure;QJsonObject result;
        auto request=[&]{result={};failure.clear();connection->request("/health",{},[&](const QJsonObject &r,const QString &error){result=r;failure=error;loop.quit();});QTimer::singleShot(20000,&loop,&QEventLoop::quit);loop.exec();};
        request();QVERIFY(failure.contains("HTTPS"));
        auto destination=Paths::data("cloud/server-ca.crt");QDir().mkpath(QFileInfo(destination).absolutePath());QVERIFY(QFile::copy(config["ca"].toString(),destination));
        settings.setValue("cloud/ca","cloud/server-ca.crt");connection->notifySettingsChanged();
        request();QVERIFY2(failure.isEmpty(),qPrintable(failure));QCOMPARE(result["protocol"].toInt(),1);
        // Replacing a certificate at the same configured path must reset the pool too.
        connection->notifySettingsChanged();request();QVERIFY2(failure.isEmpty(),qPrintable(failure));QCOMPARE(result["protocol"].toInt(),1);
    }
    void cleanupTestCase(){MailManager::getInstance()->clearUp();QSettings().setValue("cloud/enabled",false);}
};
QTEST_MAIN(CloudTests)
#include "test_cloud.moc"
