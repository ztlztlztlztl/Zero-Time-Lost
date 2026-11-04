#include <QtTest>
#include <QtWidgets>
#include "testdialogs.h"
#include "advancedfilter.h"
#include "recurrence.h"
#include "deadlineeditor.h"
#include "deadlinewidget.h"
#include "deadlinecalendarwidget.h"
#include "deadlineagendawidget.h"
#include "notewidget.h"
#include "bookmarktarget.h"
#include "mailmanager.h"
#include "configmanager.h"
#include "settingwidget.h"
#include "paths.h"
class TestUrlReceiver:public QObject {
    Q_OBJECT
public:
    QList<QUrl> received;
public slots:
    void capture(const QUrl &url){received.append(url);}
};
class Phase2Tests:public QObject {
    Q_OBJECT
    QTemporaryDir data;
    QJsonObject recurring(const QString &id,const QDateTime &due,int count,const QString &rule="daily"){
        return {{"id",id},{"title",id},{"repeat",rule},{"hasDue",true},{"due",due.toString(Qt::ISODate)},{"countLimited",true},{"countLimit",count}};
    }
    void snapshot(QWidget &widget,const QString &name){
        const auto folder=qEnvironmentVariable("ZTL_QA_DIR");if(folder.isEmpty())return;QDir().mkpath(folder);widget.grab().save(folder+"/"+name+".png");
    }
private slots:
    void initTestCase(){
        qputenv("ZTL_DATA_DIR",data.path().toUtf8());Paths::initialize();
        QCoreApplication::setOrganizationName("ZTL-Tests");QCoreApplication::setApplicationName("Phase2");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,data.path());
        ConfigManager::getInstance()->initialize();ConfigManager::getInstance()->setCommonTags({"学习","生活","项目","其他"});ConfigManager::getInstance()->setNoteTags({"学习","生活","项目","其他"});
        MailManager::getInstance()->initialize();
    }
    void filterMatching(){
        AdvancedFilter f;QVERIFY(!f.active());f.keywords="  中文  ALPHA\tBeta ";
        QVERIFY(f.matchesTitle("alpha 中文 Beta"));QVERIFY(!f.matchesTitle("alpha 中文"));
        f.tags={"学习","项目"};QVERIFY(f.matchesTags("项目,生活,学习"));QVERIFY(!f.matchesTags("学习,生活"));
        QVERIFY(f.matchesDate({}));f.limitDates=true;f.from=QDate(2030,1,1);f.to=f.from.addDays(1);
        QVERIFY(f.matchesDate(f.from.startOfDay()));QVERIFY(f.matchesDate(QDateTime(f.to,QTime(23,59,59))));
        QVERIFY(!f.matchesDate({}));QVERIFY(!f.matchesDate(f.to.addDays(1).startOfDay()));
    }
    void filterDialogAndDiscard(){
        QWidget owner;owner.resize(1000,900);owner.show();
        AdvancedFilterDialog dialog(AdvancedFilterDialog::Calendar,{},&owner);
        QVERIFY(!dialog.criteria().active());auto limit=dialog.findChild<QCheckBox*>("filterLimitDates");
        auto from=dialog.findChild<DeadlineDateTimeInput*>("filterFrom"),to=dialog.findChild<DeadlineDateTimeInput*>("filterTo");
        QVERIFY(!from->isEnabled());QCOMPARE(dialog.criteria().to,dialog.criteria().from.addYears(100));
        limit->setChecked(true);QVERIFY(from->isEnabled());QVERIFY(to->isEnabled());
        auto available=dialog.findChild<QListWidget*>("ddlAvailableTags");
        for(int i=0;i<3;++i)available->itemClicked(available->item(0));
        QCOMPARE(dialog.criteria().tags.size(),3);QVERIFY(!(available->item(0)->flags()&Qt::ItemIsEnabled));
        from->findChild<QSpinBox*>("ddlYear")->setValue(2040);to->findChild<QSpinBox*>("ddlYear")->setValue(2030);
        dialog.findChild<QPushButton*>("filterApply")->click();QVERIFY(dialog.findChild<QLabel*>("filterError")->text().contains("结束"));
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);DeadlineWidget page(&dept);page.resize(1000,800);page.show();
        auto button=page.findChild<QPushButton*>("ddlFilter");QVERIFY(applyTestFilter(page,"ddlFilter"));QCOMPARE(button->text(),QString("启用筛选"));
        QTimer::singleShot(0,&page,[&]{auto popup=page.findChild<QDialog*>("advancedFilter");popup->findChild<QLineEdit*>("filterKeywords")->setText("discard");QTest::mouseClick(&page,Qt::LeftButton,Qt::NoModifier,QPoint(1,1));});
        button->click();QCOMPARE(button->text(),QString("启用筛选"));
        bool blank=false;QTimer::singleShot(0,&page,[&]{auto popup=page.findChild<QDialog*>("advancedFilter");blank=popup->findChild<QLineEdit*>("filterKeywords")->text().isEmpty();snapshot(*popup,"phase2-calendar-filter");popup->reject();});
        button->click();QVERIFY(blank);
        AdvancedFilterDialog secret(AdvancedFilterDialog::Secret,{},&owner);QVERIFY(!secret.findChild<DeadlineTagPicker*>());
        AdvancedFilterDialog mail(AdvancedFilterDialog::Mail,{"a@example.test","b@example.test"},&owner);auto accounts=mail.findChild<QListWidget*>("filterAccounts");accounts->item(0)->setCheckState(Qt::Checked);accounts->item(1)->setCheckState(Qt::Checked);QCOMPARE(mail.criteria().accounts.size(),2);
    }
    void finiteRecurrenceAndPersistence(){
        QTemporaryDir dir;const auto path=dir.filePath("tasks");const auto due=QDateTime(QDate::currentDate().addDays(10),QTime(12,0));
        {
            TaskRepository repo(path);DDLDepartment dept(&repo);QVERIFY(dept.save(recurring("daily",due,3)));
            auto task=dept.task("daily");QCOMPARE(Recurrence::remaining(task,due.addSecs(-1)),3);QCOMPARE(Recurrence::remaining(task,due),2);
            auto all=dept.occurrences(due.date(),due.date().addDays(10));QCOMPARE(all.size(),3);
            QVERIFY(dept.complete(all[1]));QCOMPARE(Recurrence::remaining(dept.task("daily"),due.addSecs(-1)),2);
            QVERIFY(!dept.complete(dept.occurrences(due.date().addDays(1),due.date().addDays(1)).first()));
            QVERIFY(dept.removeOccurrence(all[0]));QCOMPARE(Recurrence::remaining(dept.task("daily"),due.addSecs(-1)),1);
            QVERIFY(dept.purgeExpired(due.addDays(1)));QCOMPARE(Recurrence::remaining(dept.task("daily"),due.addDays(1)),1);
            QCOMPARE(Recurrence::remaining(dept.task("daily"),due.addDays(2)),0);
            QVERIFY(dept.listOccurrences(due.addDays(3),true).isEmpty());QVERIFY(dept.listOccurrences(due.addDays(300),false).isEmpty());
            // A moved occurrence keeps its allocated slot and is still discoverable after the nominal tail.
            QVERIFY(dept.setOccurrence("daily",all[2].key,{{"due",due.addDays(20).toString(Qt::ISODate)}}));
            QCOMPARE(Recurrence::remaining(dept.task("daily"),due.addDays(10)),1);
            QCOMPARE(dept.listOccurrences(due.addDays(10),false).size(),1);
            QCOMPARE(dept.occurrences(due.date().addDays(20),due.date().addDays(20)).size(),1);
        }
        TaskRepository repo(path);DDLDepartment reopened(&repo);QCOMPARE(Recurrence::remaining(reopened.task("daily"),due.addDays(10)),1);
    }
    void weeklyMonthlyAndRemainingEdit(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);
        const auto due=QDateTime(QDate(2032,1,31),QTime(9,0));
        auto task=recurring("monthly",due,4,"monthly");task["monthdays"]=QJsonArray{1,31};QVERIFY(dept.save(task));
        auto all=dept.occurrences(QDate(2032,1,1),QDate(2032,5,31));QCOMPARE(all.size(),4);
        QCOMPARE(all[0].due.date(),QDate(2032,1,31));QCOMPARE(all[1].due.date(),QDate(2032,2,1));QCOMPARE(all[2].due.date(),QDate(2032,3,1));QCOMPARE(all[3].due.date(),QDate(2032,3,31));
        QCOMPARE(Recurrence::remaining(dept.task("monthly"),QDateTime(QDate(2032,2,29),QTime(23,0))),2);
        auto replacement=recurring("ignored",all[2].due,1,"monthly");replacement["monthdays"]=QJsonArray{1,31};replacement["remainingCount"]=2;
        QVERIFY(dept.splitSeries("monthly",all[2].due.date(),replacement));
        all=dept.occurrences(QDate(2032,1,1),QDate(2032,5,31));QCOMPARE(all.size(),4);
        task=recurring("weekly",QDateTime(QDate(2032,3,1),QTime(9,0)),5,"weekly");task["weekdays"]=QJsonArray{1,3,5};QVERIFY(dept.save(task));
        QList<TaskOccurrence> weekly;for(const auto &o:dept.occurrences(QDate(2032,3,1),QDate(2032,4,1)))if(o.taskId=="weekly")weekly.append(o);
        QCOMPARE(weekly.size(),5);for(const auto &o:weekly)QVERIFY(QList<int>({1,3,5}).contains(o.due.date().dayOfWeek()));
        for(int n=0;n<weekly.size();++n)QCOMPARE(Recurrence::nth(dept.task("weekly"),n+1),weekly[n].due.date());
        auto invalid=recurring("bad",due,0);QVERIFY(!dept.save(invalid));
    }
    void editingRemainingDoesNotReplenishEarlyActions(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);
        const auto due=QDateTime(QDate::currentDate().addDays(10),QTime(9,0));
        QVERIFY(dept.save(recurring("edit",due,10)));auto all=dept.occurrences(due.date(),due.date().addDays(20));QVERIFY(dept.complete(all.last()));
        auto task=dept.task("edit");task["remainingCount"]=1;QVERIFY(dept.save(task));QCOMPARE(Recurrence::remaining(dept.task("edit")),1);
        QCOMPARE(dept.listOccurrences(due.addDays(2),false).size(),0);
        // Splitting before the old tail reports only dates that still belong to each segment.
        auto replacement=recurring("new",due.addDays(2),2);QVERIFY(dept.splitSeries("edit",due.date().addDays(2),replacement));QCOMPARE(Recurrence::remaining(dept.task("edit"),due.addDays(2)),0);
    }
    void exhaustedRecurrenceCanEditOrDisable(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);
        const auto due=QDateTime(QDate::currentDate().addDays(-2),QTime(9,0));
        QVERIFY(dept.save(recurring("exhausted",due,1)));
        QCOMPARE(Recurrence::remaining(dept.task("exhausted")),0);
        DeadlineEditor single(dept.task("exhausted"),due,true,true);
        single.findChild<QLineEdit*>("ddlTitle")->setText("edited occurrence");
        QSignalSpy submitted(&single,&DeadlineEditor::submitted);
        single.findChild<QPushButton*>("ddlSave")->click();QCOMPARE(submitted.size(),1);
        DeadlineEditor editor(dept.task("exhausted"),due,true,false);
        QVERIFY(!editor.findChild<QCheckBox*>("ddlCountLimited"));
        QCOMPARE(editor.values()["remainingCount"].toInt(),1);
        auto saveEditor=[&]{auto value=editor.values();value["id"]="exhausted";return dept.save(value);};
        editor.findChild<QCheckBox*>("ddlRepeatEnabled")->setChecked(false);
        QVERIFY(saveEditor());QCOMPARE(dept.task("exhausted")["repeat"].toString(),QString("none"));
    }
    void splitCancelsOldSnoozedInstance(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);dept.setQuiet(0,0);
        const auto now=QDateTime::currentDateTime(),due=now.addSecs(60);auto task=recurring("snooze",due,3);task["remind"]=false;QVERIFY(dept.save(task));
        auto first=dept.occurrences(due.date(),due.date()).first();QVERIFY(dept.snooze(first));
        auto replacement=recurring("new",due.addDays(1),2);replacement["remind"]=false;QVERIFY(dept.splitSeries("snooze",due.date(),replacement));
        QSignalSpy notices(&dept,&DDLDepartment::reminder);dept.checkReminders(now.addSecs(601).toSecsSinceEpoch());QCOMPARE(notices.size(),0);
    }
    void largeAgendaCanExpandAll(){
        QList<TaskOccurrence> items;for(int i=0;i<410;++i){TaskOccurrence o;o.taskId="series";o.key=QString::number(i);o.title="entry";o.due=QDateTime(QDate::currentDate().addDays(i),QTime(9,0));items.append(o);}
        DeadlineAgendaWidget agenda;agenda.resize(900,700);agenda.setOccurrences(items,QDate::currentDate(),true);
        QCOMPARE(agenda.findChildren<QAbstractButton*>("ddlAgendaCard").size(),200);
        agenda.findChild<QPushButton*>("ddlAgendaMore")->click();QCOMPARE(agenda.findChildren<QAbstractButton*>("ddlAgendaCard").size(),400);
        agenda.findChild<QPushButton*>("ddlAgendaMore")->click();QCOMPARE(agenda.findChildren<QAbstractButton*>("ddlAgendaCard").size(),410);QVERIFY(!agenda.findChild<QPushButton*>("ddlAgendaMore"));
        agenda.setOccurrences(items,QDate::currentDate(),true);QCOMPARE(agenda.findChildren<QAbstractButton*>("ddlAgendaCard").size(),410);
    }
    void recurrenceEditor(){
        QWidget owner;owner.resize(920,900);owner.show();
        const auto due=QDateTime(QDate::currentDate().addDays(2),QTime(9,0));
        DeadlineEditor editor({},due,false,false,&owner);editor.show();
        auto enabled=editor.findChild<QCheckBox*>("ddlRepeatEnabled");
        QVERIFY(!enabled->isChecked());QCOMPARE(editor.values()["repeat"].toString(),QString("none"));enabled->setChecked(true);
        QVERIFY(!editor.findChild<QCheckBox*>("ddlCountLimited"));
        QTRY_VERIFY(editor.findChild<QWidget*>("ddlRecurrence")->isVisible());auto count=editor.findChild<QSpinBox*>("ddlRemainingCount");QVERIFY(count->isEnabled());QCOMPARE(count->minimum(),1);count->setValue(0);QCOMPARE(count->value(),1);count->setValue(7);
        QVERIFY(editor.values()["countLimited"].toBool());
        auto repeat=editor.findChild<QComboBox*>("ddlRepeat");repeat->setCurrentIndex(repeat->findData("weekly"));
        auto days=editor.findChild<QWidget*>("ddlWeekdays");auto buttons=days->findChildren<QPushButton*>();QCOMPARE(buttons.size(),7);
        for(auto button:buttons)button->setChecked(false);
        QTest::mouseClick(buttons[0],Qt::LeftButton);QTest::mouseClick(buttons[4],Qt::LeftButton);
        for(auto button:buttons)QCOMPARE(button->y(),buttons[0]->y());
        QCOMPARE(editor.values()["weekdays"].toArray(),QJsonArray({1,5}));QCOMPARE(editor.values()["remainingCount"].toInt(),7);
        QTest::qWait(20);snapshot(editor,"phase2-recurrence-weekly");
        repeat->setCurrentIndex(repeat->findData("monthly"));QTest::qWait(20);
        auto month=editor.findChild<QWidget*>("ddlMonthdays");auto grid=qobject_cast<QGridLayout*>(month->layout());QCOMPARE(grid->rowCount(),5);QCOMPARE(grid->columnCount(),7);
        auto monthButtons=month->findChildren<QPushButton*>();QCOMPARE(monthButtons.size(),35);
        for(auto button:monthButtons)button->setChecked(false);
        QTest::mouseClick(monthButtons[0],Qt::LeftButton);QTest::mouseClick(monthButtons[30],Qt::LeftButton);
        for(int i=31;i<35;++i){QVERIFY(!monthButtons[i]->isEnabled());QVERIFY(monthButtons[i]->text().isEmpty());}
        QCOMPARE(editor.values()["monthdays"].toArray(),QJsonArray({1,31}));
        auto scroll=editor.findChild<QScrollArea*>("ddlEditorScroll");scroll->ensureWidgetVisible(monthButtons[30]);QTest::qWait(20);snapshot(editor,"phase2-recurrence-monthly");
        QVERIFY(scroll->viewport()->rect().contains(monthButtons[30]->mapTo(scroll->viewport(),monthButtons[30]->rect().center())));
        repeat->setCurrentIndex(repeat->findData("daily"));QTest::qWait(20);snapshot(editor,"phase2-recurrence-daily");
        QCOMPARE(editor.values()["weekdays"].toArray(),QJsonArray({1,5}));
        enabled->setChecked(false);QVERIFY(editor.findChild<QWidget*>("ddlRecurrence")->isHidden());QCOMPARE(editor.values()["repeat"].toString(),QString("none"));QVERIFY(!editor.values()["countLimited"].toBool());
    }
    void calendarRangeExpandsAll(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);const auto date=QDate::currentDate().addDays(10);
        auto task=recurring("ALPHA beta",QDateTime(date,QTime(9,0)),4);task["tags"]="学习,项目";task["priority"]=1;QVERIFY(dept.save(task));
        DeadlineWidget page(&dept);page.resize(1000,800);page.show();bool applied=false;
        QTimer::singleShot(0,&page,[&]{
            auto popup=page.findChild<QDialog*>("advancedFilter");popup->findChild<QLineEdit*>("filterKeywords")->setText("alpha BETA");
            popup->findChild<QCheckBox*>("filterLimitDates")->setChecked(true);
            for(const auto &pair:QList<QPair<QString,QDate>>{{"filterFrom",date},{"filterTo",date.addDays(2)}}){
                auto input=popup->findChild<DeadlineDateTimeInput*>(pair.first);input->findChild<QSpinBox*>("ddlYear")->setValue(pair.second.year());input->findChild<QSpinBox*>("ddlMonth")->setValue(pair.second.month());input->findChild<QSpinBox*>("ddlDay")->setValue(pair.second.day());
            }
            popup->findChild<QPushButton*>("filterApply")->click();applied=true;
        });page.findChild<QPushButton*>("ddlFilter")->click();QVERIFY(applied);
        auto agenda=page.findChild<DeadlineAgendaWidget*>();QCOMPARE(agenda->findChildren<QAbstractButton*>("ddlAgendaCard").size(),3);snapshot(page,"phase2-filtered-agenda");
        auto calendar=page.findChild<DeadlineCalendarWidget*>();auto tabs=page.findChild<QTabBar*>("ddlTabs");
        for(int view:{1,2}){
            tabs->setCurrentIndex(view);calendar->setDate(date);auto grid=calendar->findChild<QTableWidget*>("ddlCalendarGrid");
            for(int row=0;row<grid->rowCount();++row)for(int col=0;col<7;++col){
                auto cell=grid->item(row,col);const auto day=cell->data(DeadlineCalendarWidget::DateRole).toDate();
                QCOMPARE(cell->data(DeadlineCalendarWidget::CountRole).toInt(),day>=date&&day<=date.addDays(2)&&(view==2||row==9)?1:0);
            }
        }
        tabs->setCurrentIndex(0);QCOMPARE(agenda->findChildren<QAbstractButton*>("ddlAgendaCard").size(),3);
    }
    void mailQueryBeforePagination(){
        const auto date=QDate(2031,5,8);QJsonArray messages;
        for(int i=0;i<30;++i)messages.append(QJsonObject{{"id",QString("91:%1").arg(i+1)},{"subject",i%3==0?"ALPHA beta":"other"},{"from","alpha sender"},{"date",date.startOfDay().addSecs(i).toSecsSinceEpoch()}});
        auto manager=MailManager::getInstance();QVERIFY(manager->saveBatch("first@example.test",{{"messages",messages}},false));
        QVERIFY(manager->saveBatch("second@example.test",{{"messages",QJsonArray{QJsonObject{{"id","92:1"},{"subject","alpha BETA"},{"from","sender"},{"date",date.addDays(1).startOfDay().toSecsSinceEpoch()}}}}},false));
        AdvancedFilter f;f.keywords="alpha beta";f.accounts={"first@example.test"};f.limitDates=true;f.from=date;f.to=date;
        QCOMPARE(manager->queryFiltered(f,0,4).size(),4);QCOMPARE(manager->queryFiltered(f,0,100,4).size(),6);
        f.accounts.append("second@example.test");f.to=date.addDays(1);QCOMPARE(manager->queryFiltered(f,0,100).size(),11);
        f.read=0;QCOMPARE(manager->queryFiltered(f,0,100).size(),11);manager->checkMail("second@example.test","92:1");QCOMPARE(manager->queryFiltered(f,0,100).size(),10);
        f.read=1;manager->setStar("second@example.test","92:1",true);f.starred=1;QCOMPARE(manager->queryFiltered(f,0,100).size(),1);
        f.keywords="alpha sender";QVERIFY(manager->queryFiltered(f,0,100).isEmpty());
        MailBrief unicode;unicode.ownerEmail="unicode@example.test";unicode.uid="1:1";unicode.subject="ÉCOLE 学习";manager->addMail(unicode);
        AdvancedFilter caseFilter;caseFilter.keywords="école 学习";QCOMPARE(manager->queryFiltered(caseFilter,0,100).size(),1);
    }
    void browserPersistenceAndDrag(){
        QTemporaryDir dir;VaultRepository vault(dir.filePath("vault"));NoteDepartment dept(&vault);
        QJsonObject entry{{"id","web"},{"kind",1},{"title","Browser"},{"targetType","website"},{"target","https://example.test/a?q=中文&x=1"},{"browser","chrome"}};
        QVERIFY(dept.save(entry));NoteDepartment reopened(&vault);QCOMPARE(reopened.entry("web")["browser"].toString(),QString("chrome"));
        NoteFields fields(entry);QCOMPARE(fields.browser->currentData().toString(),QString("chrome"));fields.browser->setCurrentIndex(fields.browser->findData("edge"));QVERIFY(fields.dirty());QCOMPARE(fields.values()["browser"].toString(),QString("edge"));
        fields.targetType->setCurrentIndex(fields.targetType->findData("file"));QVERIFY(fields.browser->isHidden());QVERIFY(!fields.values().contains("browser"));
        QVERIFY(BookmarkTarget::browserCandidates("chrome").join(' ').contains("chrome.exe"));QVERIFY(BookmarkTarget::browserCandidates("edge").join(' ').contains("msedge.exe"));QVERIFY(BookmarkTarget::firstExecutable({dir.filePath("missing.exe")}).isEmpty());
        entry["browser"]="unknown";QVERIFY(!BookmarkTarget::validate(entry).isEmpty());
        QVERIFY(dept.save({{"id","paper"},{"kind",2},{"title","drag me"}}));NoteWidget page(&dept);page.resize(900,700);page.show();QTest::qWait(30);
        NoteCard *paper=nullptr;for(auto card:page.findChildren<NoteCard*>())if(card->id()=="paper")paper=card;QVERIFY(paper);
        paper->setDragReady(true);snapshot(*paper,"phase2-drag-ready");paper->setDragReady(false);
        QSignalSpy ready(paper,&NoteCard::dragReadyChanged);
        QTimer::singleShot(650,&page,[]{
            QDrag::cancel();
            // Windows OLE observes cancellation on its next native pointer event.
            if(QGuiApplication::platformName()=="windows"){
                const auto position=QCursor::pos();QCursor::setPos(position+QPoint(1,0));QCursor::setPos(position);
            }
        });QTest::mousePress(paper,Qt::LeftButton,Qt::NoModifier,QPoint(20,20));QTest::qWait(800);QTest::mouseRelease(paper,Qt::LeftButton);
        QVERIFY(ready.size()>=2);QCOMPARE(ready.first().first().toBool(),true);QCOMPARE(ready.last().first().toBool(),false);QVERIFY(!paper->property("dragReady").toBool());
    }
    void missingBrowserFallsBackToDefault(){
        QTemporaryDir dir;TestUrlReceiver receiver;
        QDesktopServices::setUrlHandler("https",&receiver,"capture");
        struct HandlerCleanup{~HandlerCleanup(){QDesktopServices::unsetUrlHandler("https");}} cleanup;
        const QUrl url("https://example.test/a?q=中文&x=1");QString error="old error";
        QVERIFY(BookmarkTarget::openWebsite(url,{dir.filePath("missing-browser.exe")},&error));
        QVERIFY(error.isEmpty());QCOMPARE(receiver.received,QList<QUrl>{url});
        QJsonObject entry{{"targetType","website"},{"target",url.toString()},{"browser","default"}};
        QVERIFY(BookmarkTarget::open(entry,&error));QCOMPARE(receiver.received.size(),2);
        QCOMPARE(receiver.received.last(),url);
    }
    void settingGroups(){
        SettingWidget page;QStringList titles;for(auto label:page.findChildren<QLabel*>())if(label->property("role")=="settingTitle")titles.append(label->text());
        QCOMPARE(titles,QStringList({"Home","Mail","Calendar","Note","Advance"}));
    }
    void cleanupTestCase(){MailManager::getInstance()->clearUp();}
};
QTEST_MAIN(Phase2Tests)
#include "test_phase2.moc"
