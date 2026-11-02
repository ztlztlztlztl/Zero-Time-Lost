
#include <QtTest>
#include "testdialogs.h"
#include <QtWidgets>
#include <QTemporaryDir>
#include "appcontext.h"
#include "mainwindow.h"
#include "deadlinewidget.h"
#include "deadlinecalendarwidget.h"
#include "deadlineagendawidget.h"
#include "deadlineeditor.h"
#include "notewidget.h"
#include "mailwidget.h"
#include "mailstyle.h"
#include "mailmanager.h"
#include "settingwidget.h"
#include "settingdeadlinepage.h"
#include "settingmailpage.h"
#include "configmanager.h"
#include "homewidget.h"
#include "homeinfowidget.h"
#include "homebackgroundshader.h"
#include "homedepartment.h"
class PasswordResponder:public QObject{
public:
    int count=0;QString password="test-master-password";
    bool eventFilter(QObject *object,QEvent *event)override{
        if(event->type()==QEvent::Show)if(auto dialog=qobject_cast<QDialog*>(object)){
            if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty()&&(dialog->objectName()=="ddlDetail"||dialog->objectName()=="noteDetail"))QTimer::singleShot(0,dialog,[dialog]{dialog->grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/"+dialog->objectName()+".png");});
        }
        if(event->type()==QEvent::Show)if(auto dialog=qobject_cast<QInputDialog*>(object)){
            ++count;QTimer::singleShot(10,dialog,[this,dialog]{dialog->setTextValue(password);dialog->accept();});
        }
        return false;
    }
};
class UiTests:public QObject{
    Q_OBJECT
private slots:
    void disabledDefaultsPaginationAndKeyboardTags(){
        QTemporaryDir dir;qputenv("ZTL_DATA_DIR",dir.path().toUtf8());
        QCoreApplication::setOrganizationName("ZTL-Tests");QCoreApplication::setApplicationName("Review");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
        ConfigManager::getInstance()->initialize();auto config=ConfigManager::getInstance()->getMailBasical();config.pollingInterval=-1;config.initialNumber=1000;config.earlierNumber=20;ConfigManager::getInstance()->setMailBasical(config);
        for(auto engine:ConfigManager::getInstance()->findChildren<HelpJsonIOEngine*>())engine->forceWrite();
        AppContext app;QSignalSpy statuses(&app.mail,&MailGetService::status);app.clock.poll();QCOMPARE(statuses.size(),0);app.clock.stop();
        QCOMPARE(ConfigManager::getInstance()->getMailBasical().pollingInterval,-1);
        auto defaults=ConfigManager::getInstance()->getDeadlineBasical();defaults.remindTime=-1;ConfigManager::getInstance()->setDeadlineBasical(defaults);
        QCOMPARE(DeadlineBasicalConfig(defaults.toJson()).remindTime,qint64(-1));
        DeadlineEditor editor({},QDateTime::currentDateTime(),false,false);QVERIFY(!editor.values()["remind"].toBool());QCOMPARE(editor.values()["advanceMinutes"].toInt(),0);
        editor.findChild<QComboBox*>("ddlRemind")->setCurrentIndex(1);QVERIFY(editor.values()["remind"].toBool());
        ConfigManager::getInstance()->setCommonTags({"学习","生活"});DeadlineTagPicker picker("学习");picker.show();
        auto selected=picker.findChild<QListWidget*>("ddlSelectedTags"),available=picker.findChild<QListWidget*>("ddlAvailableTags");
        selected->setCurrentRow(0);selected->setFocus();QTest::keyClick(selected,Qt::Key_Space);QCOMPARE(picker.selectedCount(),0);
        available->setCurrentRow(0);available->setFocus();QTest::keyClick(available,Qt::Key_Return);QCOMPARE(picker.selectedCount(),1);
        QJsonArray messages;for(int i=0;i<1003;++i)messages.append(QJsonObject{{"id",QString("9:%1").arg(i+1)},{"subject",QString("邮件 %1").arg(i)},{"from","fixture@example.test"},{"date",i+1}});
        QVERIFY(MailManager::getInstance()->saveBatch("fixture@example.test",{{"messages",messages}},false));
        MailSimpleDisplayWidget list;list.resize(1000,700);list.show();auto rows=list.findChild<QTreeWidget*>("mailRows");auto more=list.findChild<QPushButton*>("mailMore");
        QCOMPARE(rows->topLevelItemCount(),1000);QVERIFY(more->isEnabled());more->click();QCOMPARE(rows->topLevelItemCount(),1003);QVERIFY(!more->isEnabled());
        list.findChild<QLineEdit*>("mailSearch")->setText("not-found-unique");QCOMPARE(rows->topLevelItemCount(),0);QVERIFY(!more->isEnabled());
        app.shutdown();
    }
    void mailReadingModes(){
        MailBodyReader reader;reader.setStyleSheet(MailStyle::sheet);reader.resize(940,700);reader.show();
        MailBrief brief;brief.ownerEmail="student@example.edu";brief.uid="9:5";brief.subject="课程安排与报名说明";brief.sender="教务处 <office@example.edu>";brief.timestamp=QDateTime::currentSecsSinceEpoch();
        const QString html="<html><body bgcolor='#ffffff'><h2 style='color:#000000'>课程安排</h2><p style='color:#000000;background-color:#ffffff'>请在周五前确认选课信息。</p><table border='1' cellpadding='12' bgcolor='#eeeeee'><tr><td bgcolor='#ffffff'><font color='#000000'>高等数学</font></td><td>周三 10:00</td></tr></table><p><a href='https://example.edu'>查看说明</a></p><img src='https://example.invalid/tracking.png'></body></html>";
        reader.loadMail(brief,html);QCoreApplication::processEvents();
        auto mode=reader.findChild<QComboBox*>("mailDisplayMode");auto browser=reader.findChild<QTextBrowser*>("mailContent");QVERIFY(mode&&browser);QCOMPARE(mode->count(),4);QCOMPARE(browser->property("readingTheme").toString(),QString("light"));
        const auto text=reader.plainText();QVERIFY(text.contains("高等数学"));QVERIFY(!browser->openLinks());QVERIFY(!browser->openExternalLinks());
        QVERIFY(browser->document()->resource(QTextDocument::ImageResource,QUrl("https://example.invalid/tracking.png")).isNull());
        auto snapshot=[&](const QString &name){if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())reader.grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/"+name+".png");};
        snapshot("mail-html-auto");mode->setCurrentIndex(2);QCOMPARE(browser->property("readingTheme").toString(),QString("dark"));QCOMPARE(reader.plainText(),text);
        for(auto block=browser->document()->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){
            auto fragment=it.fragment();if(!fragment.isValid())continue;const auto format=fragment.charFormat();
            QVERIFY(!format.hasProperty(QTextFormat::BackgroundBrush));
            if(format.hasProperty(QTextFormat::ForegroundBrush))QVERIFY(format.foreground().color().lightness()>100);
        }
        auto tables=browser->document()->rootFrame()->childFrames();QVERIFY(!tables.isEmpty());
        auto table=qobject_cast<QTextTable*>(tables.first());QVERIFY(table);QVERIFY(!table->cellAt(0,0).format().hasProperty(QTextFormat::BackgroundBrush));snapshot("mail-html-dark");
        mode->setCurrentIndex(1);QCOMPARE(browser->property("readingTheme").toString(),QString("light"));snapshot("mail-html-light");
        mode->setCurrentIndex(3);QCOMPARE(browser->toPlainText(),text);snapshot("mail-html-plain");
        mode->setCurrentIndex(0);reader.loadMail("<pre>普通邮件 &lt;student&gt;\n下一行</pre>");QCOMPARE(browser->property("readingTheme").toString(),QString("dark"));QVERIFY(reader.plainText().contains("<student>"));
        reader.loadMail("<table width='1600'><tr><td>宽表格内容</td></tr></table>");QCoreApplication::processEvents();QVERIFY(browser->horizontalScrollBar()->maximum()>0);
        reader.loadMail(brief,html);reader.resize(760,700);QCoreApplication::processEvents();snapshot("mail-reader-narrow");QVERIFY(reader.findChild<QWidget*>("mailReadingColumn")->width()<=760);
        auto timestamp=reader.findChild<QLabel*>("mailDate");QVERIFY(timestamp->width()>=timestamp->fontMetrics().horizontalAdvance(timestamp->text()));
        reader.showLoading();QVERIFY(reader.findChild<QLabel*>("mailSubject")->isVisible());QVERIFY(!mode->isEnabled());QVERIFY(reader.plainText().isEmpty());
        reader.showError("测试连接失败");auto retry=reader.findChild<QPushButton*>("mailBodyRetry");QVERIFY(retry->isVisible());QSignalSpy retries(&reader,&MailBodyReader::retryRequested);retry->click();QCOMPARE(retries.size(),1);
    }
    void completeFlows(){
        qApp->setFont(QFont("Microsoft YaHei UI",10));QTemporaryDir dir;qputenv("ZTL_DATA_DIR",dir.path().toUtf8());
        QCoreApplication::setOrganizationName("ZTL-Tests");QCoreApplication::setApplicationName("UI");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
        QSettings().setValue("closeToTray",false);QSettings().setValue("idleMinutes",0);
        qputenv("ZTL_SCRIPTS_DIR",QByteArray(ZTL_TEST_FIXTURES));
        AppContext app;app.clock.stop();
        QVERIFY(app.notes.save({{"title","纸条 / Fragment"},{"kind",2},{"body","remember this"}}));
        QVERIFY(app.notes.save({{"title","常用网站"},{"kind",1},{"body","https://example.com"},{"category","学习"}}));
        QVERIFY(app.notes.create("test-master-password"));QVERIFY(app.notes.save({{"title","测试秘密"},{"kind",0},{"body","no-cleartext-cache"}}));app.notes.lock();
        MainWindow window(&app);window.show();QTest::qWait(100);
        PasswordResponder responder;qApp->installEventFilter(&responder);
        auto navigate=[&](Qt::Key key){window.activateWindow();QTest::qWait(30);QTest::keyClick(&window,key,Qt::AltModifier);QCoreApplication::processEvents();};
        auto snapshot=[&](const QString &name){if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())window.grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/"+name+".png");};
        auto button=[](QWidget *widget,const QString &text)->QPushButton*{for(auto b:widget->findChildren<QPushButton*>())if(b->text()==text)return b;return nullptr;};
        auto modal=[&](const QString &name){for(auto d:window.findChildren<QDialog*>())if(d->objectName()==name&&d->isVisible())return d;return static_cast<QDialog*>(nullptr);};
        // Keep failed assertions in a timer from leaving a modal dialog blocking the whole test process.
        QTimer watchdog;watchdog.setInterval(5000);connect(&watchdog,&QTimer::timeout,[] {if(auto d=qobject_cast<QDialog*>(activeTestDialog()))d->reject();});watchdog.start();
        QVERIFY(window.findChildren<QMenuBar*>().isEmpty());
        navigate(Qt::Key_S);auto settings=window.findChild<SettingWidget*>();QVERIFY(settings->isVisible());
        QCOMPARE(QApplication::focusWidget(),settings);
        for(auto list:settings->findChildren<QListWidget*>()){QCOMPARE(list->currentRow(),-1);QVERIFY(list->selectedItems().isEmpty());}
        auto settingsScroll=settings->findChild<QScrollArea*>("settingsScroll");QVERIFY(settingsScroll);
        QCOMPARE(settings->findChildren<QScrollArea*>().size(),1);QVERIFY(settings->findChildren<QTabBar*>().isEmpty());
        for(auto combo:settings->findChildren<NoWheelComboBox*>())QVERIFY(combo->currentIndex()>=0);
        for(const auto &group:{"Home","Mail","DDL","Note","general"})QVERIFY(settings->findChild<QWidget*>("settingsGroup"+QString(group)));
        auto tagPage=settings->findChild<SettingDeadlinePage*>();QVERIFY(tagPage);
        auto commonTags=tagPage->findChild<QListWidget*>("commonTags");QVERIFY(commonTags);
        auto editTag=[&](bool creating,const QString &value){
            button(tagPage,creating?"添加":"修改")->click();QCoreApplication::processEvents();
            auto popup=tagPage->findChild<QDialog*>("tagPopup");if(!popup)return false;
            auto input=popup->findChild<QLineEdit*>("tagInput");if(!input)return false;input->setText(value);
            popup->findChild<QPushButton*>("tagApply")->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);return true;
        };
        QVERIFY(editTag(true,"课程"));QVERIFY(editTag(true,"生活"));
        settingsScroll->ensureWidgetVisible(commonTags);QCoreApplication::processEvents();
        commonTags->setCurrentRow(-1);commonTags->clearSelection();
        auto clickTag=[&](int row){QTest::mouseClick(commonTags->viewport(),Qt::LeftButton,Qt::NoModifier,commonTags->visualItemRect(commonTags->item(row)).center());};
        clickTag(0);QCOMPARE(commonTags->currentRow(),0);QCOMPARE(commonTags->selectedItems().size(),1);
        clickTag(0);QCOMPARE(commonTags->currentRow(),-1);QVERIFY(commonTags->selectedItems().isEmpty());
        clickTag(0);clickTag(1);QCOMPARE(commonTags->currentRow(),1);QCOMPARE(commonTags->selectedItems().size(),1);
        QTest::mouseDClick(commonTags->viewport(),Qt::LeftButton,Qt::NoModifier,commonTags->visualItemRect(commonTags->item(1)).center());
        QTest::mouseRelease(commonTags->viewport(),Qt::LeftButton,Qt::NoModifier,commonTags->visualItemRect(commonTags->item(1)).center());
        QCOMPARE(commonTags->currentRow(),-1);QVERIFY(commonTags->selectedItems().isEmpty());
        commonTags->setCurrentRow(0);QVERIFY(editTag(false,"学习"));button(tagPage,"下移")->click();
        QCOMPARE(ConfigManager::getInstance()->commonTags(),QStringList({"生活","学习"}));
        QVERIFY(editTag(true,"临时"));commonTags->setCurrentRow(2);button(tagPage,"删除")->click();QCOMPARE(commonTags->count(),2);
        button(tagPage,"添加")->click();auto dismissed=tagPage->findChild<QDialog*>("tagPopup");QVERIFY(dismissed);QTest::keyClick(dismissed,Qt::Key_Escape);QVERIFY(!dismissed->isVisible());QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        button(tagPage,"添加")->click();dismissed=tagPage->findChild<QDialog*>("tagPopup");QVERIFY(dismissed);
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())dismissed->grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/settings-popup.png");
        QTest::mouseClick(tagPage,Qt::LeftButton,Qt::NoModifier,QPoint(1,1));QTRY_VERIFY(!dismissed->isVisible());QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        // Validation and asynchronous connection errors must leave the account popup and its draft intact.
        auto mailSettings=settings->findChild<SettingMailPage*>();QVERIFY(mailSettings);
        button(mailSettings,"添加")->click();QPointer<QDialog> accountPopup=mailSettings->findChild<QDialog*>("accountPopup");QVERIFY(accountPopup);
        auto accountField=[&](const QString &name){return accountPopup->findChild<QLineEdit*>(name);};
        auto feedback=accountPopup->findChild<QLabel*>("accountFeedback");QVERIFY(feedback);
        auto testConnection=accountPopup->findChild<QPushButton*>("accountTest");auto saveAccount=accountPopup->findChild<QPushButton*>("accountSave");QVERIFY(testConnection&&saveAccount);
        QInputMethodEvent chineseInput;chineseInput.setCommitString("未完成的账号");QApplication::sendEvent(accountField("accountName"),&chineseInput);testConnection->click();
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(accountPopup&&accountPopup->isVisible());
        QVERIFY(feedback->text().contains("信息不完整"));QCOMPARE(accountField("accountName")->text(),QString("未完成的账号"));QVERIFY(!saveAccount->isEnabled());
        accountField("accountAddress")->setText("popup@example.test");accountField("accountCode")->setText("fixture-only");accountField("accountServer")->setText("offline.invalid");
        accountField("accountPort")->setText("0");testConnection->click();QVERIFY(feedback->text().contains("端口号"));QVERIFY(accountPopup->isVisible());
        accountField("accountPort")->setText("993");testConnection->click();
        QTRY_VERIFY_WITH_TIMEOUT(feedback->text().startsWith("连接失败"),10000);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(accountPopup&&accountPopup->isVisible());QVERIFY(!saveAccount->isEnabled());
        QCOMPARE(accountField("accountAddress")->text(),QString("popup@example.test"));
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())accountPopup->grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/settings-mail-error.png");
        accountField("accountServer")->setText("imap.example.test");testConnection->click();QTRY_VERIFY_WITH_TIMEOUT(saveAccount->isEnabled(),10000);QVERIFY(accountPopup->isVisible());
        button(accountPopup,"取消")->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(accountPopup.isNull());
        // Inputs apply on Enter/focus loss, toggles apply immediately, and no section Save button remains.
        window.activateWindow();QCoreApplication::processEvents();
        auto weatherKey=settings->findChild<QLineEdit*>("weatherKey");QVERIFY(weatherKey);weatherKey->setText("   ");QTest::keyClick(weatherKey,Qt::Key_Return);QCOMPARE(QSettings().value("weatherKey").toString(),QString());
        auto storage=settings->findChild<QLineEdit*>("mailStorageLimit");QVERIFY(storage);settingsScroll->ensureWidgetVisible(storage);storage->setFocus();storage->setText("42");QTest::keyClick(storage,Qt::Key_Return);QCOMPARE(ConfigManager::getInstance()->getMailBasical().storageLimit,42);
        storage->setText("51");settings->findChild<NoWheelComboBox*>("mailPollingInterval")->setFocus();QCoreApplication::processEvents();QCOMPARE(ConfigManager::getInstance()->getMailBasical().storageLimit,51);
        auto animation=settings->findChild<QCheckBox*>("animation");QVERIFY(animation);animation->setChecked(false);QCOMPARE(QSettings().value("animation").toBool(),false);animation->setChecked(true);
        for(auto b:settings->findChildren<QPushButton*>())QVERIFY(!b->text().startsWith("保存"));
        auto combo=settings->findChild<NoWheelComboBox*>("mailPollingInterval");const auto oldIndex=combo->currentIndex();
        QWheelEvent wheel(QPointF(4,4),QPointF(combo->mapToGlobal(QPoint(4,4))),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(combo,&wheel);QCOMPARE(combo->currentIndex(),oldIndex);QVERIFY(!wheel.isAccepted());
        auto groupDDL=settings->findChild<QWidget*>("settingsGroupDDL");settingsScroll->ensureWidgetVisible(groupDDL);QCoreApplication::processEvents();snapshot("settings-ddl");
        navigate(Qt::Key_C);auto ddl=window.findChild<DeadlineWidget*>();QVERIFY(ddl->isVisible());QCOMPARE(responder.count,0);
        const auto now=QDateTime::currentDateTime();
        QVERIFY(app.ddl.save({{"title","过期待办"},{"hasDue",true},{"due",now.addSecs(-3600).toString(Qt::ISODate)},{"repeat","none"},{"tags","课程"}}));
        QVERIFY(app.ddl.save({{"title","未来待办"},{"hasDue",true},{"due",now.addSecs(7200).toString(Qt::ISODate)},{"repeat","none"},{"tags","生活"}}));
        auto agenda=ddl->findChild<DeadlineAgendaWidget*>("ddlAgenda");auto tabs=ddl->findChild<QTabBar*>("ddlTabs");QVERIFY(agenda&&tabs);QCOMPARE(tabs->count(),3);QCOMPARE(tabs->currentIndex(),0);
        auto cardByTitle=[&](const QString &title)->QAbstractButton*{for(auto card:agenda->findChildren<QAbstractButton*>("ddlAgendaCard"))if(card->property("title").toString()==title)return card;return nullptr;};
        QTimer::singleShot(30,[&]{auto d=modal("ddlEditor");QVERIFY(d);d->findChild<QLineEdit*>("ddlTitle")->setText("新增事务");auto choices=d->findChild<QListWidget*>("ddlAvailableTags");QVERIFY(choices);QCOMPARE(choices->count(),2);QCOMPARE(choices->item(1)->text(),QString("学习"));QTest::mouseClick(choices->viewport(),Qt::LeftButton,Qt::NoModifier,choices->visualItemRect(choices->item(1)).center());d->findChild<QPushButton*>("ddlSave")->click();});
        ddl->findChild<QPushButton*>("ddlAdd")->click();QCOMPARE(app.ddl.templates().size(),3);
        bool picked=false;for(const auto &task:app.ddl.templates())if(task["title"]=="新增事务"){picked=true;QCOMPARE(task["tags"].toString(),QString("学习"));}QVERIFY(picked);
        QTimer::singleShot(30,[&]{auto d=modal("ddlDetail");QVERIFY(d);
            QTimer::singleShot(30,[&]{auto editor=modal("ddlEditor");QVERIFY(editor);auto choices=editor->findChild<DeadlineTagPicker*>("ddlTagChoices");QVERIFY(choices);QCOMPARE(choices->selectedTags(),QString("课程"));auto selected=choices->findChild<QListWidget*>("ddlSelectedTags");QVERIFY(selected->item(0)->text().contains("课程（历史）"));editor->findChild<QPushButton*>("ddlSave")->click();});
            button(d,"编辑")->click();
        });
        QVERIFY(cardByTitle("过期待办"));cardByTitle("过期待办")->click();QTest::qWait(100);
        for(const auto &task:app.ddl.templates())if(task["title"]=="过期待办")QCOMPARE(task["tags"].toString(),QString("课程"));
        auto completionBox=cardByTitle("过期待办")->findChild<QAbstractButton*>("ddlComplete");QVERIFY(completionBox);QVERIFY(completionBox->text().isEmpty());QVERIFY(!completionBox->isChecked());completionBox->click();
        QTRY_VERIFY(!cardByTitle("过期待办"));QCOMPARE(app.ddl.templates().size(),2);QVERIFY(!modal("ddlDetail"));
        auto showDone=ddl->findChild<QCheckBox*>("ddlShowCompleted");QVERIFY(showDone);
        auto futureCard=cardByTitle("未来待办");QVERIFY(futureCard);futureCard->findChild<QAbstractButton*>("ddlComplete")->click();QTRY_VERIFY(!cardByTitle("未来待办"));
        showDone->setChecked(true);QVERIFY(cardByTitle("未来待办"));QVERIFY(cardByTitle("未来待办")->property("done").toBool());
        auto completed=cardByTitle("未来待办")->findChild<QAbstractButton*>("ddlComplete");QVERIFY(completed->isChecked());QVERIFY(!completed->isEnabled());snapshot("ddl-completed");
        QTimer::singleShot(30,[&]{auto d=modal("ddlDetail");QVERIFY(d);QVERIFY(!button(d,"撤销完成"));button(d,"删除")->click();});
        cardByTitle("未来待办")->click();QTRY_VERIFY(!cardByTitle("未来待办"));QCOMPARE(app.ddl.templates().size(),1);
        showDone->setChecked(false);snapshot("ddl");
        tabs->setCurrentIndex(2);QVERIFY(ddl->findChild<QPushButton*>("ddlAdd")->isVisible());QVERIFY(ddl->findChild<QLineEdit*>("ddlSearch")->isVisible());
        auto calendar=ddl->findChild<DeadlineCalendarWidget*>();auto grid=calendar->findChild<QTableWidget*>("ddlCalendarGrid");QVERIFY(calendar&&grid);
        QCOMPARE(grid->columnCount(),7);QCOMPARE(grid->editTriggers(),QAbstractItemView::NoEditTriggers);QCOMPARE(grid->selectionMode(),QAbstractItemView::SingleSelection);
        QVERIFY(grid->selectedItems().isEmpty());
        QSignalSpy transitions(calendar,&DeadlineCalendarWidget::dateActivated);
        auto selectCell=[&](int row,int column,int view){
            const auto point=grid->visualItemRect(grid->item(row,column)).center();
            const auto count=transitions.size();QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,point);
            QCOMPARE(tabs->currentIndex(),view);QCOMPARE(transitions.size(),count);QCOMPARE(grid->selectedItems().size(),1);QVERIFY(grid->item(row,column)->isSelected());
        };
        calendar->setDate(QDate(2026,8,1));QCOMPARE(grid->rowCount(),6);QCOMPARE(grid->item(5,0)->data(DeadlineCalendarWidget::DateRole).toDate(),QDate(2026,8,31));
        calendar->setDate(QDate(2026,10,1));QCOMPARE(grid->rowCount(),5);calendar->setDate(now.date());snapshot("ddl-month");
        tabs->setCurrentIndex(1);QCOMPARE(grid->rowCount(),24);snapshot("ddl-week");
        const auto before=calendar->date();ddl->findChild<QPushButton*>("ddlCalendarNext")->click();QCOMPARE(calendar->date(),before.addDays(7));
        ddl->findChild<QPushButton*>("ddlCalendarToday")->click();QCOMPARE(calendar->date(),QDate::currentDate());
        const QDate calendarDay(2030,7,1);QCOMPARE(calendarDay.dayOfWeek(),1);
        auto calendarTask=[&](const QString &id,int hour){return app.ddl.save({{"id",id},{"title",id},{"hasDue",true},{"repeat","none"},{"due",QDateTime(calendarDay,QTime(hour,0)).toString(Qt::ISODate)}});};
        QVERIFY(calendarTask("零点任务",0));QVERIFY(calendarTask("三点任务",3));QVERIFY(calendarTask("深夜任务",23));
        for(int i=0;i<8;++i)QVERIFY(calendarTask(QString("密集安排 %1").arg(i+1),9));
        QVERIFY(calendarTask("已完成不计数",9));
        for(const auto &o:app.ddl.listOccurrences(now,false))if(o.taskId=="已完成不计数")QVERIFY(app.ddl.complete(o));
        QVERIFY(app.ddl.save({{"id","undated-calendar"},{"title","无时间事项"},{"hasDue",false},{"repeat","none"}}));
        QVERIFY(app.ddl.save({{"id","calendar-series"},{"title","每日阅读"},{"hasDue",true},{"repeat","daily"},{"due",QDateTime(calendarDay,QTime(9,0)).toString(Qt::ISODate)}}));
        calendar->setDate(calendarDay);QCOMPARE(grid->item(0,0)->data(DeadlineCalendarWidget::CountRole).toInt(),1);
        QCOMPARE(grid->item(3,0)->data(DeadlineCalendarWidget::CountRole).toInt(),1);QCOMPARE(grid->item(23,0)->data(DeadlineCalendarWidget::CountRole).toInt(),1);
        QCOMPARE(grid->item(9,0)->data(DeadlineCalendarWidget::CountRole).toInt(),9);
        grid->scrollToItem(grid->item(9,0),QAbstractItemView::PositionAtTop);QCoreApplication::processEvents();snapshot("ddl-week-overflow");
        selectCell(9,0,1);ddl->refresh();QVERIFY(grid->item(9,0)->isSelected());
        selectCell(9,0,1);
        QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(grid->item(9,0)).center());QCOMPARE(tabs->currentIndex(),0);QVERIFY(cardByTitle("每日阅读"));
        tabs->setCurrentIndex(2);
        QCOMPARE(grid->item(0,0)->data(DeadlineCalendarWidget::DateRole).toDate(),calendarDay);QCOMPARE(grid->item(0,0)->data(DeadlineCalendarWidget::CountRole).toInt(),12);
        QCOMPARE(grid->item(0,1)->data(DeadlineCalendarWidget::CountRole).toInt(),1);snapshot("ddl-month-counts");
        // Month -> week -> agenda preserves the selected date, including a later recurring instance.
        selectCell(0,2,2);
        QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(grid->item(0,2)).center());QCOMPARE(tabs->currentIndex(),1);QCOMPARE(calendar->date(),calendarDay.addDays(2));
        grid->scrollToItem(grid->item(9,2),QAbstractItemView::PositionAtTop);QCoreApplication::processEvents();
        selectCell(9,2,1);
        QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(grid->item(9,2)).center());QCOMPARE(tabs->currentIndex(),0);
        int seriesCards=0;QAbstractButton *selectedInstance=nullptr;for(auto card:agenda->findChildren<QAbstractButton*>("ddlAgendaCard"))if(card->property("title")=="每日阅读"){++seriesCards;if(card->property("due").toDateTime().date()==calendarDay.addDays(2))selectedInstance=card;}
        QCOMPARE(seriesCards,2);QVERIFY(selectedInstance);QTest::qWait(30);
        QVERIFY(agenda->verticalScrollBar()->value()>0);snapshot("ddl-agenda-selected-day");
        selectedInstance->findChild<QAbstractButton*>("ddlComplete")->click();QTest::qWait(30);
        auto series=app.ddl.task("calendar-series");QVERIFY(series["instances"].toObject()["2030-07-03"].toObject()["done"].toBool());QVERIFY(!series["instances"].toObject()["2030-07-01"].toObject()["done"].toBool());
        tabs->setCurrentIndex(1);tabs->setCurrentIndex(0);seriesCards=0;for(auto card:agenda->findChildren<QAbstractButton*>("ddlAgendaCard"))if(card->property("title")=="每日阅读")++seriesCards;QCOMPARE(seriesCards,1);
        tabs->setCurrentIndex(2);calendar->setDate(calendarDay);
        auto search=ddl->findChild<QLineEdit*>("ddlSearch");auto tagFilter=ddl->findChild<QComboBox*>("ddlCalendarTag");auto starFilter=ddl->findChild<QComboBox*>("ddlCalendarStar");
        QVERIFY(app.ddl.save({{"id","filter-match"},{"title","日历筛选目标"},{"note","FILTERWORD"},{"tags","学习,生活"},{"priority",1},{"hasDue",true},{"repeat","none"},{"due",QDateTime(calendarDay,QTime(10,0)).toString(Qt::ISODate)}}));
        search->setText("filterword");QCOMPARE(grid->item(0,0)->data(DeadlineCalendarWidget::CountRole).toInt(),1);
        tagFilter->setCurrentIndex(tagFilter->findData("学习"));starFilter->setCurrentIndex(starFilter->findData(1));QCOMPARE(grid->item(0,0)->data(DeadlineCalendarWidget::CountRole).toInt(),1);
        tabs->setCurrentIndex(0);QVERIFY(cardByTitle("日历筛选目标"));QVERIFY(!cardByTitle("零点任务"));QCOMPARE(search->text(),QString("filterword"));QCOMPARE(tagFilter->currentData().toString(),QString("学习"));
        starFilter->setCurrentIndex(starFilter->findData(0));QVERIFY(!cardByTitle("日历筛选目标"));QVERIFY(agenda->findChild<QLabel*>("ddlDayEmpty"));
        ddl->findChild<QPushButton*>("ddlClearFilters")->click();QVERIFY(cardByTitle("零点任务"));QCOMPARE(search->text(),QString());QCOMPARE(starFilter->currentIndex(),0);
        tabs->setCurrentIndex(2);const auto today=QDate::currentDate();
        QVERIFY(app.ddl.save({{"id","past-repeat"},{"title","跨日循环"},{"hasDue",true},{"repeat","daily"},{"due",QDateTime(today.addDays(-5),QTime(0,0)).toString(Qt::ISODate)}}));
        QVERIFY(app.ddl.save({{"id","past-single"},{"title","过去单次"},{"hasDue",true},{"repeat","none"},{"due",QDateTime(today.addDays(-1),QTime(0,0)).toString(Qt::ISODate)}}));
        calendar->setDate(today);bool checkedPast=false,checkedToday=false;
        for(int r=0;r<grid->rowCount();++r)for(int c=0;c<7;++c){auto cell=grid->item(r,c);const auto date=cell->data(DeadlineCalendarWidget::DateRole).toDate();const auto titles=cell->data(DeadlineCalendarWidget::TitlesRole).toStringList();
            if(date==today.addDays(-1)){checkedPast=true;QVERIFY(!titles.contains("跨日循环"));QVERIFY(titles.contains("过去单次"));}
            if(date==today){checkedToday=true;QVERIFY(titles.contains("跨日循环"));}}
        QVERIFY(checkedPast&&checkedToday);ddl->findChild<QPushButton*>("ddlCalendarToday")->click();tabs->setCurrentIndex(0);
        auto boundary=ddl->findChild<QLabel*>("ddlNowBoundary");QVERIFY(boundary);QVERIFY(cardByTitle("无时间事项"));QVERIFY(cardByTitle("零点任务"));
        // Navigating an empty day keeps an explicit anchor in the continuous agenda.
        tabs->setCurrentIndex(2);calendar->setDate(QDate(2020,1,1));QCoreApplication::processEvents();
        selectCell(0,2,2);
        QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(grid->item(0,2)).center());QCOMPARE(tabs->currentIndex(),1);
        grid->scrollToItem(grid->item(12,2));QCoreApplication::processEvents();selectCell(12,2,1);QTest::mouseDClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(grid->item(12,2)).center());QCOMPARE(tabs->currentIndex(),0);
        QVERIFY(agenda->findChild<QLabel*>("ddlDayEmpty"));ddl->findChild<QPushButton*>("ddlCalendarToday")->click();
        navigate(Qt::Key_N);auto note=window.findChild<NoteWidget*>();QVERIFY(note->isVisible());auto noteTabs=note->findChild<QTabBar*>("noteTabs");QCOMPARE(noteTabs->count(),3);QCOMPARE(noteTabs->currentIndex(),0);QCOMPARE(responder.count,0);
        auto ordinary=note->findChildren<NoteCard*>();NoteCard *paper=nullptr;for(auto c:ordinary)if(!c->isHidden())paper=c;QVERIFY(paper);
        QTest::mouseDClick(paper,Qt::LeftButton);QVERIFY(paper->expanded());
        auto fields=paper->findChild<NoteFields*>();QVERIFY(fields);fields->title->setText("修改后的纸条");QVERIFY(paper->save());QCOMPARE(paper->entry()["title"].toString(),QString("修改后的纸条"));fields->title->setText("");navigate(Qt::Key_S);QVERIFY(note->isVisible());fields->title->setText("修改后的纸条");QVERIFY(paper->save());snapshot("note");
        noteTabs->setCurrentIndex(2);QVERIFY(!paper->expanded());QCOMPARE(responder.count,0);
        auto secretCard=[&]()->NoteCard*{for(auto c:note->findChildren<NoteCard*>())if(!c->isHidden()&&c->entry()["kind"].toInt()==0)return c;return nullptr;};
        QVERIFY(secretCard());
        QTimer closer;closer.setInterval(25);connect(&closer,&QTimer::timeout,this,[&]{if(auto d=modal("noteSecretEditor")){d->findChild<QTextEdit*>()->setPlainText("任意秘密正文，不限账密");d->reject();}});closer.start();
        QTest::mouseDClick(secretCard(),Qt::LeftButton);closer.stop();QCOMPARE(responder.count,1);QVERIFY(!app.notes.unlocked());
        QVERIFY(app.notes.unlock("test-master-password"));QCOMPARE(app.notes.entries().first().toObject()["body"].toString(),QString("任意秘密正文，不限账密"));app.notes.lock();
        QTimer secondClose;secondClose.setInterval(25);connect(&secondClose,&QTimer::timeout,this,[&]{if(auto d=modal("noteSecretEditor"))d->reject();});secondClose.start();QTest::mouseDClick(secretCard(),Qt::LeftButton);secondClose.stop();QCOMPARE(responder.count,2);QVERIFY(!app.notes.unlocked());
        navigate(Qt::Key_M);QVERIFY(window.findChild<MailWidget*>()->isVisible());QCOMPARE(responder.count,2);
        MailAccount sample;sample.name="Sample";sample.address="sample@example.edu";sample.server="imap.example.test";sample.port="993";sample.code="fixture-only";MailAccountManager::getInstance()->setMailAccount(sample);
        auto repository=MailManager::getInstance();QJsonObject batch{{"validity","9"},{"cursor",1},{"messages",QJsonArray{QJsonObject{{"id","9:1"},{"from","teacher@example.edu"},{"subject","测试通知 / Sample mail"},{"date",QDateTime::currentSecsSinceEpoch()}}}}};
        QVERIFY(repository->saveBatch("sample@example.edu",batch));repository->updateMailBody("sample@example.edu","9:1","<p>Cached body</p>");
        auto mail=window.findChild<MailWidget*>();auto bodyPage=mail->findChild<QWidget*>("mailBodyPage");QVERIFY(!bodyPage->isVisible());
        auto mailRows=mail->findChild<QTreeWidget*>("mailRows");auto mailTabs=mail->findChild<QTabBar*>("mailTabs");
        QVERIFY(mailRows&&mailTabs);QCOMPARE(mailTabs->count(),4);QCOMPARE(mailRows->columnCount(),3);QCOMPARE(mailRows->topLevelItemCount(),1);QVERIFY(mailRows->topLevelItem(0)->toolTip(0).contains("teacher@example.edu"));snapshot("mail");
        QTest::mouseClick(mailRows->viewport(),Qt::LeftButton,Qt::NoModifier,mailRows->visualItemRect(mailRows->topLevelItem(0)).center());QVERIFY(!bodyPage->isVisible());QCOMPARE(repository->query({},{},1,100).size(),1);QCOMPARE(mailRows->selectedItems().size(),1);
        auto check=mailRows->itemWidget(mailRows->topLevelItem(0),2)->findChild<QPushButton*>("mailCheck");QVERIFY(check);check->click();QVERIFY(!bodyPage->isVisible());QCOMPARE(repository->query({},{},1,100).size(),0);
        QTest::mouseClick(mailRows->viewport(),Qt::LeftButton,Qt::NoModifier,mailRows->visualItemRect(mailRows->topLevelItem(0)).center());
        QTest::mouseDClick(mailRows->viewport(),Qt::LeftButton,Qt::NoModifier,mailRows->visualItemRect(mailRows->topLevelItem(0)).center());
        QVERIFY(bodyPage->isVisible());QCOMPARE(bodyPage->findChildren<QComboBox*>().size(),1);QVERIFY(!button(bodyPage,"查看附件"));QVERIFY(!mailRows->isVisible());QVERIFY(button(mail,"转为事务")->isVisible());QCOMPARE(repository->query({},{},1,100).size(),0);snapshot("mail-reading");
        auto mode=bodyPage->findChild<QComboBox*>("mailDisplayMode");mode->setCurrentIndex(1);mode->setCurrentIndex(2);mode->setCurrentIndex(3);QCOMPARE(repository->getMailBody("sample@example.edu","9:1"),QString("<p>Cached body</p>"));
        const auto originalSize=window.size();window.resize(760,700);QCoreApplication::processEvents();snapshot("mail-reading-narrow");for(const auto &name:{"mailBack","mailToTask","mailToNote","mailStar","mailReadingDelete"}){auto action=mail->findChild<QPushButton*>(name);QVERIFY(action);QVERIFY(action->width()>=action->sizeHint().width());}window.resize(originalSize);QCoreApplication::processEvents();
        button(mail,"收藏")->click();QCOMPARE(repository->query({},{},2,100).size(),1);button(mail,"← 返回")->click();QVERIFY(mailRows->isVisible());QVERIFY(!bodyPage->isVisible());
        mailTabs->setCurrentIndex(2);QCOMPARE(mailRows->topLevelItemCount(),1);
        auto remove=mailRows->itemWidget(mailRows->topLevelItem(0),2)->findChild<QPushButton*>("mailDelete");QVERIFY(remove);remove->click();QVERIFY(!bodyPage->isVisible());QCOMPARE(mailRows->topLevelItemCount(),0);QCOMPARE(repository->getMailBody("sample@example.edu","9:1"),QString());QVERIFY(repository->saveBatch("sample@example.edu",batch));QCOMPARE(repository->query({},{},0,100).size(),0);
        mailTabs->setCurrentIndex(3);QCOMPARE(mailRows->topLevelItemCount(),1);
        snapshot("mail-trash");
        QVERIFY(mailRows->isColumnHidden(1));QVERIFY(!mailRows->isColumnHidden(2));QVERIFY(!mailRows->topLevelItem(0)->toolTip(0).contains("teacher@example.edu"));
        QTest::mouseClick(mailRows->viewport(),Qt::LeftButton,Qt::NoModifier,mailRows->visualItemRect(mailRows->topLevelItem(0)).center());QVERIFY(!bodyPage->isVisible());
        auto restore=mailRows->itemWidget(mailRows->topLevelItem(0),2)->findChild<QPushButton*>("mailRestore");QVERIFY(restore);QCoreApplication::processEvents();QVERIFY(restore->width()>=restore->sizeHint().width());restore->click();
        QTRY_COMPARE_WITH_TIMEOUT(mailRows->topLevelItemCount(),0,10000);QVERIFY(!bodyPage->isVisible());QCOMPARE(repository->query({},{},0,100).size(),1);
        // A failed fetch leaves metadata and actions in place; retry uses the current account configuration.
        sample.server="offline.invalid";MailAccountManager::getInstance()->setMailAccount(sample);mailTabs->setCurrentIndex(0);mailRows->setCurrentItem(mailRows->topLevelItem(0));mailRows->setFocus();QTest::keyClick(mailRows,Qt::Key_Return);
        QVERIFY(bodyPage->isVisible());QVERIFY(bodyPage->findChild<QLabel*>("mailSubject")->isVisible());QVERIFY(!button(mail,"转为事务")->isEnabled());
        auto retry=bodyPage->findChild<QPushButton*>("mailBodyRetry");QTRY_VERIFY_WITH_TIMEOUT(retry->isVisible(),10000);QVERIFY(!bodyPage->findChild<QLabel*>("mailBodyStatus")->text().isEmpty());snapshot("mail-reading-error");
        sample.server="imap.example.test";MailAccountManager::getInstance()->setMailAccount(sample);retry->click();QVERIFY(!retry->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(button(mail,"转为事务")->isEnabled(),10000);QVERIFY(bodyPage->findChild<QTextBrowser*>("mailContent")->toPlainText().contains("Integration body"));QVERIFY(button(mail,"保存纸条")->isEnabled());
        button(mail,"← 返回")->click();QVERIFY(mailRows->isVisible());window.resize(760,700);QCoreApplication::processEvents();snapshot("mail-narrow");window.resize(originalSize);
        auto history=batch;history["cursor"]=0;history["messages"]=QJsonArray{};QVERIFY(repository->saveBatch("sample@example.edu",history,false));QCOMPARE(repository->syncState("sample@example.edu")["cursor"].toInt(),1);
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty()){
            QJsonArray messages;const QStringList subjects{"关于下周课程调整及实验室开放时间的通知","课程作业：请在周五前提交实验报告","图书馆预约已确认","本周学习小组讨论材料","校园网账号安全提醒","学术讲座报名成功"};
            for(int i=0;i<subjects.size();++i)messages.append(QJsonObject{{"id",QString("9:%1").arg(200+i)},{"from",i%2?"课程助教 <assistant@example.edu>":"教务处 <office@example.edu>"},{"subject",subjects[i]},{"date",QDateTime::currentSecsSinceEpoch()-i*3600},{"read",i>2}});
            QVERIFY(repository->saveBatch(sample.address,{{"messages",messages}},false));
            const QString html="<html><body bgcolor='#ffffff' style='color:#20242a'><h2>课程与实验室安排</h2><p>同学你好：</p><p>下周课程安排如下，请提前查看，并预留实验准备时间。</p><table border='1' cellspacing='0' cellpadding='10' width='100%'><tr bgcolor='#e8e6e1'><th>课程</th><th>时间</th><th>地点</th></tr><tr><td>高等数学</td><td>周三 10:00</td><td>教学楼 A302</td></tr><tr><td>程序设计实验</td><td>周五 14:00</td><td>实验楼 B201</td></tr></table><p>请携带学生证。若有时间冲突，请联系课程助教。</p><p><a href='https://example.edu'>查看课程说明</a></p><p>教务处</p></body></html>";
            repository->updateMailBody(sample.address,"9:200",html);mailRows->clearSelection();mailRows->setCurrentItem(nullptr);QCoreApplication::processEvents();snapshot("mail-populated");
            QTest::mouseClick(mailRows->viewport(),Qt::LeftButton,Qt::NoModifier,mailRows->visualItemRect(mailRows->topLevelItem(0)).center());QTest::mouseDClick(mailRows->viewport(),Qt::LeftButton,Qt::NoModifier,mailRows->visualItemRect(mailRows->topLevelItem(0)).center());QCoreApplication::processEvents();snapshot("mail-reading-html");
            mode->setCurrentIndex(2);QCoreApplication::processEvents();snapshot("mail-reading-html-dark");window.resize(760,700);QCoreApplication::processEvents();snapshot("mail-reading-html-narrow");button(mail,"← 返回")->click();QCoreApplication::processEvents();snapshot("mail-populated-narrow");window.resize(originalSize);
        }
        navigate(Qt::Key_S);QCOMPARE(responder.count,2);
        QCOMPARE(QApplication::focusWidget(),settings);
        for(auto list:settings->findChildren<QListWidget*>()){QCOMPARE(list->currentRow(),-1);QVERIFY(list->selectedItems().isEmpty());}
        settingsScroll->verticalScrollBar()->setValue(0);QCoreApplication::processEvents();snapshot("settings");
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty()){
            const auto output=qEnvironmentVariable("ZTL_QA_DIR");
            const auto originalTags=ConfigManager::getInstance()->commonTags();
            auto previewTags=originalTags;previewTags.append({"资料","项目","计划"});ConfigManager::getInstance()->setCommonTags(previewTags);
            for(const auto &name:{"Home","Mail","DDL","Note","general"}){
                auto group=settings->findChild<QWidget*>("settingsGroup"+QString(name));settingsScroll->ensureWidgetVisible(group);QCoreApplication::processEvents();
                group->grab().save(output+"/settings-group-"+name+".png");
            }
            settingsScroll->ensureWidgetVisible(commonTags);QCoreApplication::processEvents();commonTags->scrollToBottom();QCoreApplication::processEvents();
            commonTags->grab().save(output+"/settings-list-scrolled.png");ConfigManager::getInstance()->setCommonTags(originalTags);
            auto separator=settings->findChild<QWidget*>("settingsBoundaryHomeMail");settingsScroll->ensureWidgetVisible(separator);QCoreApplication::processEvents();snapshot("settings-boundary");
            auto cityUp=settings->findChild<QPushButton*>("city上移");settingsScroll->ensureWidgetVisible(cityUp);QCoreApplication::processEvents();
            QTest::mouseMove(settings,QPoint(0,0));QTest::qWait(30);cityUp->grab().save(output+"/settings-button-rest.png");
            QTest::mouseMove(cityUp,cityUp->rect().center());QTest::qWait(30);cityUp->grab().save(output+"/settings-button-hover.png");
            QTest::mousePress(cityUp,Qt::LeftButton);cityUp->grab().save(output+"/settings-button-pressed.png");QTest::mouseRelease(cityUp,Qt::LeftButton);
            settingsScroll->ensureWidgetVisible(weatherKey);weatherKey->setFocus();QCoreApplication::processEvents();weatherKey->grab().save(output+"/settings-input-focus.png");
        }
        MailBrief homeMail;homeMail.ownerEmail=sample.address;homeMail.uid="9:5500";homeMail.subject="首页计数测试";homeMail.sender="home@example.edu";homeMail.timestamp=QDateTime::currentSecsSinceEpoch();repository->addMail(homeMail);
        navigate(Qt::Key_H);auto home=window.findChild<HomeWidget*>();auto homeSummary=home->findChild<QLabel*>("homeSummary");QVERIFY(homeSummary);
        QVERIFY(homeSummary->text().contains(QString::number(repository->unreadCount())+" 封邮件"));
        homeSummary->linkActivated("mail");QVERIFY(mail->isVisible());QCOMPARE(mailTabs->currentIndex(),1);QCOMPARE(mail->findChild<QLineEdit*>("mailSearch")->text(),QString());
        navigate(Qt::Key_H);homeSummary->linkActivated("today");QVERIFY(ddl->isVisible());QCOMPARE(tabs->currentIndex(),0);QVERIFY(ddl->findChild<QLineEdit*>("ddlSearch")->text().isEmpty());
        if(QGuiApplication::platformName()=="windows"){navigate(Qt::Key_H);QTest::qWait(200);snapshot("home");app.home.weatherUpdated("北京海淀","--","Key 平台不匹配，请使用 Web 服务 Key");QTest::qWait(100);snapshot("home-weather-error");navigate(Qt::Key_M);}
        qApp->removeEventFilter(&responder);app.shutdown();
    }
    void homeLayoutAndPlayback(){
        const auto oldAnimation=qApp->property("ztlAnimation");qApp->setProperty("ztlAnimation",true);
        HomeDepartment department;HomeWidget home(&department);home.resize(1180,760);home.show();QTest::qWait(100);
        auto info=home.findChild<HomeInfoWidget*>();auto flow=home.findChild<HomeBackgroundShader*>();auto timer=flow->findChild<QTimer*>("homeAnimationTimer");auto clockTimer=info->findChild<QTimer*>("homeClockTimer");
        auto summary=home.findChild<QLabel*>("homeSummary");auto weather=home.findChild<QLabel*>("homeWeather");const auto today=QDate::currentDate();
        auto text=[&]{QTextDocument doc;doc.setHtml(summary->text());return doc.toPlainText();};
        department.weatherUpdated("北京海淀","18","晴");QCOMPARE(weather->text(),QString("晴"));QCOMPARE(home.findChild<QLabel*>("homeTemperature")->text(),QString("18°"));
        home.setSummary(today,3,2,0,8);QVERIFY(text().contains("今天还有 3 个 DDL"));QVERIFY(text().contains("8 封邮件未处理"));
        QSignalSpy mail(&home,&HomeWidget::mailRequested),ddl(&home,&HomeWidget::ddlRequested);summary->linkActivated("mail");QCOMPARE(mail.size(),1);summary->linkActivated("today");QCOMPARE(ddl.last().first().toDate(),today);
        auto snapshot=[&](const QString &name){QCoreApplication::processEvents();if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())home.grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/"+name+".png");};
        QCoreApplication::processEvents();snapshot("home-design");
        const QPoint mailPoint(summary->width()-10,summary->height()/2);QTest::mouseMove(summary,mailPoint);QTest::qWait(30);snapshot("home-summary-hover");QTest::mouseClick(summary,Qt::LeftButton,Qt::NoModifier,mailPoint);QCOMPARE(mail.size(),2);
        home.setFocus();QTest::mouseMove(&home,QPoint(1,1));QTest::keyClick(&home,Qt::Key_Tab);QVERIFY(summary->hasFocus());QTest::keyClick(summary,Qt::Key_Return);QCOMPARE(ddl.last().first().toDate(),today);home.setFocus();
        home.setSummary(today,0,2,0,8);QVERIFY(text().contains("今天的 DDL 已清空"));
        home.setSummary(today,0,2,0,0);QVERIFY(text().contains("明天尚有 2 个 DDL"));summary->linkActivated("tomorrow");QCOMPARE(ddl.last().first().toDate(),today.addDays(1));snapshot("home-tomorrow");
        home.setSummary(today,0,0,2,0);QVERIFY(text().contains("2 个逾期 DDL"));snapshot("home-overdue");
        home.setSummary(today,0,0,0,0);QCOMPARE(text(),QString("最近的事都干完了"));snapshot("home-clear");
        home.setSummary(today,0,0,0,-1);QVERIFY(!text().contains("干完"));
        home.setSummary(today,3,2,0,8);home.resize(760,700);QTest::qWait(30);snapshot("home-design-narrow");QVERIFY(home.width()<=760);
        auto weatherGroup=home.findChild<QWidget*>("homeWeatherGroup");QVERIFY(weatherGroup->mapTo(&home,QPoint()).y()>home.height()*.75);
        QVERIFY(summary->mapTo(&home,QPoint()).x()+summary->width()<=home.width()-25);QVERIFY(info->mapTo(&home,info->rect().center()).y()<home.height()*.5);
        const QDateTime fixed(QDate(2032,1,2),QTime(9,41));info->refreshDateTime(fixed);QCOMPARE(home.findChild<QLabel*>("homeDate")->text(),QString("2032.01.02  Friday"));QCOMPARE(home.findChild<QLabel*>("homeClock")->text(),QString("09:41"));
        QSignalSpy days(info,&HomeInfoWidget::dayChanged);info->refreshDateTime(fixed.addDays(1));QCOMPARE(days.size(),1);info->refreshDateTime(fixed.addDays(1));QCOMPARE(days.size(),1);info->refreshDateTime();
        department.weatherUpdated("北京海淀","--","Key 平台不匹配，请使用 Web 服务 Key");QCOMPARE(weather->text(),QString("天气暂不可用"));QVERIFY(weather->toolTip().contains("Key"));QVERIFY(!home.findChild<QLabel*>("homeTemperature")->isVisible());snapshot("home-design-weather-error");
        qApp->setProperty("ztlAnimation",false);QVERIFY(!timer->isActive());QVERIFY(clockTimer->isActive());
        if(QGuiApplication::platformName()=="windows"){
            QVERIFY(flow->isValid());QVERIFY(flow->property("shaderReady").toBool());QCoreApplication::processEvents();auto still=flow->grabFramebuffer();QTest::qWait(100);QCOMPARE(flow->grabFramebuffer(),still);
            qApp->setProperty("ztlAnimation",true);QVERIFY(timer->isActive());QSignalSpy frames(flow,&HomeBackgroundShader::flowTimeChanged);QTest::qWait(200);QVERIFY(frames.size()>=3);QVERIFY(flow->grabFramebuffer()!=still);
            home.showMinimized();QTest::qWait(70);QVERIFY(!timer->isActive());home.showNormal();QTest::qWait(70);QVERIFY(timer->isActive());
        }
        home.hide();QVERIFY(!timer->isActive());QVERIFY(!clockTimer->isActive());home.show();QTest::qWait(30);QVERIFY(clockTimer->isActive());
        qApp->setProperty("ztlAnimation",oldAnimation);
    }
    void deadlineLayoutPreview(){
        QTemporaryDir dir;TaskRepository repo(dir.filePath("preview.db"));DDLDepartment department(&repo);
        const auto today=QDate::currentDate();
        auto add=[&](const QString &title,const QDate &date,const QTime &time,const QString &tag,const QString &note="",int star=0,const QString &repeat="none"){
            return department.save({{"title",title},{"hasDue",true},{"due",QDateTime(date,time).toString(Qt::ISODate)},{"repeat",repeat},{"tags",tag},{"note",note},{"priority",star}});
        };
        QVERIFY(add("整理实验数据",today.addDays(-1),QTime(18,0),"学习","核对测量结果，补全记录。"));
        QVERIFY(add("提交课程报告",today,QTime(10,25),"学习","检查引用与附件后提交。",1));
        QVERIFY(add("整理本周笔记",today,QTime(16,30),"学习","把课堂记录整理到知识库。"));
        QVERIFY(add("复习英语词汇",today.addDays(1),QTime(8,0),"学习","复习易错词与例句。",0,"daily"));
        QVERIFY(add("图书馆还书",today.addDays(1),QTime(10,15),"生活"));
        QVERIFY(add("提交小组作业",today.addDays(1),QTime(13,0),"学习","合并成员内容，检查最终版本。",1));
        for(int i=0;i<7;++i)QVERIFY(add(QString("项目资料整理 %1").arg(i+1),today.addDays(2),QTime(9,i*5),"项目"));
        DeadlineWidget preview(&department);preview.resize(1180,760);preview.show();QTest::qWait(50);
        for(auto card:preview.findChildren<QAbstractButton*>("ddlAgendaCard")){
            QVERIFY2(card->height()>=card->findChild<QAbstractButton*>("ddlComplete")->height()+20,qPrintable(QString("card %1, minimum hint %2").arg(card->height()).arg(card->minimumSizeHint().height())));
            for(auto label:card->findChildren<QLabel*>())QVERIFY2(label->height()>=label->fontMetrics().height(),qPrintable(QString("%1: label %2, font %3, card %4").arg(label->text()).arg(label->height()).arg(label->fontMetrics().height()).arg(card->height())));
        }
        auto tabs=preview.findChild<QTabBar*>("ddlTabs");auto calendar=preview.findChild<DeadlineCalendarWidget*>();auto grid=calendar->findChild<QTableWidget*>("ddlCalendarGrid");
        auto snapshot=[&](const QString &name){if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())preview.grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/"+name+".png");};
        snapshot("ddl-design-agenda");
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty()){
            auto complete=preview.findChild<QAbstractButton*>("ddlComplete");const auto output=qEnvironmentVariable("ZTL_QA_DIR");
            QTest::mouseMove(&preview,QPoint(1,1));QTest::qWait(20);complete->grab().save(output+"/ddl-complete-rest.png");
            QTest::mouseMove(complete,complete->rect().center());QTest::qWait(20);complete->grab().save(output+"/ddl-complete-hover.png");
            QTest::mousePress(complete,Qt::LeftButton);complete->grab().save(output+"/ddl-complete-pressed.png");QTest::mouseRelease(complete,Qt::LeftButton,Qt::NoModifier,QPoint(-3,-3));
        }
        tabs->setCurrentIndex(2);QTest::qWait(30);snapshot("ddl-design-month");
        tabs->setCurrentIndex(1);QTest::qWait(30);grid->scrollToItem(grid->item(8,0),QAbstractItemView::PositionAtTop);QCoreApplication::processEvents();snapshot("ddl-design-week");
        preview.resize(760,700);QTest::qWait(30);snapshot("ddl-design-week-narrow");tabs->setCurrentIndex(0);QTest::qWait(30);snapshot("ddl-design-agenda-narrow");
        QVERIFY(preview.width()<=760);QVERIFY(preview.findChild<QPushButton*>("ddlAdd")->isVisible());
        preview.resize(1180,760);tabs->setCurrentIndex(2);QTest::qWait(30);
        grid->setCurrentCell(0,0);grid->setFocus();QTest::keyClick(grid,Qt::Key_Right);QCOMPARE(grid->currentColumn(),1);QTest::keyClick(grid,Qt::Key_Return);QCOMPARE(tabs->currentIndex(),1);
        grid->setCurrentCell(12,0);grid->setFocus();QTest::keyClick(grid,Qt::Key_Return);QCOMPARE(tabs->currentIndex(),0);
        auto agenda=preview.findChild<DeadlineAgendaWidget*>();auto checks=agenda->findChildren<QAbstractButton*>("ddlComplete");QVERIFY(!checks.isEmpty());
        auto check=checks.first();check->setFocus();const auto cards=agenda->findChildren<QAbstractButton*>("ddlAgendaCard").size();QTest::keyClick(check,Qt::Key_Space);
        QTRY_VERIFY(agenda->findChildren<QAbstractButton*>("ddlAgendaCard").size()<cards);
    }
    void deadlineEditorRules(){
        const QDateTime time(QDate(2032,1,31),QTime(10,23));
        QCOMPARE(DeadlineDateTimeInput::roundUp(time),QDateTime(time.date(),QTime(10,25)));
        QCOMPARE(DeadlineDateTimeInput::roundUp(QDateTime(time.date(),QTime(23,59))),QDateTime(QDate(2032,2,1),QTime(0,0)));
        DeadlineEditor editor({{"tags","旧一,旧二,旧三,旧四"},{"repeat","weekly"},{"weekdays",QJsonArray{1,3,5}}},time,true,false);
        auto title=editor.findChild<QLineEdit*>("ddlTitle");auto save=editor.findChild<QPushButton*>("ddlSave");
        auto feedback=editor.findChild<QLabel*>("ddlEditorFeedback");QSignalSpy submitted(&editor,&DeadlineEditor::submitted);
        editor.show();QTest::qWait(30);save->click();QCOMPARE(submitted.size(),0);QVERIFY(feedback->text().contains("标题"));
        auto minute=editor.findChild<QSpinBox*>("ddlMinute");QCOMPARE(minute->value(),25);
        QTest::mouseClick(minute,Qt::LeftButton,Qt::NoModifier,QPoint(minute->width()-8,8));QCOMPARE(minute->value(),30);
        QTest::mouseClick(minute,Qt::LeftButton,Qt::NoModifier,QPoint(minute->width()-8,minute->height()-8));QCOMPARE(minute->value(),25);
        title->setText("保留草稿");save->click();QCOMPARE(submitted.size(),0);QVERIFY(feedback->text().contains("三个"));QVERIFY(editor.isVisible());
        auto selected=editor.findChild<QListWidget*>("ddlSelectedTags");auto available=editor.findChild<QListWidget*>("ddlAvailableTags");
        auto click=[&](QListWidget *list,int row){list->scrollToItem(list->item(row));QCoreApplication::processEvents();QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(row)).center());};
        const int originalChoices=available->count();
        click(selected,0);QCOMPARE(selected->count(),3);QCOMPARE(available->count(),originalChoices+1);QVERIFY(!(available->item(0)->flags()&Qt::ItemIsEnabled));
        click(selected,0);QCOMPARE(selected->count(),2);QVERIFY(available->item(0)->flags()&Qt::ItemIsEnabled);
        int oldTag=-1;for(int i=0;i<available->count();++i)if(available->item(i)->data(Qt::UserRole)=="旧一")oldTag=i;QVERIFY(oldTag>=0);
        click(available,oldTag);QCOMPARE(selected->count(),3);QCOMPARE(available->count(),originalChoices+1);
        auto year=editor.findChild<QSpinBox*>("ddlYear");auto month=editor.findChild<QSpinBox*>("ddlMonth");auto day=editor.findChild<QSpinBox*>("ddlDay");
        month->setValue(2);QCOMPARE(day->maximum(),29);year->setValue(2031);QCOMPARE(day->maximum(),28);
        QCOMPARE(editor.values()["weekdays"].toArray(),QJsonArray({1,3,5}));
        auto repeat=editor.findChild<QComboBox*>("ddlRepeat");repeat->setCurrentIndex(repeat->findData("monthly"));QCOMPARE(editor.values()["repeat"].toString(),QString("monthly"));
        repeat->setCurrentIndex(repeat->findData("weekly"));QCOMPARE(editor.values()["weekdays"].toArray(),QJsonArray{QDate(2031,2,28).dayOfWeek()});
        save->click();QCOMPARE(submitted.size(),1);QCOMPARE(editor.values()["hasDue"].toBool(),true);QCOMPARE(title->text(),QString("保留草稿"));
        if(!qEnvironmentVariable("ZTL_QA_DIR").isEmpty())editor.grab().save(qEnvironmentVariable("ZTL_QA_DIR")+"/ddl-editor.png");
        QTest::mouseClick(editor.windowHandle(),Qt::LeftButton,Qt::NoModifier,QPoint(-8,-8));QVERIFY(editor.isVisible());
        QTimer::singleShot(10,[]{if(auto box=qobject_cast<QMessageBox*>(activeTestDialog()))box->button(QMessageBox::No)->click();});
        editor.reject();QVERIFY(editor.isVisible());QCOMPARE(title->text(),QString("保留草稿"));
        QTimer::singleShot(10,[]{if(auto box=qobject_cast<QMessageBox*>(activeTestDialog()))box->button(QMessageBox::Yes)->click();});
        editor.reject();QVERIFY(!editor.isVisible());
        // Outside clicks keep an unchanged dialog open; explicit Esc closes it.
        DeadlineEditor popup({},time,false,false);QSignalSpy ignored(&popup,&DeadlineEditor::submitted);
        QTimer::singleShot(30,&popup,[&]{QTest::mouseClick(popup.windowHandle(),Qt::LeftButton,Qt::NoModifier,QPoint(-8,-8));QVERIFY(popup.isVisible());QTest::keyClick(&popup,Qt::Key_Escape);});
        QTimer::singleShot(1000,&popup,&QDialog::reject);QElapsedTimer elapsed;elapsed.start();popup.exec();QVERIFY(elapsed.elapsed()<900);QCOMPARE(ignored.size(),0);
    }
};
QTEST_MAIN(UiTests)
#include "test_ui.moc"
