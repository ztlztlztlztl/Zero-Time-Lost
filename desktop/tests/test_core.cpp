#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "ddldepartment.h"
#include "notedepartment.h"
#include "cryptotool.h"
#include "businessclock.h"
#include "processworker.h"
#include "mailmanager.h"
#include "mailaccountmanager.h"
#include "configmanager.h"
#include "cachemanager.h"
#include "homeweatherservice.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include <memory>
class CoreTests:public QObject {
    Q_OBJECT
private slots:
    void configurationWriteFailureRetainsBuffer(){
        QTemporaryDir dir;const auto path=dir.filePath("settings.json");QVERIFY(QDir().mkdir(path));
        HelpJsonIOEngine engine(dir.path(),"settings.json");QSignalSpy failed(&engine,&HelpJsonIOEngine::writeFailed),recovered(&engine,&HelpJsonIOEngine::writeRecovered);
        const QJsonObject value{{"choice","pending"}};engine.endWrite(value);QCOMPARE(failed.size(),1);engine.forceWrite();QCOMPARE(failed.size(),1);
        QVERIFY(QDir().rmdir(path));engine.forceWrite();QCOMPARE(recovered.size(),1);QCOMPARE(engine.read(),value);
    }
    void homeTaskDaySummary(){
        QTemporaryDir dir;TaskRepository repository(dir.filePath("home.db"));DDLDepartment department(&repository);const QDate today(2036,5,6);
        auto add=[&](const QString &id,const QDate &date,const QString &repeat="none",bool paused=false){return department.save({{"id",id},{"title",id},{"repeat",repeat},{"hasDue",true},{"due",QDateTime(date,QTime(1,0)).toString(Qt::ISODate)},{"paused",paused}});};
        QVERIFY(add("today",today));QVERIFY(add("tomorrow",today.addDays(1)));QVERIFY(add("old",today.addDays(-1)));
        QVERIFY(add("daily",today.addDays(-10),"daily"));QVERIFY(add("paused",today,"daily",true));QVERIFY(add("monthly",QDate(2036,3,6),"monthly"));
        QVERIFY(department.save({{"id","undated"},{"title","undated"},{"hasDue",false},{"repeat","none"}}));
        QVERIFY(add("done",today));for(const auto &o:department.occurrences(today,today))if(o.taskId=="done")QVERIFY(department.complete(o));
        QVERIFY(add("removed",today));QVERIFY(department.setDeleted("removed",true));
        QVERIFY(add("moved",today,"daily"));QVERIFY(department.setOccurrence("moved",today.toString(Qt::ISODate),{{"due",QDateTime(today.addDays(1),QTime(2,0)).toString(Qt::ISODate)}}));
        auto counts=department.daySummary(today);QVERIFY(counts.available);QCOMPARE(counts.today,3);QCOMPARE(counts.tomorrow,4);QCOMPARE(counts.overdue,1);
        // Date boundaries, rather than the next future occurrence, determine the count.
        counts=department.daySummary(today.addDays(1));QCOMPARE(counts.today,4);QCOMPARE(counts.tomorrow,2);QCOMPARE(counts.overdue,2);
        QVERIFY(department.setPaused("daily",true));counts=department.daySummary(today);QCOMPARE(counts.today,2);QCOMPARE(counts.tomorrow,3);
        QVERIFY(!department.daySummary({}).available);
    }
    void monthlyScheduleAndOverrides(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("monthly.db"));DDLDepartment d(&repo);d.setQuiet(0,0);
        auto due=QDateTime(QDate(2032,1,31),QTime(9,0));
        QVERIFY(d.save({{"id","monthly"},{"title","月末安排"},{"hasDue",true},{"due",due.toString(Qt::ISODate)},{"repeat","monthly"},{"remind",true},{"advanceMinutes",5}}));
        auto all=d.occurrences(QDate(2032,1,1),QDate(2032,5,31));QCOMPARE(all.size(),3);
        QCOMPARE(all[0].due.date(),QDate(2032,1,31));QCOMPARE(all[1].due.date(),QDate(2032,3,31));QCOMPARE(all[2].due.date(),QDate(2032,5,31));
        QCOMPARE(d.listOccurrences(due,false).first().due.date(),QDate(2032,3,31));
        QVERIFY(d.setOccurrence("monthly","2032-03-31",{{"due",QDateTime(QDate(2032,2,10),QTime(8,0)).toString(Qt::ISODate)},{"tags","本次"}}));
        auto moved=d.occurrences(QDate(2032,2,10),QDate(2032,2,10));QCOMPARE(moved.size(),1);QCOMPARE(moved.first().tags,QString("本次"));
        QCOMPARE(d.listOccurrences(due,false).first().key,QString("2032-03-31"));
        QVERIFY(d.removeOccurrence(moved.first()));QVERIFY(d.occurrences(QDate(2032,2,10),QDate(2032,2,10)).isEmpty());
        QSignalSpy notices(&d,&DDLDepartment::reminder);d.checkReminders(due.addSecs(-60).toSecsSinceEpoch());QCOMPARE(notices.size(),1);
        QVERIFY(d.complete(all.first()));QVERIFY(d.purgeExpired(due));QVERIFY(d.task("monthly")["instances"].toObject()["2032-01-31"].toObject()["done"].toBool());
        auto replacement=d.task("monthly");replacement["due"]=QDateTime(QDate(2032,1,29),QTime(9,0)).toString(Qt::ISODate);replacement["instances"]=QJsonObject();replacement["excluded"]=QJsonArray();
        QVERIFY(d.save(replacement));all=d.occurrences(QDate(2032,2,1),QDate(2032,2,29));QCOMPARE(all.size(),1);QCOMPARE(all.first().due.date(),QDate(2032,2,29));
        QVERIFY(d.occurrences(QDate(2033,2,1),QDate(2033,2,28)).isEmpty());
        QVERIFY(d.splitSeries("monthly",QDate(2032,3,29),replacement));
    }
    void recurrenceAndReminder(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks.db"));QVERIFY(repo.ready());DDLDepartment dept(&repo);dept.setQuiet(0,0);
        // Keep the three-day range stable even when this test runs near midnight.
        const auto now=QDateTime(QDate(2036,5,6),QTime(12,0));const auto due=now.addSecs(120);
        QJsonObject task{{"title","daily test"},{"hasDue",true},{"due",due.toString(Qt::ISODate)},{"repeat","daily"},{"remind",true},{"advanceMinutes",5}};
        QVERIFY(dept.save(task));const auto all=dept.occurrences(now.date(),now.date().addDays(2));QCOMPARE(all.size(),3);
        QSignalSpy notifications(&dept,&DDLDepartment::reminder);
        dept.checkReminders(now.toSecsSinceEpoch());dept.checkReminders(now.toSecsSinceEpoch());QCOMPARE(notifications.size(),1);
        QVERIFY(dept.complete(all.first()));QVERIFY(dept.occurrences(now.date(),now.date().addDays(1)).first().done);
        QVERIFY(!dept.occurrences(now.date(),now.date().addDays(1)).last().done);
        dept.checkReminders(due.toSecsSinceEpoch()+1);QCOMPARE(notifications.size(),1);
        const auto id=all.first().taskId;
        QVERIFY(dept.setDeleted(id,true));QVERIFY(dept.task(id).isEmpty());QVERIFY(dept.occurrences(now.date(),now.date()).isEmpty());
        QVERIFY(!dept.setDeleted(id,false));
    }
    void weeklyAndPersistence(){
        QTemporaryDir dir;QString id;auto monday=QDate::currentDate().addDays(30);monday=monday.addDays(1-monday.dayOfWeek());
        {
            TaskRepository repo(dir.filePath("tasks.db"));DDLDepartment d(&repo);
            QJsonObject t{{"title","weekly"},{"hasDue",true},{"due",QDateTime(monday,QTime(9,0)).toString(Qt::ISODate)},{"repeat","weekly"},{"weekdays",QJsonArray{1,3,5}}};
            QVERIFY(d.save(t));auto list=d.occurrences(monday,monday.addDays(6));QCOMPARE(list.size(),3);
            QCOMPARE(list[1].due.date(),monday.addDays(2));id=list[1].taskId;QVERIFY(d.complete(list[1]));
        }
        TaskRepository repo(dir.filePath("tasks.db"));DDLDepartment d(&repo);
        auto list=d.occurrences(monday,monday.addDays(6));QCOMPARE(list.size(),3);QVERIFY(list[1].done);QCOMPARE(list[1].taskId,id);
        QVERIFY(!d.save({{"title","bad"},{"repeat","weekly"},{"hasDue",false}}));
    }
    void ddlCleanupAndPermanentRemoval(){
        QTemporaryDir dir;const auto path=dir.filePath("tasks.db");const auto due=QDateTime::currentDateTime().addSecs(3600);QString seriesId,removedKey;
        {
            TaskRepository repo(path);DDLDepartment d(&repo);
            QVERIFY(d.save({{"id","single"},{"title","single"},{"repeat","none"},{"hasDue",true},{"due",due.toString(Qt::ISODate)}}));
            auto single=d.listOccurrences(QDateTime::currentDateTime(),false).first();QVERIFY(d.complete(single));
            QCOMPARE(d.listOccurrences(QDateTime::currentDateTime(),false).size(),0);QCOMPARE(d.listOccurrences(QDateTime::currentDateTime(),true).size(),1);
            auto edited=d.task("single");edited["due"]=due.addSecs(60).toString(Qt::ISODate);QVERIFY(d.save(edited));
            QVERIFY(d.listOccurrences(QDateTime::currentDateTime(),true).first().done);
            edited=d.task("single");edited["due"]=due.toString(Qt::ISODate);QVERIFY(d.save(edited));
            QVERIFY(d.purgeExpired(due.addSecs(-1)));QVERIFY(!d.task("single").isEmpty());
            QVERIFY(d.purgeExpired(due));QVERIFY(!d.task("single").isEmpty());QVERIFY(d.listOccurrences(due,true).isEmpty());
            QVERIFY(d.save({{"id","undated"},{"title","undated"},{"repeat","none"},{"hasDue",false}}));
            QVERIFY(d.complete(d.listOccurrences(QDateTime::currentDateTime(),false).first()));QVERIFY(d.purgeExpired(due.addDays(300)));QVERIFY(!d.task("undated").isEmpty());
            QVERIFY(d.save({{"id","recurring"},{"title","daily"},{"repeat","daily"},{"hasDue",true},{"due",due.toString(Qt::ISODate)}}));
            auto series=d.occurrences(due.date(),due.date()).first();seriesId=series.taskId;removedKey=series.key;
            QVERIFY(d.setOccurrence(seriesId,removedKey,{{"note","discard-this-content"},{"done",true}}));
            QVERIFY(d.purgeExpired(due));QVERIFY(d.task(seriesId)["instances"].toObject()[removedKey].toObject()["done"].toBool());QVERIFY(!d.task(seriesId)["excluded"].toArray().contains(removedKey));
            QVERIFY(d.setOccurrence(seriesId,removedKey,{{"note","retained-history"}}));
            auto next=d.listOccurrences(due,false).first();QVERIFY(next.due>due);QVERIFY(d.removeOccurrence(next));QVERIFY(!d.setOccurrence(seriesId,next.key,{{"done",false}}));
            QVERIFY(repo.save({{"id","old-trash"},{"title","old trash content"},{"repeat","none"},{"deleted",true}}));QVERIFY(d.purgeExpired(due));QVERIFY(d.task("old-trash").isEmpty());
        }
        TaskRepository repo(path);DDLDepartment d(&repo);QVERIFY(!d.task(seriesId).isEmpty());
        const auto all=d.occurrences(due.date(),due.date().addDays(3));bool retained=false;for(const auto &o:all)if(o.taskId==seriesId&&o.key==removedKey){retained=true;QVERIFY(o.done);QCOMPARE(o.note,QString("retained-history"));}QVERIFY(retained);
        QVERIFY(d.setDeleted(seriesId,true));QVERIFY(d.task(seriesId).isEmpty());QVERIFY(!d.setDeleted(seriesId,false));
    }
    void overdueSnoozeOutsideCalendarRange(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks.db"));DDLDepartment d(&repo);d.setQuiet(0,0);
        const auto now=QDateTime::currentDateTime();
        QVERIFY(d.save({{"title","old overdue"},{"repeat","none"},{"hasDue",true},{"due",now.addDays(-30).toString(Qt::ISODate)}}));
        const auto o=d.listOccurrences(now,false).first();QVERIFY(d.snooze(o));QSignalSpy notices(&d,&DDLDepartment::reminder);
        d.checkReminders(now.toSecsSinceEpoch()+601);QCOMPARE(notices.size(),1);d.checkReminders(now.toSecsSinceEpoch()+602);QCOMPARE(notices.size(),1);
    }
    void ddlUpcomingSeriesAndMovedInstances(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks.db"));DDLDepartment d(&repo);const auto now=QDateTime::currentDateTime();
        QVERIFY(d.save({{"id","daily"},{"title","daily"},{"repeat","daily"},{"hasDue",true},{"due",now.addDays(-10).toString(Qt::ISODate)}}));
        auto rows=d.listOccurrences(now,false);QCOMPARE(rows.size(),1);QVERIFY(rows.first().due>now);
        const auto first=rows.first();QVERIFY(d.setOccurrence(first.taskId,first.key,{{"due",first.due.addDays(8).toString(Qt::ISODate)}}));
        rows=d.listOccurrences(now,false);QCOMPARE(rows.size(),1);QVERIFY(rows.first().key!=first.key);QVERIFY(rows.first().due<first.due.addDays(8));
        QVERIFY(d.complete(rows.first()));QCOMPARE(d.listOccurrences(now,true).first().key,rows.first().key);QVERIFY(d.listOccurrences(now,false).first().key!=rows.first().key);
        QVERIFY(d.save({{"id","remote"},{"title","future start"},{"repeat","weekly"},{"hasDue",true},{"due",now.addDays(200).toString(Qt::ISODate)},{"weekdays",QJsonArray{now.addDays(200).date().dayOfWeek()}}}));
        QCOMPARE(d.listOccurrences(now,false).size(),2);
        auto remote=d.occurrences(now.addDays(200).date(),now.addDays(200).date()).first();
        if(remote.taskId!="remote")for(const auto &o:d.occurrences(now.addDays(200).date(),now.addDays(200).date()))if(o.taskId=="remote")remote=o;
        QVERIFY(d.setOccurrence(remote.taskId,remote.key,{{"due",now.addSecs(60).toString(Qt::ISODate)}}));
        QCOMPARE(d.listOccurrences(now,false).first().taskId,QString("remote"));
    }
    void vaultRoundTripAndTamper(){
#ifdef Q_OS_WIN
        QTemporaryDir dir;VaultRepository repo(dir.filePath("vault"));NoteDepartment notes(&repo);
        QVERIFY(notes.create("correct-horse-secret"));
        QVERIFY(notes.save({{"title","private-title"},{"kind",0},{"body","unique-secret-42\nprivate note"}}));
        QFile file(repo.path());QVERIFY(file.open(QIODevice::ReadOnly));auto bytes=file.readAll();QVERIFY(!bytes.contains("unique-secret-42"));QVERIFY(bytes.contains("private-title"));file.close();
        notes.lock();QVERIFY(notes.entries().isEmpty());QVERIFY(!notes.unlock("wrong-password"));
        QVERIFY(notes.unlock("correct-horse-secret"));QCOMPARE(notes.entries().size(),1);
        QVERIFY(notes.backup(dir.filePath("backup")));QVERIFY(notes.changePassword("another-good-secret"));
        notes.lock();QVERIFY(!notes.unlock("correct-horse-secret"));QVERIFY(notes.unlock("another-good-secret"));
        QVERIFY(notes.restore(dir.filePath("backup"),"correct-horse-secret"));QCOMPARE(notes.entries().size(),1);
        notes.lock();auto env=repo.read();auto wrapped=env["private"].toObject();auto cipher=QByteArray::fromBase64(wrapped["cipher"].toString().toLatin1());cipher[0]=char(cipher[0]^1);wrapped["cipher"]=QString::fromLatin1(cipher.toBase64());env["private"]=wrapped;QVERIFY(repo.write(env));QVERIFY(!notes.unlock("correct-horse-secret"));
#else
        QSKIP("Windows cryptography backend");
#endif
    }
    void splitIsAtomicAndReschedule(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks.db"));DDLDepartment d(&repo);
        QJsonObject t{{"title","series"},{"hasDue",true},{"due","2026-10-19T09:00:00+08:00"},{"repeat","daily"}};
        QVERIFY(d.save(t));auto items=d.occurrences(QDate(2026,10,19),QDate(2026,10,22));QString id=items.first().taskId;
        QVERIFY(!d.splitSeries(id,QDate(2026,10,21),{{"title","invalid"},{"hasDue",false}}));
        QCOMPARE(d.occurrences(QDate(2026,10,19),QDate(2026,10,22)).size(),4);
        t["due"]="2026-10-21T10:00:00+08:00";QVERIFY(d.splitSeries(id,QDate(2026,10,21),t));
        items=d.occurrences(QDate(2026,10,19),QDate(2026,10,22));QCOMPARE(items.size(),4);
        QVERIFY(items[0].taskId!=items[2].taskId);
    }
    void movedOccurrenceAndPauseDedup(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks.db"));DDLDepartment d(&repo);d.setQuiet(0,0);
        const auto now=QDateTime::currentDateTime();const auto future=now.addDays(60);
        QVERIFY(d.save({{"title","moved"},{"hasDue",true},{"due",future.toString(Qt::ISODate)},{"repeat","daily"},{"advanceMinutes",5}}));
        auto series=d.occurrences(future.date(),future.date()).first();
        QVERIFY(d.setOccurrence(series.taskId,series.key,{{"due",now.addSecs(60).toString(Qt::ISODate)}}));
        auto moved=d.occurrences(now.date(),now.date());QCOMPARE(moved.size(),1);QCOMPARE(moved.first().key,series.key);
        QSignalSpy spy(&d,&DDLDepartment::reminder);d.checkReminders(now.toSecsSinceEpoch());QCOMPARE(spy.size(),1);
        QVERIFY(d.setPaused(series.taskId,true));QVERIFY(d.setPaused(series.taskId,false));
        d.checkReminders(now.toSecsSinceEpoch());QCOMPARE(spy.size(),1);
    }
    void ordinaryNotesAndLegacyMigration(){
#ifdef Q_OS_WIN
        QTemporaryDir dir;const auto path=dir.filePath("vault");
        {
            VaultRepository repo(path);NoteDepartment notes(&repo);
            QVERIFY(notes.save({{"title","paper"},{"kind",2},{"category","old group"},{"body","ordinary fragment"}}));
            QVERIFY(!notes.catalog().first().toObject().contains("category"));
            QVERIFY(!notes.exists());QCOMPARE(notes.catalog().size(),1);
        }
        const auto salt=CryptoTool::random(16),key=CryptoTool::derive("legacy-password",salt);
        const QJsonArray old{QJsonObject{{"id","old-paper"},{"title","legacy paper"},{"kind",2},{"body","old text"}},
                             QJsonObject{{"id","old-secret"},{"title","secret label"},{"kind",0},{"account","private-account"},{"secret","protected-value"}}};
        auto envelope=CryptoTool::encrypt(QJsonDocument(old).toJson(QJsonDocument::Compact),key);
        envelope["version"]=1;envelope["kdf"]="PBKDF2-SHA256-600000";envelope["salt"]=QString::fromLatin1(salt.toBase64());
        VaultRepository repo(path);QVERIFY(repo.write(envelope));NoteDepartment notes(&repo);
        QVERIFY(notes.needsMigration());QVERIFY(notes.unlock("legacy-password"));QVERIFY(!notes.needsMigration());
        QCOMPARE(notes.catalog().size(),3);QVERIFY(QFile::exists(path+".before-split"));notes.lock();
        QVERIFY(!notes.entry("old-secret").contains("secret"));QCOMPARE(notes.entry("old-paper")["body"].toString(),QString("old text"));
        QFile index(path+".notes.json");QVERIFY(index.open(QIODevice::ReadOnly));const auto bytes=index.readAll();QVERIFY(!bytes.contains("protected-value"));QVERIFY(!bytes.contains("private-account"));index.close();
        QVERIFY(!notes.remove("old-secret"));QVERIFY2(notes.unlock("legacy-password"),qPrintable(notes.error()));QCOMPARE(notes.entry("old-secret")["body"].toString(),QString("账号：private-account\n秘密：protected-value"));QVERIFY(!notes.entry("old-secret").contains("account"));QVERIFY(notes.remove("old-secret"));notes.lock();
        QVERIFY(notes.entry("old-secret").isEmpty());
#else
        QSKIP("Windows cryptography backend");
#endif
    }
    void processProtocol(){
        QTemporaryDir dir;QFile script(dir.filePath("mail_worker.py"));QVERIFY(script.open(QIODevice::WriteOnly));
        script.write("import sys,time,json\njson.load(sys.stdin)\nsys.stdout.write('{\\\"version\\\":1,\\\"ok\\\":');sys.stdout.flush();time.sleep(0.05);print('true,\\\"data\\\":{}}')\n");script.close();
        const auto old=qgetenv("ZTL_SCRIPTS_DIR");qputenv("ZTL_SCRIPTS_DIR",dir.path().toUtf8());
        ProcessWorker worker;bool finished=false;QString failure;
        worker.run({{"command","check"}},[&](const QJsonObject &,const QString &error){finished=true;failure=error;});
        QTRY_VERIFY_WITH_TIMEOUT(finished,10000);QVERIFY2(failure.isEmpty(),qPrintable(failure));
        const auto python=qgetenv("ZTL_PYTHON");qputenv("ZTL_PYTHON","Z:/missing-python.exe");finished=false;
        worker.run({},[&](const QJsonObject &,const QString &error){finished=true;failure=error;});
        QTRY_VERIFY_WITH_TIMEOUT(finished,3000);QVERIFY(!failure.isEmpty());
        qputenv("ZTL_PYTHON",python);qputenv("ZTL_SCRIPTS_DIR",old);
    }
    void credentialProtection(){
#ifdef Q_OS_WIN
        auto protectedText=CryptoTool::protect("imap-test-secret");QVERIFY(!protectedText.contains("imap-test-secret"));QCOMPARE(CryptoTool::unprotect(protectedText),QString("imap-test-secret"));
#endif
    }
    void schedulerCoalesces(){
        BusinessClock clock;int calls=0;clock.every("once",5,[&]{++calls;},true);clock.poll();clock.poll();QCOMPARE(calls,1);
        clock.every("replace",1,[&]{calls+=10;},true);clock.remove("replace");clock.poll();QCOMPARE(calls,1);clock.stop();
    }
    void weatherKeyAndProviderErrors(){
        QTemporaryDir dir;const auto oldRoot=qgetenv("ZTL_DATA_DIR");qputenv("ZTL_DATA_DIR",dir.path().toUtf8());
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
        ConfigManager::getInstance()->initialize();CacheManager::getInstance()->initialize();
        QSettings().setValue("weatherKey","  test-web-key \n");
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray payload=R"({"status":"0","info":"USERKEY_PLAT_NOMATCH","infocode":"10009"})";
        QString sentKey;int requests=0;
        connect(&server,&QTcpServer::newConnection,this,[&]{
            while(server.hasPendingConnections()){
                auto socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();
                connect(socket,&QTcpSocket::readyRead,this,[&,socket,buffer]{
                    buffer->append(socket->readAll());if(!buffer->contains("\r\n\r\n"))return;
                    socket->disconnect(this);++requests;
                    const auto target=buffer->split(' ').value(1);sentKey=QUrlQuery(QUrl("http://localhost"+QString::fromUtf8(target))).queryItemValue("key");
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(payload.size())+"\r\n\r\n"+payload);socket->disconnectFromHost();
                });
                connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            }
        });
        HomeWeatherService weather(nullptr,QUrl(QString("http://127.0.0.1:%1/weather").arg(server.serverPort())));
        QSignalSpy sceneSpy(&weather,&HomeWeatherService::sceneWeatherUpdated);
        QSignalSpy spy(&weather,&HomeWeatherService::updateWeatherRequest);weather.updateWeather(true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,3000);QCOMPARE(sentKey,QString("test-web-key"));QVERIFY(spy.takeFirst()[2].toString().contains("平台不匹配"));
        payload=R"({"status":"0","info":"INVALID_USER_KEY","infocode":"10001"})";weather.updateWeather(true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,3000);QVERIFY(spy.takeFirst()[2].toString().contains("Key 无效"));
        payload=R"({"status":"1","lives":[{"temperature":"22","weather":"晴"}]})";weather.updateWeather(true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,3000);QCOMPARE(spy.takeFirst()[1].toString(),QString("22"));
        const int before=requests;weather.updateWeather();QCOMPARE(requests,before);QCOMPARE(spy.takeFirst()[2].toString(),QString("晴"));
        payload=QString::fromUtf8("{\"status\":\"1\",\"lives\":[{\"temperature\":\"22\",\"weather\":\"雷阵雨\"}]}").toUtf8();weather.updateWeather(true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,3000);spy.clear();QCOMPARE(sceneSpy.last()[0].toString(),QString("雷阵雨"));
        payload="invalid response";weather.updateWeather(true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.size(),1,3000);QVERIFY(spy.takeFirst()[2].toString().contains("格式异常"));
        QVERIFY(sceneSpy.last()[0].toString().isEmpty());
        qputenv("ZTL_DATA_DIR",oldRoot);
    }
};
QTEST_MAIN(CoreTests)
#include "test_core.moc"
