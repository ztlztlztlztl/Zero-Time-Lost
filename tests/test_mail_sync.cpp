#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include "appcontext.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"

class MailSyncTests: public QObject {
    Q_OBJECT
private slots:
    void failedSeenRetriesAfterLocalCommit(){
        QTemporaryDir data;qputenv("ZTL_DATA_DIR",data.path().toUtf8());qputenv("ZTL_SCRIPTS_DIR",QByteArray(ZTL_TEST_FIXTURES));
        QCoreApplication::setOrganizationName("ZTL-Tests");QCoreApplication::setApplicationName("MailSyncRetry");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,data.path());
        AppContext app;app.clock.stop();MailAccount account;account.name="中文测试";account.address="retry@example.test";account.server="fail-seen-once.test";account.port="993";account.code="fixture";MailAccountManager::getInstance()->setMailAccount(account);
        auto repo=MailManager::getInstance();QSignalSpy completed(&app.mail,&MailGetService::pollingOver);QSignalSpy status(&app.mail,&MailGetService::status);
        app.mail.mailPolling();QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,10000);QCOMPARE(repo->pendingSeen(account.address).size(),4);QCOMPARE(repo->unreadCount(),qint64(4));QVERIFY(status.last().first().toString().contains("下次同步重试"));
        app.mail.mailPolling();QTRY_COMPARE_WITH_TIMEOUT(completed.size(),2,10000);QVERIFY(repo->pendingSeen(account.address).isEmpty());QCOMPARE(repo->unreadCount(),qint64(4));app.shutdown();
    }
    void firstSyncThroughProcessAndDatabase() {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        qputenv("ZTL_DATA_DIR",data.path().toUtf8());
        qputenv("ZTL_SCRIPTS_DIR",QByteArray(ZTL_TEST_FIXTURES));
        QCoreApplication::setOrganizationName("ZTL-Tests");
        QCoreApplication::setApplicationName("MailSync");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,data.path());
        AppContext app;
        app.clock.stop();
        MailAccount account;
        account.name="中文邮箱账号";
        account.address="user@example.test";
        account.server="imap.example.test";
        account.port="993";
        account.code="fixture-only";
        MailAccountManager::getInstance()->setMailAccount(account);
        auto repo=MailManager::getInstance();
        QVERIFY(repo->syncState(account.address).isEmpty());
        QSignalSpy completed(&app.mail,&MailGetService::pollingOver);
        QSignalSpy statuses(&app.mail,&MailGetService::status);
        app.mail.mailPolling();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,10000);
        QVERIFY2(statuses.last().first().toString().startsWith("同步完成"),qPrintable(statuses.last().first().toString()));
        QVERIFY(completed.last().first().toBool());
        QVERIFY(repo->pendingSeen(account.address).isEmpty());
        QCOMPARE(repo->query({},account.address,0,100).size(),4);
        QCOMPARE(repo->syncState(account.address)["cursor"].toInteger(),qint64(4));
        QCOMPARE(repo->syncState(account.address)["validity"].toString(),QString("9"));

        app.mail.mailPolling();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(),2,10000);
        QVERIFY(!completed.last().first().toBool());
        QVERIFY(repo->pendingSeen(account.address).isEmpty());
        QCOMPARE(repo->query({},account.address,0,100).size(),4);
        app.mail.mailHistory();
        QTRY_COMPARE_WITH_TIMEOUT(completed.size(),3,10000);
        QCOMPARE(repo->syncState(account.address)["cursor"].toInteger(),qint64(4));

        QSignalSpy body(&app.mail,&MailGetService::newMailBody);
        app.mail.mailBody(account,"9:4");
        QTRY_COMPARE_WITH_TIMEOUT(body.size(),1,10000);
        QVERIFY(body.first().at(2).toString().contains("Integration body"));
        QCOMPARE(repo->query({},account.address,1,100).size(),4);

        repo->updateMailBody(account.address,"9:4","private cached body");
        repo->setStar(account.address,"9:4",true);repo->checkMail(account.address,"9:4");
        repo->deleteMail(account.address,"9:4");
        QVERIFY(repo->isDeleted(account.address,"9:4"));
        QCOMPARE(repo->getMailBody(account.address,"9:4"),QString());
        auto trash=repo->query({},account.address,3,100);QCOMPARE(trash.size(),1);
        QCOMPARE(trash.first().subject,QString("Integration mail"));
        QVERIFY(trash.first().sender.isEmpty());QCOMPARE(trash.first().timestamp,qint64(0));QVERIFY(!trash.first().starred);QVERIFY(!trash.first().isRead);
        // Late body results and another history batch must not refill a deleted record.
        repo->updateMailBody(account.address,"9:4","late body result");
        QCOMPARE(repo->getMailBody(account.address,"9:4"),QString());
        QJsonObject duplicate{{"messages",QJsonArray{QJsonObject{{"id","9:4"},{"from","ignored"},{"subject","ignored"},{"date",123}}}}};
        QVERIFY(repo->saveBatch(account.address,duplicate,false));QVERIFY(repo->isDeleted(account.address,"9:4"));
        QVERIFY(repo->query({},account.address,3,100).first().sender.isEmpty());

        QSignalSpy restored(&app.mail,&MailGetService::restoreFinished);
        app.mail.restoreMail(account.address,"9:4");
        QTRY_COMPARE_WITH_TIMEOUT(restored.size(),1,10000);QVERIFY(restored.last().at(2).toBool());
        QVERIFY(!repo->isDeleted(account.address,"9:4"));
        QVERIFY(repo->pendingSeen(account.address).contains("9:4"));
        QCOMPARE(repo->query({},account.address,0,100).size(),4);
        QCOMPARE(repo->query({},account.address,0,100).first().sender,QString("sender@example.test"));
        QCOMPARE(repo->syncState(account.address)["cursor"].toInteger(),qint64(4));

        MailBrief missing;missing.ownerEmail=account.address;missing.uid="9:99";missing.subject="Missing on server";
        repo->addMail(missing);repo->deleteMail(account.address,missing.uid);
        app.mail.restoreMail(account.address,missing.uid);
        QTRY_COMPARE_WITH_TIMEOUT(restored.size(),2,10000);QVERIFY(!restored.last().at(2).toBool());
        QVERIFY(repo->isDeleted(account.address,missing.uid));
        QVERIFY(statuses.last().first().toString().contains("不存在"));
        QVERIFY(!repo->restoreMail(account.address,missing.uid,{{"id","9:4"}}));
        const auto baseline=repo->unreadCount();QJsonArray extra;
        for(int i=0;i<1105;++i)extra.append(QJsonObject{{"id",QString("9:%1").arg(200+i)},{"subject","count fixture"},{"from","count@example.test"},{"date",QDateTime::currentSecsSinceEpoch()}});
        QVERIFY(repo->saveBatch(account.address,{{"messages",extra}},false));QCOMPARE(repo->unreadCount(),baseline+1105);
        QCOMPARE(repo->query({},{},1,1000).size(),1000);
        repo->checkMail(account.address,"9:200");QCOMPARE(repo->unreadCount(),baseline+1104);
        repo->deleteMail(account.address,"9:201");QCOMPARE(repo->unreadCount(),baseline+1103);
        app.mail.mailPolling();QTRY_COMPARE_WITH_TIMEOUT(completed.size(),4,15000);QVERIFY(repo->pendingSeen(account.address).isEmpty());QCOMPARE(repo->unreadCount(),baseline+1103);
        auto engine=repo->findChild<HelpSQLiteIOEngine*>();QVERIFY(engine);
        QVERIFY(engine->executeNonQuery("CREATE TRIGGER reject_summary BEFORE INSERT ON mails WHEN NEW.uid='9:99999' BEGIN SELECT RAISE(ABORT,'fixture failure'); END"));
        QVERIFY(!repo->saveBatch(account.address,{{"messages",QJsonArray{QJsonObject{{"id","9:99999"},{"subject","not saved"}}}},{"validity","9"},{"cursor",99999}},true));
        QVERIFY(!repo->pendingSeen(account.address).contains("9:99999"));QCOMPARE(repo->syncState(account.address)["cursor"].toInteger(),qint64(4));
        app.shutdown();
    }
};
QTEST_MAIN(MailSyncTests)
#include "test_mail_sync.moc"
