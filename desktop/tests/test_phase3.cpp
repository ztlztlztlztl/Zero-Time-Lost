#include <QtTest>
#include <QtWidgets>
#include "ddldepartment.h"
#include "deadlineeditor.h"
#include "deadlineagendawidget.h"
#include "deadlinecalendarwidget.h"
#include "deadlinewidget.h"
#include "homeinfowidget.h"
#include "advancedfilter.h"
#include "configmanager.h"
#include "paths.h"
#include "recurrence.h"

class Phase3Tests:public QObject {
    Q_OBJECT
    QTemporaryDir storage;
    QJsonObject task(const QString &id,const QString &type,const QDate &date,int start=9,int end=10){
        return {{"id",id},{"type",type},{"title",id},{"note","详细说明"},{"repeat","none"},{"hasDue",true},{"due",QDateTime(date,QTime(type=="label"?0:end,0)).toString(Qt::ISODate)},{"start",QDateTime(date,QTime(start,0)).toString(Qt::ISODate)}};
    }
    int rowFor(QTableWidget *grid,int minute){for(int row=0;row<grid->rowCount();++row)if(grid->item(row,0)->data(DeadlineCalendarWidget::RowStartRole).toInt()<=minute&&minute<grid->item(row,0)->data(DeadlineCalendarWidget::RowEndRole).toInt())return row;return -1;}
    void snapshot(QWidget &widget,const QString &name){const auto path=qEnvironmentVariable("ZTL_QA_DIR");if(path.isEmpty())return;QDir().mkpath(path);QTest::qWait(30);widget.grab().save(path+"/phase3-"+name+".png");}
    QList<QAbstractButton*> cards(QWidget &widget,const QString &title={}){QList<QAbstractButton*> result;for(auto card:widget.findChildren<QAbstractButton*>("ddlAgendaCard"))if(title.isEmpty()||card->property("title")==title)result.append(card);return result;}
private slots:
    void initTestCase(){
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");QApplication::setFont(QFont("Microsoft YaHei UI",10));qputenv("ZTL_DATA_DIR",storage.path().toUtf8());Paths::initialize();
        QCoreApplication::setOrganizationName("ZTL-Tests");QCoreApplication::setApplicationName("Phase3");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,storage.path());ConfigManager::getInstance()->initialize();ConfigManager::getInstance()->setCommonTags({"学习","生活"});
    }
    void typesAndValidation(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);const auto day=QDate::currentDate().addDays(3);
        auto t=task("interval","interval",day);QVERIFY(dept.save(t));t["start"]=QDateTime(day.addDays(-1),QTime(9,0)).toString(Qt::ISODate);QVERIFY(!dept.save(t));t=task("interval","interval",day,10,10);QVERIFY(!dept.save(t));
        t=task("interval","ddl",day);QVERIFY(!dept.save(t));
        t=task("label","label",day);t["tags"]="a,b,c,d";t["priority"]=1;t["remind"]=true;QVERIFY(dept.save(t));QCOMPARE(dept.task("label")["tags"].toString(),QString());QVERIFY(!dept.task("label")["remind"].toBool());QCOMPARE(dept.task("label")["priority"].toInt(),0);
        t["repeat"]="daily";QVERIFY(!dept.save(t));
        auto all=dept.occurrences(day,day);for(const auto &o:all){QVERIFY(!dept.complete(o));QVERIFY(!dept.snooze(o));QVERIFY(!dept.setOccurrence(o.taskId,o.key,{{"done",true}}));}
        QCOMPARE(dept.daySummary(day).today,0);
    }
    void intervalRecurrenceAndHistory(){
        QTemporaryDir dir;const auto path=dir.filePath("tasks");const auto day=QDate::currentDate().addDays(5);QString movedKey;
        {
            TaskRepository repo(path);DDLDepartment dept(&repo);auto t=task("series","interval",day);t["repeat"]="daily";t["countLimited"]=true;t["countLimit"]=3;QVERIFY(dept.save(t));auto all=dept.occurrences(day,day.addDays(10));QCOMPARE(all.size(),3);QCOMPARE(all[1].start,QDateTime(day.addDays(1),QTime(9,0)));
            QCOMPARE(Recurrence::remaining(dept.task("series"),all[0].start),3);QCOMPARE(Recurrence::remaining(dept.task("series"),all[0].due.addSecs(-1)),3);QCOMPARE(Recurrence::remaining(dept.task("series"),all[0].due),2);
            QVERIFY(dept.setOccurrence("series",all[1].key,{{"note","仅本次备注"}}));movedKey=all[1].key;
            const auto movedStart=all[1].start.addDays(8),movedEnd=all[1].due.addDays(8);
            QVERIFY(!dept.setOccurrence("series",movedKey,{{"start",movedEnd.toString(Qt::ISODate)},{"due",movedStart.toString(Qt::ISODate)}}));
            QVERIFY(dept.setOccurrence("series",movedKey,{{"start",movedStart.toString(Qt::ISODate)},{"due",movedEnd.toString(Qt::ISODate)}}));
            auto moved=dept.occurrences(movedStart.date(),movedEnd.date());QCOMPARE(moved.size(),1);QCOMPARE(moved.first().start,movedStart);QCOMPARE(moved.first().note,QString("仅本次备注"));QCOMPARE(Recurrence::remaining(dept.task("series"),all[2].due),1);
            QVERIFY(dept.listOccurrences(movedEnd,true).isEmpty());QVERIFY(dept.purgeExpired(movedEnd));QCOMPARE(dept.occurrences(day,day.addDays(10)).size(),3);
            auto replacement=task("new","interval",day.addDays(2),13,14);replacement["repeat"]="daily";replacement["countLimited"]=true;replacement["remainingCount"]=2;QVERIFY(dept.splitSeries("series",day.addDays(2),replacement));QCOMPARE(dept.occurrences(day,day.addDays(10)).size(),4);
        }
        TaskRepository repo(path);DDLDepartment dept(&repo);QVERIFY(dept.task("series")["instances"].toObject().contains(movedKey));
    }
    void retainedDDLAndLabels(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);const auto yesterday=QDate::currentDate().addDays(-1);QVERIFY(dept.save(task("done","ddl",yesterday)));auto all=dept.occurrences(yesterday,yesterday);QVERIFY(dept.complete(all.first()));QVERIFY(dept.purgeExpired(QDateTime::currentDateTime()));QVERIFY(!dept.task("done").isEmpty());QVERIFY(dept.listOccurrences(QDateTime::currentDateTime(),true).isEmpty());QVERIFY(dept.occurrences(yesterday,yesterday).first().done);
        QVERIFY(dept.save(task("label","label",yesterday)));QVERIFY(dept.listOccurrences(QDateTime::currentDateTime(),true).isEmpty());QCOMPARE(dept.occurrences(yesterday,yesterday).size(),2);
        DeadlineCalendarWidget calendar(&dept);calendar.resize(1100,760);calendar.setDate(yesterday);calendar.show();auto grid=calendar.findChild<QTableWidget*>("ddlCalendarGrid");bool found=false;for(int r=0;r<grid->rowCount();++r)for(int c=0;c<7;++c){auto cell=grid->item(r,c);if(cell->data(DeadlineCalendarWidget::DateRole).toDate()==yesterday){found=true;QCOMPARE(cell->data(DeadlineCalendarWidget::TitlesRole).toStringList(),QStringList{"done"});QCOMPARE(cell->data(DeadlineCalendarWidget::LabelsRole).toStringList(),QStringList{"label"});}}
        QVERIFY(found);
    }
    void noReminderOrTodoAndHomeContext(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);dept.setQuiet(0,0);const auto day=QDate::currentDate();const auto now=QDateTime(day,QTime(9,30));
        for(const auto &id:{"班会","讨论"}){auto t=task(id,"interval",day);t["remind"]=true;QVERIFY(dept.save(t));}
        QVERIFY(dept.save(task("版本更新","label",day)));QVERIFY(dept.save(task("周末","label",day)));QVERIFY(dept.save(task("汇报讲义","ddl",day)));QCOMPARE(dept.daySummary(day).today,1);QCOMPARE(dept.currentIntervals(now).size(),2);QVERIFY(dept.currentIntervals(QDateTime(day,QTime(10,0))).isEmpty());QCOMPARE(dept.labelsToday(now).size(),2);
        QSignalSpy notices(&dept,&DDLDepartment::reminder);dept.checkReminders(QDateTime(day,QTime(9,45)).toSecsSinceEpoch());QCOMPARE(notices.size(),1); // DDL alone
        HomeInfoWidget info;info.resize(1000,400);info.refreshDateTime(now);info.setCalendarContext(dept.labelsToday(now),dept.currentIntervals(now));info.show();auto labels=info.findChild<QLabel*>("homeCalendarLabels"),current=info.findChild<QLabel*>("homeCurrentIntervals");QVERIFY(labels->isVisible());QVERIFY(current->isVisible());QVERIFY(labels->text().contains("今天"));QVERIFY(current->text().contains("班会")&&current->text().contains("讨论"));QVERIFY(labels->y()<current->y());QVERIFY(current->y()<info.findChild<QLabel*>("homeClock")->y());snapshot(info,"home");info.setCalendarContext({},{});QVERIFY(labels->isHidden());QVERIFY(current->isHidden());
    }
    void typeEditorAndFilter(){
        DeadlineEditor editor({},QDateTime(QDate::currentDate().addDays(1),QTime(10,0)),false,false);editor.show();auto type=editor.findChild<QComboBox*>("calendarType");QCOMPARE(type->count(),3);type->setCurrentIndex(type->findData("label"));QVERIFY(editor.findChild<QWidget*>("labelDate")->isVisible());QVERIFY(editor.findChild<QWidget*>("ddlTagChoices")->isHidden());QVERIFY(editor.findChild<QComboBox*>("ddlRemind")->isHidden());editor.findChild<QLineEdit*>("ddlTitle")->setText("更新日");QCOMPARE(editor.values()["repeat"].toString(),QString("none"));snapshot(editor,"editor-label");
        type->setCurrentIndex(type->findData("interval"));QVERIFY(editor.findChild<QWidget*>("intervalStart")->isVisible());QVERIFY(editor.findChild<QComboBox*>("ddlRemind")->isHidden());auto start=editor.findChild<DeadlineDateTimeInput*>("intervalStart");editor.findChild<DeadlineDateTimeInput*>("ddlDue")->findChild<QSpinBox*>("ddlMinute")->setValue(5);start->findChild<QSpinBox*>("ddlHour")->setValue(11);QSignalSpy saved(&editor,&DeadlineEditor::submitted);editor.findChild<QPushButton*>("ddlSave")->click();QCOMPARE(saved.size(),0);QVERIFY(editor.findChild<QLabel*>("ddlEditorFeedback")->text().contains("早于"));start->findChild<QSpinBox*>("ddlHour")->setValue(9);editor.findChild<QPushButton*>("ddlSave")->click();QCOMPARE(saved.size(),1);snapshot(editor,"editor-interval");
        DeadlineEditor fixed(editor.values(),QDateTime::fromString(editor.values()["due"].toString(),Qt::ISODate),true,false);QVERIFY(!fixed.findChild<QComboBox*>("calendarType")->isEnabled());
        AdvancedFilterDialog filter(AdvancedFilterDialog::Calendar,{},nullptr);auto ddl=filter.findChild<QCheckBox*>("filterDDL"),interval=filter.findChild<QCheckBox*>("filterIntervals");ddl->setChecked(false);interval->setChecked(false);filter.findChild<QPushButton*>("filterApply")->click();QCOMPARE(filter.result(),0);QVERIFY(!filter.findChild<QLabel*>("filterError")->text().isEmpty());interval->setChecked(true);QVERIFY(filter.criteria().matchesCalendarType("interval"));QVERIFY(!filter.criteria().matchesCalendarType("ddl"));QVERIFY(filter.criteria().matchesCalendarType("label"));
    }
    void agendaSelectionEndpointsAndFiltering(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);const auto day=QDate::currentDate().addDays(2);
        for(const auto &t:{task("会议","interval",day,9,12),task("交稿","ddl",day,9,10),task("休息日","label",day)})QVERIFY(dept.save(t));
        DeadlineAgendaWidget agenda;agenda.resize(1040,720);agenda.setOccurrences(dept.occurrences(day,day),day,false);agenda.show();QTest::qWait(20);QCOMPARE(cards(agenda).size(),3);QCOMPARE(cards(agenda,"会议").size(),2);QCOMPARE(agenda.findChildren<QAbstractButton*>("ddlComplete").size(),1);QVERIFY(agenda.findChild<QPushButton*>("ddlAllDayLabel"));
        auto endpoints=cards(agenda,"会议");QSignalSpy detail(&agenda,&DeadlineAgendaWidget::detailRequested);QTest::mouseClick(endpoints[0],Qt::LeftButton);QCOMPARE(detail.size(),0);QVERIFY(endpoints[0]->property("selected").toBool());QVERIFY(endpoints[1]->property("selected").toBool());snapshot(agenda,"agenda-highlight");
        QTest::mouseDClick(endpoints[1],Qt::LeftButton);QTRY_COMPARE(detail.size(),1);cards(agenda,"交稿").first()->click();QVERIFY(!endpoints[0]->property("selected").toBool());QVERIFY(cards(agenda,"交稿").first()->property("selected").toBool());
        DeadlineWidget page(&dept);page.resize(1100,760);page.show();
        QTimer::singleShot(0,&page,[&]{auto dialog=page.findChild<QDialog*>("advancedFilter");QVERIFY(dialog);dialog->findChild<QCheckBox*>("filterDDL")->setChecked(false);dialog->findChild<QLineEdit*>("filterKeywords")->setText("无匹配");dialog->findChild<QCheckBox*>("filterLimitDates")->setChecked(true);dialog->findChild<QPushButton*>("filterApply")->click();});page.findChild<QPushButton*>("ddlFilter")->click();QVERIFY(cards(page).isEmpty());QVERIFY(page.findChild<QPushButton*>("ddlAllDayLabel"));snapshot(page,"filtered-label");
    }
    void compressedWeekAndItemHitTesting(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);auto monday=QDate::currentDate().addDays(14);monday=monday.addDays(1-monday.dayOfWeek());
        QVERIFY(dept.save(task("跨度","interval",monday,9,13)));QVERIFY(dept.save(task("清晨DDL","ddl",monday.addDays(1),6,6)));QVERIFY(dept.save(task("全天假日","label",monday)));
        DeadlineCalendarWidget calendar(&dept);calendar.resize(1120,760);calendar.setMonthView(false);calendar.setDate(monday);calendar.show();auto grid=calendar.findChild<QTableWidget*>("ddlCalendarGrid");QVERIFY(grid->rowCount()<24);
        QCOMPARE(grid->item(0,0)->data(DeadlineCalendarWidget::RowStartRole).toInt(),0);QCOMPARE(grid->item(0,0)->data(DeadlineCalendarWidget::RowEndRole).toInt(),360);
        for(int hour=9;hour<=13;++hour){const int row=rowFor(grid,hour*60);QVERIFY(row>=0);QCOMPARE(grid->item(row,0)->data(DeadlineCalendarWidget::RowEndRole).toInt()-grid->item(row,0)->data(DeadlineCalendarWidget::RowStartRole).toInt(),60);}
        QList<QPair<int,int>> ranges;for(int r=0;r<grid->rowCount();++r)ranges.append({grid->item(r,0)->data(DeadlineCalendarWidget::RowStartRole).toInt(),grid->item(r,0)->data(DeadlineCalendarWidget::RowEndRole).toInt()});AdvancedFilter f;f.keywords="跨度";f.calendarDDL=false;calendar.setFilter(f);QCOMPARE(grid->rowCount(),ranges.size());for(int r=0;r<ranges.size();++r)QCOMPARE(grid->item(r,0)->data(DeadlineCalendarWidget::RowStartRole).toInt(),ranges[r].first);
        const int row=rowFor(grid,540);grid->scrollToItem(grid->item(row,0),QAbstractItemView::PositionAtTop);QTest::qWait(20);const auto rect=grid->visualItemRect(grid->item(row,0));QSignalSpy detail(&calendar,&DeadlineCalendarWidget::detailRequested),dates(&calendar,&DeadlineCalendarWidget::dateActivated);const auto point=rect.topLeft()+QPoint(30,17);QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,point);QCOMPARE(detail.size(),0);QVERIFY(grid->item(rowFor(grid,660),0)->data(DeadlineCalendarWidget::HighlightStartRole).isValid());snapshot(calendar,"week-highlight");
        QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,point);QTRY_COMPARE(detail.size(),1);QCOMPARE(dates.size(),0);QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,rect.bottomRight()-QPoint(10,8));QCOMPARE(dates.size(),1);
        calendar.setFilter({});snapshot(calendar,"week");calendar.setMonthView(true);snapshot(calendar,"month");calendar.resize(840,620);snapshot(calendar,"month-narrow");
    }
    void controllerDetailsAndHistoryActions(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);const auto day=QDate::currentDate().addDays(2);QVERIFY(dept.save(task("会议","interval",day)));QVERIFY(dept.save(task("更新日","label",day)));
        DeadlineWidget page(&dept);page.resize(1100,760);page.show();auto agenda=page.findChild<DeadlineAgendaWidget*>();bool edited=false;
        QTimer::singleShot(30,&page,[&]{auto dialog=page.findChild<QDialog*>("ddlDetail");QVERIFY(dialog);QPushButton *modify=nullptr;for(auto b:dialog->findChildren<QPushButton*>()){QVERIFY(b->text()!="完成本次");QVERIFY(b->text()!="稍后10分钟");if(b->text()=="编辑")modify=b;}QVERIFY(modify);
            QTimer::singleShot(30,&page,[&]{auto editor=page.findChild<DeadlineEditor*>("ddlEditor");QVERIFY(editor);QVERIFY(!editor->findChild<QComboBox*>("calendarType")->isEnabled());editor->findChild<QLineEdit*>("ddlTitle")->setText("新会议");editor->findChild<QPushButton*>("ddlSave")->click();edited=true;});modify->click();
        });QTest::mouseDClick(cards(*agenda,"会议").first(),Qt::LeftButton);QTRY_VERIFY(edited);QCOMPARE(dept.task("会议")["title"].toString(),QString("新会议"));QCOMPARE(cards(*agenda,"新会议").size(),2);
        bool deleted=false;QTimer::singleShot(30,&page,[&]{auto dialog=page.findChild<QDialog*>("ddlDetail");QVERIFY(dialog);for(auto b:dialog->findChildren<QPushButton*>()){QVERIFY(b->text()!="完成本次");if(b->text()=="删除"){b->click();deleted=true;return;}}});QTest::mouseDClick(page.findChild<QPushButton*>("ddlAllDayLabel"),Qt::LeftButton);QTRY_VERIFY(deleted);QVERIFY(dept.task("更新日").isEmpty());
        auto old=task("历史完成","ddl",day.addDays(-5));QVERIFY(dept.save(old));for(const auto &o:dept.occurrences(day.addDays(-5),day.addDays(-5)))if(o.taskId=="历史完成")QVERIFY(dept.complete(o));auto tabs=page.findChild<QTabBar*>("ddlTabs");tabs->setCurrentIndex(2);auto calendar=page.findChild<DeadlineCalendarWidget*>();calendar->setDate(day.addDays(-5));auto grid=calendar->findChild<QTableWidget*>("ddlCalendarGrid");QTableWidgetItem *cell=nullptr;for(int r=0;r<grid->rowCount();++r)for(int c=0;c<7;++c)if(grid->item(r,c)->data(DeadlineCalendarWidget::TitlesRole).toStringList().contains("历史完成"))cell=grid->item(r,c);QVERIFY(cell);
        deleted=false;QTimer::singleShot(30,&page,[&]{auto dialog=page.findChild<QDialog*>("ddlDetail");QVERIFY(dialog);for(auto b:dialog->findChildren<QPushButton*>())if(b->text()=="删除"){b->click();deleted=true;return;}});QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(cell).topLeft()+QPoint(20,44));QTRY_VERIFY(deleted);QVERIFY(dept.task("历史完成").isEmpty());
    }
    void monthlyOverflowAndHeaderLabels(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);auto day=QDate::currentDate().addDays(14);day=day.addDays(1-day.dayOfWeek());for(int i=0;i<8;++i){QVERIFY(dept.save(task(QString("DDL%1").arg(i),"ddl",day)));QVERIFY(dept.save(task(QString("标签%1").arg(i),"label",day)));}
        DeadlineCalendarWidget calendar(&dept);calendar.resize(900,600);calendar.setDate(day);calendar.show();auto grid=calendar.findChild<QTableWidget*>("ddlCalendarGrid");QTableWidgetItem *cell=nullptr;for(int r=0;r<grid->rowCount();++r)if(grid->item(r,0)->data(DeadlineCalendarWidget::DateRole).toDate()==day)cell=grid->item(r,0);QVERIFY(cell);QSignalSpy details(&calendar,&DeadlineCalendarWidget::detailRequested);bool overflow=false;
        auto choose=[&]{auto list=calendar.findChild<QListWidget*>("calendarOverflow");QVERIFY(list);QCOMPARE(list->count(),8);overflow=true;list->itemActivated(list->item(7));};
        QTimer::singleShot(0,&calendar,choose);QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(cell).topLeft()+QPoint(45,15));QVERIFY(overflow);QTRY_COMPARE(details.size(),1);
        overflow=false;QTimer::singleShot(0,&calendar,choose);const auto rect=grid->visualItemRect(cell);QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(rect.left()+20,rect.top()+35+qMax(0,(rect.height()-39)/25-1)*25+8));QVERIFY(overflow);QTRY_COMPARE(details.size(),2);
        calendar.setMonthView(false);QTest::qWait(20);overflow=false;QTimer::singleShot(0,&calendar,choose);QTest::mouseDClick(grid->horizontalHeader()->viewport(),Qt::LeftButton,Qt::NoModifier,QPoint(grid->columnWidth(0)/2,66+2*22+8));QVERIFY(overflow);QTRY_COMPARE(details.size(),3);snapshot(calendar,"many-labels");
    }
    void boundaryRefreshHidesEndedIntervals(){
        const auto now=QDateTime::currentDateTime();if(now.time()>QTime(23,59,53))QSKIP("接近午夜，边界验证留到次日");
        QTemporaryDir dir;TaskRepository repo(dir.filePath("tasks"));DDLDepartment dept(&repo);auto t=task("短事务","interval",now.date());t["start"]=now.addSecs(2).toString(Qt::ISODate);t["due"]=now.addSecs(4).toString(Qt::ISODate);QSignalSpy boundary(&dept,&DDLDepartment::temporalChanged);QVERIFY(dept.save(t));DeadlineWidget page(&dept);page.resize(1000,700);page.show();QCOMPARE(cards(page,"短事务").size(),2);QTRY_VERIFY_WITH_TIMEOUT(!dept.currentIntervals(QDateTime::currentDateTime()).isEmpty(),2800);QTRY_VERIFY_WITH_TIMEOUT(cards(page,"短事务").isEmpty(),3500);QVERIFY(boundary.size()>=2);QVERIFY(!dept.task("短事务").isEmpty());QCOMPARE(dept.occurrences(now.date(),now.date()).size(),1);
    }
};
QTEST_MAIN(Phase3Tests)
#include "test_phase3.moc"
