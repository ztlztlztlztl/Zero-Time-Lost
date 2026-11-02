#include <QtTest>
#include <QtWidgets>
#include "paths.h"
#include "autostart.h"
#include "configmanager.h"
#include "draftstore.h"
#include "popuppolicy.h"
#include "tagrules.h"
#include "notewidget.h"
#include "appcontext.h"
#include "mainwindow.h"
#include "homewidget.h"
#include "testdialogs.h"
class Phase1Tests:public QObject {
    Q_OBJECT
    QTemporaryDir data;
    NoteCard *card(NoteWidget &page,const QString &id){for(auto c:page.findChildren<NoteCard*>())if(c->id()==id)return c;return nullptr;}
private slots:
    void initTestCase(){
        qputenv("ZTL_DATA_DIR",data.path().toUtf8());Paths::initialize();
        QCoreApplication::setOrganizationName("ZTL-Tests");QCoreApplication::setApplicationName("Phase1");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,Paths::data("config"));
        ConfigManager::getInstance()->initialize();auto mail=ConfigManager::getInstance()->getMailBasical();mail.pollingInterval=-1;ConfigManager::getInstance()->setMailBasical(mail);
    }
    void portablePaths(){
        QCOMPARE(Paths::root(),data.path());
        for(const auto &dir:{"config","database","cache","logs","backups","tmp","crash","update-cache"})QVERIFY(QFileInfo(Paths::data(dir)).isDir());
        QCOMPARE(QDir::fromNativeSeparators(qEnvironmentVariable("TEMP")),Paths::data("tmp"));
        qunsetenv("ZTL_DATA_DIR");QCOMPARE(Paths::root(),QCoreApplication::applicationDirPath()+"/data");qputenv("ZTL_DATA_DIR",data.path().toUtf8());
        qputenv("ZTL_SCRIPTS_DIR",data.path().toUtf8());QCOMPARE(Paths::scripts(),data.path());qunsetenv("ZTL_SCRIPTS_DIR");
        qputenv("ZTL_PYTHON","fixture-python");QCOMPARE(Paths::python(),QString("fixture-python"));qunsetenv("ZTL_PYTHON");
        QVERIFY(AutoStart::command().startsWith('"'));QVERIFY(AutoStart::command().endsWith("\" --autostart"));
    }
    void tagRulesAndIndependentCatalogs(){
        QCOMPARE(TagRules::weight("中文abc"),7);QVERIFY(TagRules::error("一二三四五").isEmpty());QVERIFY(TagRules::error("abcdefghij").isEmpty());
        for(const auto &tag:QStringList{"一二三四五a","abcdefghijk","", "   ","a,b","a，b","a\nb",QString("a")+QChar(1)})QVERIFY(!TagRules::error(tag).isEmpty());
        auto config=ConfigManager::getInstance();QSignalSpy calendar(config,&ConfigManager::deadlineChanged),note(config,&ConfigManager::noteTagsChanged);
        config->setCommonTags({"日程"});config->setNoteTags({"纸条","中文abc"});QCOMPARE(calendar.size(),1);QCOMPARE(note.size(),1);
        DeadlineTagPicker cal(""),notes("",nullptr,true);
        QCOMPARE(cal.findChild<QListWidget*>("ddlAvailableTags")->count(),1);QCOMPARE(notes.findChild<QListWidget*>("ddlAvailableTags")->count(),2);
        for(auto engine:config->findChildren<HelpJsonIOEngine*>())engine->forceWrite();config->initialize();
        QCOMPARE(config->commonTags(),QStringList{"日程"});QCOMPARE(config->noteTags(),QStringList({"纸条","中文abc"}));
    }
    void popupBoundaries(){
        QWidget owner,external;owner.resize(500,400);owner.show();external.show();
        QDialog dialog(&owner);int dismissed=0;PopupPolicy policy(&dialog,[&]{++dismissed;dialog.reject();});dialog.show();
        QEvent deactivated(QEvent::WindowDeactivate);QCoreApplication::sendEvent(&dialog,&deactivated);QVERIFY(dialog.isVisible());
        QTest::mouseClick(&external,Qt::LeftButton);QCoreApplication::processEvents();QVERIFY(dialog.isVisible());
        QDialog nested(&dialog);nested.setWindowModality(Qt::ApplicationModal);nested.show();QCoreApplication::processEvents();
        QTest::mouseClick(&owner,Qt::LeftButton);QCoreApplication::processEvents();QVERIFY(dialog.isVisible());QVERIFY(nested.isVisible());nested.hide();
        QTest::mouseClick(&owner,Qt::LeftButton);QTRY_COMPARE(dismissed,1);QVERIFY(!dialog.isVisible());
        dialog.show();owner.hide();QCOMPARE(dismissed,2);
    }
    void encryptedDraftRoundTrip(){
        QJsonObject draft{{"title","中文草稿"},{"body","phase1-private-draft"}};
        QVERIFY(DraftStore::save("phase1",draft));QCOMPARE(DraftStore::read("phase1"),draft);
        QFile file(DraftStore::path("phase1"));QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(!file.readAll().contains("phase1-private-draft"));file.close();DraftStore::clear("phase1");QVERIFY(DraftStore::read("phase1").isEmpty());
    }
    void filteredOrderPersistenceAndExpandedEditor(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);
        for(int i=0;i<4;++i)QVERIFY(dept.save({{"id",QString::number(i)},{"kind",2},{"title",i%2?"hidden":"visible"},{"body","body"}}));
        QVERIFY(dept.reorder(2,{"0","1","2","3"}));NoteWidget page(&dept);page.resize(1000,700);page.show();QTest::qWait(30);
        auto a=card(page,"0"),b=card(page,"2");a->expand();auto fields=a->findChild<NoteFields*>();fields->body->setPlainText("keep expanded body");
        page.findChild<QLineEdit*>("noteSearch")->setText("visible");
        auto canvas=page.findChild<QWidget*>("noteCanvas");QMimeData mime;mime.setData("application/x-ztl-note","0");
        QDragEnterEvent enter(b->geometry().center(),Qt::MoveAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&enter);QVERIFY(enter.isAccepted());
        QDropEvent drop(b->geometry().center(),Qt::MoveAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&drop);QVERIFY(drop.isAccepted());
        QCOMPARE(dept.order(2),QStringList({"2","1","0","3"}));QVERIFY(a->expanded());QCOMPARE(a->findChild<NoteFields*>(),fields);
        QVERIFY(a->save());QCOMPARE(dept.entry("0")["body"].toString(),QString("keep expanded body"));
        NoteDepartment reopened(&vault);QCOMPARE(reopened.order(2),dept.order(2));QVERIFY(!dept.reorder(2,{"0","0"}));
        QVERIFY(dept.create("test-master-password"));QVERIFY(dept.save({{"id","secret1"},{"kind",0},{"title","a"},{"body","secret"}}));QVERIFY(dept.save({{"id","secret2"},{"kind",0},{"title","b"},{"body","secret"}}));dept.lock();
        QVERIFY(dept.reorder(0,{"secret2","secret1"}));QVERIFY(!dept.unlocked());QCOMPARE(dept.order(2),QStringList({"2","1","0","3"}));
    }
    void noteCreationDraftDismissAndResume(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);NoteWidget page(&dept);page.resize(950,700);page.show();
        QTimer::singleShot(40,&page,[&]{auto dialog=qobject_cast<QDialog*>(activeTestDialog());QVERIFY(dialog);dialog->findChild<NoteFields*>()->title->setText("恢复的纸条");QTest::mouseClick(&page,Qt::LeftButton,Qt::NoModifier,QPoint(1,1));});
        page.findChild<QPushButton*>("noteAdd")->click();QVERIFY(dept.catalog().isEmpty());
        bool restored=false;QTimer::singleShot(40,&page,[&]{auto dialog=qobject_cast<QDialog*>(activeTestDialog());QVERIFY(dialog);restored=dialog->findChild<NoteFields*>()->title->text()=="恢复的纸条";dialog->findChild<QPushButton*>("noteCreate")->click();});
        page.findChild<QPushButton*>("noteAdd")->click();QVERIFY(restored);QCOMPARE(dept.catalog().size(),1);
    }
    void windowStateAndNavigation(){
        QSettings().setValue("idleMinutes",0);QSettings().setValue("closeToTray",true);
        AppContext app;app.clock.stop();
        {MainWindow window(&app);window.resize(700,600);window.move(20,20);window.show();QTest::qWait(30);QCOMPARE(window.windowTitle(),QString("Z.T.L"));
            QAction *calendar=nullptr;for(auto a:window.actions()){QVERIFY(a->shortcut()!=QKeySequence("Alt+D"));if(a->shortcut()==QKeySequence("Alt+C"))calendar=a;}QVERIFY(calendar);calendar->trigger();QVERIFY(!window.findChild<HomeWidget*>()->isVisible());
            QCOMPARE(window.findChildren<QLabel*>("pageTitle").size(),4);
            window.showMaximized();window.saveWindowState();QVERIFY(QSettings().value("windowState").toInt()&Qt::WindowMaximized);
            window.showFullScreen();window.saveWindowState();QVERIFY(QSettings().value("windowState").toInt()&Qt::WindowFullScreen);
            window.showNormal();window.resize(700,600);window.saveWindowState();
            window.findChild<QSystemTrayIcon*>()->show();window.close();QVERIFY(!window.isVisible());QVERIFY(window.findChild<HomeWidget*>()->isVisibleTo(&window));window.openFromTray();QVERIFY(window.isVisible());}
        {MainWindow restored(&app);restored.show();QTest::qWait(30);QCOMPARE(restored.size(),QSize(700,600));}
        app.shutdown();
    }
};
QTEST_MAIN(Phase1Tests)
#include "test_phase1.moc"
