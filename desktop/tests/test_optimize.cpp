#include "calendardto.h"
#include "calendarquery.h"
#include "cloudprofile.h"
#include "cloudsync.h"
#include "localdataservice.h"
#include "configmanager.h"
#include "ddldepartment.h"
#include "filebatch.h"
#include "helpjsonioengine.h"
#include "mailmanager.h"
#include "mailaccountmanager.h"
#include "notecard.h"
#include "paths.h"
#include "taskrepository.h"
#include <QtTest>
#include <QtWidgets>

class OptimizeTests : public QObject {
    Q_OBJECT
    QTemporaryDir root;
  private slots:
    void storageStatisticsIncludeRootHiddenAndNestedFilesAndCanCancel() {
        QTemporaryDir data;
        QVERIFY(QDir(data.path()).mkpath("database/nested"));
        auto write = [&](const QString &name, int bytes) {
            QFile file(data.filePath(name));
            return file.open(QIODevice::WriteOnly) && file.write(QByteArray(bytes, 'x')) == bytes;
        };
        QVERIFY(write("vault.ztlvault", 31));
        QVERIFY(write(".hidden", 7));
        QVERIFY(write("database/nested/tasks.db", 41));
        std::atomic_bool cancelled{false};
        const auto report = LocalDataService::scan(data.path(), cancelled);
        QCOMPARE(report.files, 3);QCOMPARE(report.rootBytes, 38);
        QCOMPARE(report.directories.value("database"), 41);QCOMPARE(report.total, 79);
        QCOMPARE(report.unreadable, 0);QVERIFY(!report.cancelled);
        cancelled.store(true);
        QVERIFY(LocalDataService::scan(data.path(), cancelled).cancelled);
        LocalDataService service(nullptr);
        QSignalSpy ready(&service, &LocalDataService::statisticsReady);
        QVERIFY(service.startStatistics(data.path()));QVERIFY(!service.startStatistics(data.path()));
        service.cancelStatistics();
        QTRY_COMPARE(ready.size(), 1);QVERIFY(ready.takeFirst()[0].toString().contains("取消"));
        QVERIFY(service.startStatistics(data.path()));QTRY_COMPARE(ready.size(), 1);
        QVERIFY(ready.takeFirst()[0].toString().contains("文件数：3"));
    }
    void initTestCase() {
        qputenv("ZTL_DATA_DIR", root.path().toUtf8());
        QCoreApplication::setOrganizationName("ZTL-Tests");
        QCoreApplication::setApplicationName("Optimize");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root.path());
        Paths::initialize();
        ConfigManager::getInstance()->initialize();
        MailManager::getInstance()->initialize();
    }
    void mailInitializationFailureCanBeRetriedWithoutReadySnapshot() {
        auto mail = MailManager::getInstance();
        mail->clearUp();
        QTemporaryDir isolated;
        qputenv("ZTL_DATA_DIR", isolated.path().toUtf8());
        QDir().mkpath(isolated.filePath("database"));
        QFile broken(isolated.filePath("database/mail.db"));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not a database");broken.close();
        const bool failed = !mail->initialize();
        const bool notReady = !mail->ready();
        bool readable = true;mail->cloudFlags(&readable);
        mail->clearUp();
        const bool removed = broken.remove();
        const bool recovered = mail->initialize();
        mail->clearUp();
        qputenv("ZTL_DATA_DIR", root.path().toUtf8());
        const bool originalReady = mail->initialize();
        QVERIFY(failed);QVERIFY(notReady);QVERIFY(!readable);QVERIFY(removed);QVERIFY(recovered);QVERIFY(originalReady);
    }
    void malformedSyncStateRemainsProtected() {
        QTemporaryDir isolated;
        const auto oldRoot = qEnvironmentVariable("ZTL_DATA_DIR");
        qputenv("ZTL_DATA_DIR", isolated.path().toUtf8());
        QDir().mkpath(Paths::data("cloud"));
        QFile file(Paths::data("cloud/state.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));file.write("{\"pending\":[]}");file.close();
        TaskRepository tasks(isolated.filePath("tasks.db"));DDLDepartment ddl(&tasks);
        VaultRepository vault(isolated.filePath("vault"));NoteDepartment notes(&vault);
        bool blocked;
        { CloudSync sync(&tasks,&ddl,&notes);blocked=!sync.flush(); }
        QVERIFY(file.open(QIODevice::ReadOnly));const auto original=file.readAll();file.close();
        qputenv("ZTL_DATA_DIR", oldRoot.toUtf8());
        QVERIFY(blocked);QCOMPARE(original,QByteArray("{\"pending\":[]}"));
    }
    void restoredConfigurationDoesNotGetOverwrittenByFallbackEdits() {
        QTemporaryDir dir;
        QFile f(dir.filePath("broken.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("broken");
        f.close();
        HelpJsonIOEngine engine(dir.path(), "broken.json");
        engine.read();
        engine.endWrite({{"fallback", true}});
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("{\"original\":true}");
        f.close();
        QVERIFY(engine.recover(false));
        QVERIFY(engine.read()["original"].toBool());
        QVERIFY(!engine.read().contains("fallback"));
    }
    void retainedCalendarCursorMatchesFreshQuery() {
        QList<QJsonObject> tasks;
        const QDate first(2026, 1, 1);
        for (int i = 0; i < 3; ++i)
            tasks.append({{"id", QString::number(i)},
                          {"title", "daily"},
                          {"type", "ddl"},
                          {"hasDue", true},
                          {"repeat", "daily"},
                          {"due", QDateTime(first, QTime(10, i)).toString(Qt::ISODate)}});
        AdvancedFilter filter;
        filter.limitDates = true;
        filter.from = first;
        filter.to = first.addDays(500);
        const auto now = QDateTime(first, QTime(0, 0));
        CalendarQuery::Session cursor(tasks, filter, filter.from, filter.to, now, true);
        QCOMPARE(cursor.page(20).items.size(), 20);
        auto resumed = cursor.page(65),
             fresh = CalendarQuery::page(tasks, filter, filter.from, filter.to, 65, now, true);
        QCOMPARE(resumed.items.size(), fresh.items.size());
        for (int i = 0; i < fresh.items.size(); ++i)
            QCOMPARE(resumed.items[i].identity(), fresh.items[i].identity());
        QVERIFY(resumed.more);
    }
    void monthlyArithmeticMatchesLeapAndCenturyDates() {
        for (auto first : {QDate(1899, 12, 29), QDate(1999, 12, 30), QDate(2099, 12, 31)}) {
            QJsonObject task{{"due", QDateTime(first, QTime(10, 0)).toString(Qt::ISODate)},
                             {"repeat", "monthly"},
                             {"monthdays", QJsonArray{1, 28, 29, 30, 31}}};
            int count = 0;
            for (auto d = first; d < first.addYears(3); d = d.addDays(1)) {
                if (QSet<int>{1, 28, 29, 30, 31}.contains(d.day())) {
                    ++count;
                    QCOMPARE(Recurrence::nth(task, count), d);
                }
                QCOMPARE(Recurrence::ordinal(task, d), count);
            }
        }
    }
    void profileRulesAndNoOpApplication() {
        CloudProfile p;
        p.host = "::1";
        p.token = "test-token";
        QString error;
        QCOMPARE(p.url(&error, true).host(), QString("::1"));
        QVERIFY(error.isEmpty());
        p.host = "host/path";
        p.url(&error, true);
        QVERIFY(!error.isEmpty());
        p.host = "example.test";
        QVERIFY(p.apply(&error));
        const auto token = QSettings().value("cloud/token");
        QVERIFY(p.apply(&error));
        QCOMPARE(QSettings().value("cloud/token"), token);
        QSettings().setValue("cloud/enabled", false);
    }
    void cryptoWorkAllowsEventsAndRejectsCancelledResults() {
        QTemporaryDir dir;
        VaultRepository vault(dir.filePath("vault"));
        NoteDepartment notes(&vault);
        int ticks = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, [&] { ++ticks; });
        timer.start(1);
        QVERIFY(notes.create("a-long-test-password"));
        QVERIFY(ticks > 1);
        notes.lock();
        QTimer::singleShot(10, &notes, [&] { notes.lock(); });
        QVERIFY(!notes.unlock("a-long-test-password"));
        QVERIFY(!notes.unlocked());
        QVERIFY(notes.unlock("a-long-test-password"));
    }
    void damagedConfigurationIsProtected() {
        QTemporaryDir dir;
        QFile original(dir.filePath("config.json"));
        QVERIFY(original.open(QIODevice::WriteOnly));
        original.write("{broken");
        original.close();
        HelpJsonIOEngine engine(dir.path(), "config.json");
        engine.read();
        engine.endWrite({{"edited", true}});
        QVERIFY(!engine.flush());
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("{broken"));
        original.close();
        QVERIFY(engine.recover(true));
        QVERIFY(engine.read()["edited"].toBool());
        QCOMPARE(QDir(dir.path()).entryList({"*.damaged-*"}, QDir::Files).size(), 1);
    }
    void unrelatedNoteFieldsMergeAndSameFieldConflicts() {
        QTemporaryDir dir;
        VaultRepository vault(dir.filePath("vault"));
        NoteDepartment notes(&vault);
        QVERIFY(notes.save({{"id", "one"}, {"kind", 2}, {"title", "base"}, {"body", "old"}}));
        NoteCard card(notes.entry("one"), &notes);
        card.expand();
        auto fields = card.findChild<NoteFields *>();
        auto remote = notes.entry("one");
        remote["body"] = "remote body";
        QVERIFY(notes.save(remote));
        fields->title->setText("local title");
        QVERIFY(card.save());
        QCOMPARE(notes.entry("one")["body"].toString(), QString("remote body"));
        QCOMPARE(fields->body->toPlainText(), QString("remote body"));
        remote = notes.entry("one");
        remote["body"] = "second remote";
        QVERIFY(notes.save(remote));
        fields->body->setPlainText("local body");
        QVERIFY(!card.save());
        QCOMPARE(notes.entry("one")["body"].toString(), QString("second remote"));
        QVERIFY(notes.remove("one"));
        QVERIFY(!card.save());
        QCOMPARE(fields->body->toPlainText(), QString("local body"));
    }
    void sqliteLockRecoveryAndCorruption() {
        QTemporaryDir dir;
        TaskRepository tasks(dir.filePath("tasks"));
        QVERIFY(tasks.save({{"id", "one"}, {"title", "one"}}));
        const auto connection = "optimize-lock";
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(dir.filePath("tasks"));
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("BEGIN IMMEDIATE"));
            QVERIFY(!tasks.save({{"id", "two"}}));
            QVERIFY(q.exec("ROLLBACK"));
            QVERIFY(tasks.save({{"id", "two"}}));
            QVERIFY(tasks.ready());
            QVERIFY(q.exec("UPDATE tasks SET payload='broken' WHERE id='one'"));
            QVERIFY(!tasks.probe());
            QVERIFY(!tasks.ready());
            QVERIFY(tasks.save({{"id", "three"}}));
            QVERIFY(!tasks.ready());
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
    }
    void interruptedSettingsBatchReplays() {
        const auto a = Paths::data("config/test-a.json"), b = Paths::data("config/test-blocked/value.json");
        QVERIFY(FileBatch::write(a, "old"));
        QFile block(Paths::data("config/test-blocked"));
        QVERIFY(block.open(QIODevice::WriteOnly));
        block.write("block");
        block.close();
        QVERIFY(!FileBatch::commit({{a, "new"}, {b, "new-b"}}));
        {
            QFile old(a);
            QVERIFY(old.open(QIODevice::ReadOnly));
            QCOMPARE(old.readAll(), QByteArray("old"));
        }
        QVERIFY(!QFile::exists(FileBatch::journal()));
        QVERIFY(QFile::remove(block.fileName()));
        QVERIFY(FileBatch::commit({{a, "new"}, {b, "new-b"}}));
        QVERIFY(FileBatch::recover());
        QFile f(a);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("new"));
        QFile g(b);
        QVERIFY(g.open(QIODevice::ReadOnly));
        QCOMPARE(g.readAll(), QByteArray("new-b"));
        QVERIFY(!QFile::exists(FileBatch::journal()));
    }
    void mailCacheKeepsStarredAndPinnedAndEmptyLoaded() {
        auto config = ConfigManager::getInstance();
        auto policy = config->getMailBasical();
        policy.storageLimit = 1;
        config->setMailBasical(policy);
        auto mail = MailManager::getInstance();
        QJsonArray messages;
        for (int i = 1; i <= 4; ++i)
            messages.append(QJsonObject{
                {"id", QString("1:%1").arg(i)}, {"from", "sender"}, {"subject", "subject"}, {"date", i}});
        QVERIFY(mail->saveBatch("cache@test", {{"messages", messages}, {"validity", "1"}, {"cursor", 4}}));
        QVERIFY(mail->setStar("cache@test", "1:1", true));
        QVERIFY(mail->updateMailBody("cache@test", "1:1", "star"));
        mail->pinBody("cache@test", "1:2");
        QVERIFY(mail->updateMailBody("cache@test", "1:2", "pinned"));
        QVERIFY(mail->updateMailBody("cache@test", "1:3", "old"));
        QVERIFY(mail->updateMailBody("cache@test", "1:4", ""));
        QVERIFY(mail->bodyLoaded("cache@test", "1:1"));
        QVERIFY(mail->bodyLoaded("cache@test", "1:2"));
        QVERIFY(!mail->bodyLoaded("cache@test", "1:3"));
        QVERIFY(mail->bodyLoaded("cache@test", "1:4"));
        mail->pinBody();
    }
    void largeCatalogAndMailSnapshotsReuseValidatedData() {
        QTemporaryDir isolated;
        MailAccountManager::getInstance()->initialize();
        QJsonArray entries, metadata, messages;
        QStringList order;
        for (int i=0;i<1500;++i) {
            const auto id=QString("note-%1").arg(i);
            order.prepend(id);
            entries.append(QJsonObject{{"id",id},{"kind",2},{"title",id},{"body","body"},{"createdAt","2026-01-01T00:00:00Z"}});
            metadata.append(QJsonObject{{"id",QString("secret-%1").arg(i)},{"title","metadata only"}});
        }
        for (int i=0;i<10000;++i) messages.append(QJsonObject{{"id",QString("70:%1").arg(i+1)},{"subject","summary"},{"date",i+1}});
        VaultRepository vault(isolated.filePath("vault"));
        QVERIFY(vault.write({{"version",2},{"entries",metadata}}));
        NoteRepository source(vault.path()+".notes.json");QVERIFY(source.save(entries,true));QVERIFY(source.saveOrder(2,order));
        NoteDepartment notes(&vault);
        TaskRepository tasks(isolated.filePath("tasks.db"));DDLDepartment ddl(&tasks);
        auto mail=MailManager::getInstance();
        QVERIFY(mail->saveBatch("performance@test",{{"messages",messages},{"validity","70"},{"cursor",10000}}));
        CloudSync sync(&tasks,&ddl,&notes);
        QElapsedTimer timer;timer.start();const auto first=sync.snapshot();const auto cold=timer.elapsed();
        timer.restart();
        for(int i=0;i<100;++i){QCOMPARE(notes.catalog().size(),3000);QCOMPARE(notes.entry("note-750")["body"].toString(),QString("body"));}
        const auto catalogMs=timer.elapsed();
        timer.restart();QVERIFY(mail->setStar("performance@test","70:5000",true));const auto updated=sync.snapshot();const auto editMs=timer.elapsed();
        QCOMPARE(first.size(),updated.size());
        const auto identity=QString::fromLatin1(QCryptographicHash::hash(QByteArray("performance@test")+'\0'+"70:5000",QCryptographicHash::Sha256).toHex());
        QVERIFY(updated["mailflag:"+identity].toObject()["payload"].toObject()["starred"].toBool());
        qInfo()<<"Performance sample: 10000 mails, 1500 notes, 1500 secret metadata; cold snapshot ms"<<cold<<"100 cached catalog lookups ms"<<catalogMs<<"star commit and incremental snapshot ms"<<editMs;
        const auto wasEnabled=QSettings().value("cloud/enabled");
        const auto restore=qScopeGuard([&]{QSettings().setValue("cloud/enabled",wasEnabled);});
        QSettings().setValue("cloud/enabled",true);
        timer.restart();sync.capture();const auto initialCapture=timer.elapsed();
        QVERIFY(sync.healthy);
        auto shadow=sync.state["pending"].toObject();
        for(auto it=shadow.begin();it!=shadow.end();++it){auto r=it.value().toObject();r["rev"]=1;it.value()=r;}
        sync.state["shadow"]=shadow;sync.state["pending"]=QJsonObject();QVERIFY(sync.save());
        timer.restart();for(int i=0;i<100;++i)sync.capture();const auto unchangedCapture=timer.elapsed();
        QCOMPARE(sync.lastCaptureCount,0);
        timer.restart();QVERIFY(mail->setStar("performance@test","70:5000",false));sync.capture();const auto changedCapture=timer.elapsed();
        QCOMPARE(sync.lastCaptureCount,1);QVERIFY(sync.healthy);
        const auto queued=sync.state["pending"].toObject();QCOMPARE(queued.size(),1);
        QVERIFY(!queued["mailflag:"+identity].toObject()["payload"].toObject()["starred"].toBool());
        qInfo()<<"Capture including queue persistence: initial ms"<<initialCapture<<"100 unchanged captures ms"<<unchangedCapture<<"star commit + snapshot + diff + persisted queue ms"<<changedCapture;
    }
};
QTEST_MAIN(OptimizeTests)
#include "test_optimize.moc"
