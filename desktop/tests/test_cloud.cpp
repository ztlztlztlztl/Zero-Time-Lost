#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QJsonDocument>
#include <QFontDatabase>
#include <QSslSocket>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
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
#include "generalsettings.h"
#include "calendardto.h"

class CloudTests:public QObject {
    Q_OBJECT
    QTemporaryDir root;
    void prepare(){QFile::remove(Paths::data("cloud/state.json"));QSettings().setValue("cloud/enabled",true);}
    static QJsonObject remote(QJsonObject payload,int rev=1,bool deleted=false){return {{"kind","calendar"},{"id",payload["id"]},{"payload",deleted?QJsonObject():payload},{"rev",rev},{"deleted",deleted}};}
private slots:
    void disableCloudDoesNotRequireReadableCredentialsOrNetwork(){
        QSettings settings;
        QMap<QString,QVariant> previous;
        for(auto key:{"enabled","host","token","ca"})previous[key]=settings.value("cloud/"+QString(key));
        const auto restore=qScopeGuard([&]{for(auto it=previous.begin();it!=previous.end();++it){const auto key="cloud/"+it.key();if(it.value().isValid())settings.setValue(key,it.value());else settings.remove(key);}CloudConnection::instance()->notifySettingsChanged();});
        settings.setValue("cloud/enabled",true);
        settings.setValue("cloud/host","invalid address/path");
        settings.setValue("cloud/token","unreadable-protected-token");
        settings.setValue("cloud/ca","missing-certificate.crt");
        CloudSettings widget;
        auto enabled=widget.findChild<QCheckBox*>("cloudEnabled");
        auto apply=widget.findChild<QPushButton*>("cloudTestApply");
        QVERIFY(enabled);QVERIFY(apply);QVERIFY(enabled->isChecked());
        QSignalSpy changed(CloudConnection::instance(),&CloudConnection::settingsChanged);
        enabled->setChecked(false);apply->click();
        QVERIFY(!CloudConnection::enabled());QCOMPARE(changed.size(),1);
        QCOMPARE(settings.value("cloud/token").toString(),QString("unreadable-protected-token"));
        QCOMPARE(settings.value("cloud/ca").toString(),QString("missing-certificate.crt"));
        QCOMPARE(settings.value("cloud/host").toString(),QString("invalid address/path"));
    }
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
    void changedWeatherKeyInvalidatesCachedSettingsWithoutCityChange(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QSettings settings;const auto previous=settings.value("weatherKey");
        const auto restore=qScopeGuard([&]{if(previous.isValid())settings.setValue("weatherKey",previous);else settings.remove("weatherKey");});
        settings.setValue("weatherKey","old-test-key");
        CloudSync sync(&tasks,&ddl,&notes);sync.capture();
        auto shadow=sync.state["pending"].toObject();
        for(auto it=shadow.begin();it!=shadow.end();++it){auto r=it.value().toObject();r["rev"]=7;it.value()=r;}
        sync.state["shadow"]=shadow;sync.state["pending"]=QJsonObject();
        sync.capture();QVERIFY(sync.state["pending"].toObject().isEmpty());
        settings.setValue("weatherKey","new-test-key");
        sync.snapshot(); // A diagnostic snapshot must not consume the pending change.
        sync.capture();
        const auto next=sync.state["pending"].toObject()["settings:shared"].toObject();
        QCOMPARE(next["payload"].toObject()["weatherKey"].toString(),QString("new-test-key"));
        QCOMPARE(next["base"].toInt(),7);QCOMPARE(sync.lastCaptureCount,1);
        const auto operation=next["op"];
        settings.setValue("weatherKey","newest-test-key");sync.capture();
        QCOMPARE(sync.state["pending"].toObject()["settings:shared"].toObject()["op"],operation);
        auto accepted=next;accepted["rev"]=8;shadow["settings:shared"]=accepted;
        sync.state["shadow"]=shadow;sync.state["pending"]=QJsonObject();sync.captureKeys.insert("settings:shared");
        sync.capture();
        const auto newest=sync.state["pending"].toObject()["settings:shared"].toObject();
        QCOMPARE(newest["payload"].toObject()["weatherKey"].toString(),QString("newest-test-key"));
        QCOMPARE(newest["base"].toInt(),8);
        settings.setValue("weatherKey",QString());sync.capture();
        QCOMPARE(sync.snapshot()["settings:shared"].toObject()["payload"].toObject()["weatherKey"].toString(),QString());
    }
    void weatherKeyEditorRequestsSyncOnApply(){
        QSettings settings;const auto previous=settings.value("weatherKey");
        const auto restore=qScopeGuard([&]{if(previous.isValid())settings.setValue("weatherKey",previous);else settings.remove("weatherKey");});
        settings.setValue("weatherKey","before-edit");
        GeneralSettings widget(GeneralSettings::Home);
        auto field=widget.findChild<QLineEdit*>("weatherKey");QVERIFY(field);
        QSignalSpy requested(CloudConnection::instance(),&CloudConnection::syncRequested);
        field->setText("  after-edit  ");QTest::keyClick(field,Qt::Key_Return);
        QCOMPARE(settings.value("weatherKey").toString(),QString("after-edit"));
        QCOMPARE(requested.size(),1);
        QTest::keyClick(field,Qt::Key_Return);QCOMPARE(requested.size(),1);
    }
    void repairsRejectedEmptyPrefixWithoutChangingValidInflightRequests(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        const QJsonObject legacy{{"id","empty-prefix"},{"title","series"},{"type","interval"},{"repeat","weekly"},{"hasDue",true},
            {"start","2032-11-08T08:00:00+08:00"},{"due","2032-11-08T09:50:00+08:00"},{"zone","Asia/Shanghai"},
            {"until","2032-11-07"},{"weekdays",QJsonArray{QDate(2032,11,8).dayOfWeek()}},{"countLimited",true},{"countLimit",10}};
        QVERIFY(!CalendarDTO::validate(legacy).isEmpty());QVERIFY(tasks.save(legacy));
        const QJsonObject valid{{"id","following"},{"title","valid following task"},{"repeat","none"},{"hasDue",false}};
        QVERIFY(tasks.save(valid));QString repairedOperation;
        {
            CloudSync sync(&tasks,&ddl,&notes);sync.capture();
            auto pending=sync.state["pending"].toObject();
            auto rejected=pending["calendar:empty-prefix"].toObject();rejected["payload"]=legacy;rejected["op"]="rejected-legacy-operation";pending["calendar:empty-prefix"]=rejected;
            const auto immutable=pending["calendar:following"].toObject();
            sync.state["pending"]=pending;sync.fullCapture=true;sync.capture();
            const auto repaired=sync.state["pending"].toObject()["calendar:empty-prefix"].toObject();
            const auto payload=repaired["payload"].toObject();
            QVERIFY(CalendarDTO::validate(payload).isEmpty());QCOMPARE(payload["until"].toString(),QString("2032-11-08"));
            QVERIFY(payload["excluded"].toArray().contains("2032-11-08"));
            repairedOperation=repaired["op"].toString();QVERIFY(repairedOperation!="rejected-legacy-operation");
            QCOMPARE(sync.state["pending"].toObject()["calendar:following"].toObject(),immutable);
            QCOMPARE(tasks.find("empty-prefix"),legacy); // Preserve the original local record.
            QCOMPARE(ddl.occurrences(QDate(2032,11,1),QDate(2033,2,1)).size(),1); // Only the undated task remains.
            sync.capture();QCOMPARE(sync.state["pending"].toObject()["calendar:empty-prefix"].toObject(),repaired);
        }
        CloudSync restarted(&tasks,&ddl,&notes);restarted.capture();
        QCOMPARE(restarted.state["pending"].toObject()["calendar:empty-prefix"].toObject()["op"].toString(),repairedOperation);
    }
    void coldStartEnumeratesExistingCalendarAndNotesBeforeDetectingDeletions(){
        QTemporaryDir dir;const auto originalRoot=qEnvironmentVariable("ZTL_DATA_DIR");
        qputenv("ZTL_DATA_DIR",dir.path().toUtf8());
        const auto restore=qScopeGuard([&]{qputenv("ZTL_DATA_DIR",originalRoot.toUtf8());});
        prepare();
        {
            TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
            QVERIFY(tasks.save({{"id","cold-calendar"},{"title","existing calendar"},{"hasDue",false},{"repeat","none"}}));
            QVERIFY(notes.save({{"id","cold-note"},{"title","existing note"},{"body","saved before restart"},{"kind",2}}));QVERIFY(notes.reorder(2,{"cold-note"}));QVERIFY(notes.flush());
            CloudSync sync(&tasks,&ddl,&notes);sync.capture();
            auto shadow=sync.state["pending"].toObject();
            QVERIFY(shadow.contains("calendar:cold-calendar"));QVERIFY(shadow.contains("note:cold-note"));
            for(auto it=shadow.begin();it!=shadow.end();++it){auto value=it.value().toObject();value["rev"]=11;it.value()=value;}
            sync.state["shadow"]=shadow;sync.state["pending"]=QJsonObject();QVERIFY(sync.save());
        }
        {
            TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
            QCOMPARE(tasks.revision(),quint64(0));QCOMPARE(tasks.all().size(),1);
            CloudSync sync(&tasks,&ddl,&notes);QVERIFY(sync.snapshotCache.isEmpty());
            const auto snapshot=sync.snapshot();
            QVERIFY(snapshot.contains("calendar:cold-calendar"));QVERIFY(snapshot.contains("note:cold-note"));
            QVERIFY(snapshot["order:2"].toObject()["payload"].toObject()["ids"].toArray().contains("cold-note"));
            sync.capture();
            const auto pending=sync.state["pending"].toObject();
            // Reopening legacy notes may normalize an absent deleted field to null.
            // That harmless update must never turn existing records into deletions.
            for(auto it=pending.begin();it!=pending.end();++it){
                QCOMPARE(it.key(),QString("note:cold-note"));
                const auto record=it.value().toObject();
                QVERIFY(!record["deleted"].toBool());
                auto payload=record["payload"].toObject();
                auto original=sync.state["shadow"].toObject()[it.key()].toObject()["payload"].toObject();
                payload.remove("deleted");original.remove("deleted");
                QCOMPARE(payload,original);
            }
            QVERIFY(!sync.state["shadow"].toObject()["calendar:cold-calendar"].toObject()["deleted"].toBool());
        }
    }
    void incrementalCaptureIncludesExternalSnapshotsAndRepositoryChanges(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QList<QJsonObject> initial;
        for(int i=0;i<1000;++i)initial.append({{"id",QString::number(i)},{"title","original"},{"hasDue",false},{"repeat","none"}});
        QVERIFY(tasks.saveMany(initial));
        CloudSync sync(&tasks,&ddl,&notes);sync.capture();QVERIFY(sync.healthy);
        auto shadow=sync.state["pending"].toObject();
        for(auto it=shadow.begin();it!=shadow.end();++it){auto r=it.value().toObject();r["rev"]=5;it.value()=r;}
        sync.state["shadow"]=shadow;sync.state["pending"]=QJsonObject();
        sync.capture();QCOMPARE(sync.lastCaptureCount,0);QVERIFY(sync.state["pending"].toObject().isEmpty());
        auto changed=tasks.find("500");changed["title"]="edited directly";QVERIFY(tasks.save(changed));
        sync.snapshot(); // UI/diagnostic snapshots must not consume the queue's dirty set.
        sync.capture();QCOMPARE(sync.lastCaptureCount,1);
        auto pending=sync.state["pending"].toObject();QCOMPARE(pending.size(),1);
        const auto operation=pending["calendar:500"].toObject();QCOMPARE(operation["base"].toInt(),5);
        changed["title"]="edited while pending";QVERIFY(tasks.save(changed));sync.capture();
        QCOMPARE(sync.state["pending"].toObject()["calendar:500"].toObject(),operation);
        shadow["calendar:500"]=remote(operation["payload"].toObject(),6);
        sync.state["shadow"]=shadow;sync.state["pending"]=QJsonObject();sync.captureKeys.insert("calendar:500");
        sync.capture();QCOMPARE(sync.lastCaptureCount,1);
        const auto next=sync.state["pending"].toObject()["calendar:500"].toObject();
        QCOMPARE(next["base"].toInt(),6);QCOMPARE(next["payload"].toObject()["title"].toString(),QString("edited while pending"));
        QVERIFY(tasks.applyChanges({}, {"700"}));sync.capture();QCOMPARE(sync.lastCaptureCount,1);
        QVERIFY(sync.state["pending"].toObject()["calendar:700"].toObject()["deleted"].toBool());
    }
    void restartPerformsFullReconciliationForOfflineDeletion(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(ddl.save({{"id","gone"},{"title","offline deletion"},{"hasDue",false},{"repeat","none"}}));
        {CloudSync sync(&tasks,&ddl,&notes);sync.capture();sync.state["shadow"]=sync.state["pending"];sync.state["pending"]=QJsonObject();QVERIFY(sync.save());}
        QVERIFY(tasks.applyChanges({}, {"gone"}));
        CloudSync sync(&tasks,&ddl,&notes);sync.capture();
        QVERIFY(sync.state["pending"].toObject()["calendar:gone"].toObject()["deleted"].toBool());
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
    void incrementalNoteSnapshotUpdatesWhitelistAndPublicOrder(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(notes.save({{"id","switch"},{"kind",1},{"title","bookmark"},{"targetType","website"},{"target","https://example.test"}}));
        QVERIFY(notes.reorder(1,{"switch"}));
        CloudSync sync(&tasks,&ddl,&notes);QVERIFY(sync.snapshot().contains("note:switch"));
        auto entry=notes.entry("switch");entry["targetType"]="file";entry["target"]="D:/personal.txt";QVERIFY(notes.save(entry));
        auto data=sync.snapshot();QVERIFY(!data.contains("note:switch"));
        QVERIFY(!data["order:1"].toObject()["payload"].toObject()["ids"].toArray().contains("switch"));
        entry["targetType"]="website";entry["target"]="https://example.test/restored";QVERIFY(notes.save(entry));
        data=sync.snapshot();QVERIFY(data.contains("note:switch"));
        QVERIFY(data["order:1"].toObject()["payload"].toObject()["ids"].toArray().contains("switch"));
        QVERIFY(notes.remove("switch"));data=sync.snapshot();QVERIFY(!data.contains("note:switch"));
        // Missing entries retain their reserved order, as when ordering arrives before content.
        QVERIFY(data["order:1"].toObject()["payload"].toObject()["ids"].toArray().contains("switch"));
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
        HelpJsonIOEngine broken(dir.path(),"broken.json");broken.read();QVERIFY(!broken.readable());broken.endWrite({{"replacement",true}});QVERIFY(!broken.readable());QVERIFY(!broken.flush());QVERIFY(broken.recover(true));QVERIFY(broken.readable());
    }
    void restoreReconnectPreservesDataAndPendingDeletions(){
        prepare();QTemporaryDir dir;TaskRepository tasks(dir.filePath("tasks"));DDLDepartment ddl(&tasks);VaultRepository vault(dir.filePath("vault"));NoteDepartment notes(&vault);
        QVERIFY(ddl.save({{"id","keep"},{"title","offline edit"},{"hasDue",false},{"repeat","none"}}));
        CloudSync sync(&tasks,&ddl,&notes);sync.capture();auto pending=sync.state["pending"].toObject();
        const auto oldOperation=pending["calendar:keep"].toObject()["op"].toString();
        pending["calendar:deleted"]=QJsonObject{{"op","old-op"},{"kind","calendar"},{"id","deleted"},{"base",8},{"deleted",true},{"payload",QJsonObject{}}};
        sync.state["pending"]=pending;sync.state["dataset"]="old-dataset";sync.state["cursor"]=99;
        QVERIFY(sync.prepareReconnect());QVERIFY(!sync.state.contains("dataset"));QCOMPARE(sync.state.value("cursor").toInteger(),0);
        const auto now=sync.state["pending"].toObject();QCOMPARE(now["calendar:keep"].toObject()["base"].toInt(),0);QVERIFY(now["calendar:keep"].toObject()["op"].toString()!=oldOperation);
        QVERIFY(now["calendar:deleted"].toObject()["deleted"].toBool());QCOMPARE(now["calendar:deleted"].toObject()["base"].toInt(),0);QCOMPARE(tasks.find("keep")["title"].toString(),QString("offline edit"));
        QVERIFY(!QDir(Paths::data("backups")).entryList({"cloud-sync-before-reconnect-*.json"},QDir::Files).isEmpty());
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
            CloudSync sync(&tasks,&ddl,&notes);
            QSignalSpy settingsUploaded(&sync,&CloudSync::sharedSettingsUploaded);
            const QJsonObject legacy{{"id","https-empty-prefix"},{"title","legacy empty prefix"},{"repeat","daily"},{"hasDue",true},
                {"due","2032-01-01T10:00:00+08:00"},{"zone","Asia/Shanghai"},{"until","2031-12-31"}};
            QVERIFY(tasks.save(legacy));sync.capture();
            auto queued=sync.state["pending"].toObject();auto rejected=queued["calendar:https-empty-prefix"].toObject();
            rejected["payload"]=legacy;rejected["op"]="rejected-https-prefix";queued["calendar:https-empty-prefix"]=rejected;sync.state["pending"]=queued;sync.fullCapture=true;
            {
                QEventLoop loop;QString failure;
                CloudConnection::instance()->request("/v1/mutate",rejected,[&](const QJsonObject &,const QString &error){failure=error;loop.quit();},true);
                QTimer::singleShot(20000,&loop,&QEventLoop::quit);loop.exec();
                QVERIFY(failure.contains("422"));QVERIFY(failure.contains("calendar.until"));
                QVERIFY(!failure.contains("legacy empty prefix"));
            }
            bool editedWithPending=false;
            connect(&sync,&CloudSync::status,&sync,[&](const QString &message){
                if(!editedWithPending&&message=="正在同步云端数据…"){
                    editedWithPending=true;
                    QVERIFY(ddl.update("live",{{"title","edited with pending upload"}}));
                }
            });
            sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);QVERIFY(sync.state["conflicts"].toObject().isEmpty());QVERIFY(sync.state["pending"].toObject().isEmpty());
            const auto baseline=sync.state["shadow"].toObject()["calendar:live"].toObject();QVERIFY(baseline["rev"].toInteger()>0);
            const auto repaired=sync.state["shadow"].toObject()["calendar:https-empty-prefix"].toObject()["payload"].toObject();
            QCOMPARE(repaired["until"].toString(),QString("2032-01-01"));QVERIFY(repaired["excluded"].toArray().contains("2032-01-01"));
            QVERIFY(settingsUploaded.size()>0);
            const auto previousWeatherKey=settings.value("weatherKey");
            const auto restoreWeatherKey=qScopeGuard([&]{if(previousWeatherKey.isValid())settings.setValue("weatherKey",previousWeatherKey);else settings.remove("weatherKey");});
            settingsUploaded.clear();settings.setValue("weatherKey","https-updated-weather-key");
            sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);
            QCOMPARE(settingsUploaded.size(),1);
            QCOMPARE(sync.state["shadow"].toObject()["settings:shared"].toObject()["payload"].toObject()["weatherKey"].toString(),QString("https-updated-weather-key"));
            settingsUploaded.clear();sync.sync();QTRY_VERIFY_WITH_TIMEOUT(!sync.busy,30000);QCOMPARE(settingsUploaded.size(),0);
            QVERIFY(editedWithPending);QCOMPARE(baseline["payload"].toObject()["title"].toString(),QString("edited with pending upload"));
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
