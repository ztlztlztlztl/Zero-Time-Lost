#include "mainwindow.h"
#include "appcontext.h"
#include "homewidget.h"
#include "settingwidget.h"
#include "mailwidget.h"
#include "mailmanager.h"
#include "cachemanager.h"
#include "configmanager.h"
#include "deadlinewidget.h"
#include "notewidget.h"
#include "setupwizard.h"
#include "cloudconnection.h"
#include "paths.h"
#include "localdataservice.h"
#include "navigationrail.h"
#include "uimotion.h"
#include "uistyle.h"
#include <QtWidgets>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wtsapi32.h>
#endif
MainWindow::MainWindow(AppContext *context,QWidget *parent):QMainWindow(parent),app(context){
    setWindowTitle("Z.T.L");setMinimumSize(760,560);resize(1180,760);
    setStyleSheet(UiStyle::sheet);
    UiMotion::install();
    localData = new LocalDataService(this);
    auto shell = new QWidget; shell->setObjectName("appShell");
    auto shellLayout = new QHBoxLayout(shell); shellLayout->setContentsMargins(0,0,0,0); shellLayout->setSpacing(0);
    navigation = new NavigationRail(shell);
    stack = new QStackedWidget; stack->setObjectName("appPages");
    shellLayout->addWidget(navigation); shellLayout->addWidget(stack,1); setCentralWidget(shell);
    connect(navigation,&NavigationRail::pageRequested,this,&MainWindow::navigate);
    connect(navigation,&NavigationRail::lockRequested,this,&MainWindow::lock);
    connect(stack,&QStackedWidget::currentChanged,navigation,&NavigationRail::setCurrent);
    home=new HomeWidget(&app->home);settings=new SettingWidget;mail=new MailWidget(&app->mail);ddl=new DeadlineWidget(&app->ddl);notes=new NoteWidget(&app->notes);
    stack->addWidget(home);stack->addWidget(mail);stack->addWidget(ddl);stack->addWidget(notes);stack->addWidget(settings);
    connect(home,&HomeWidget::ddlRequested,this,[this](const QDate &date){navigate(2);ddl->showDay(date);});
    connect(home,&HomeWidget::calendarItemRequested,this,[this](const QString &identity,const QDate &date){navigate(2);ddl->showItem(identity,date);});
    connect(home,&HomeWidget::mailRequested,this,[this]{navigate(1);mail->showUnread();});
    connect(home,&HomeWidget::summaryRefreshRequested,this,&MainWindow::refreshHomeSummary);
    connect(&app->ddl,&DDLDepartment::changed,this,&MainWindow::refreshHomeSummary);
    connect(&app->ddl,&DDLDepartment::writeFailed,this,[this](const QString &e){statusBar()->showMessage(e);});
    connect(&app->ddl,&DDLDepartment::temporalChanged,this,&MainWindow::refreshHomeSummary);
    connect(MailManager::getInstance(),&MailManager::changed,this,&MainWindow::refreshHomeSummary);
    connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state){
        if(state==Qt::ApplicationActive){app->ddl.resyncTime();refreshHomeSummary();}
        else if(app->notes.unlocked())lock();
    });
    app->clock.every("home-summary",60,[this]{refreshHomeSummary();});refreshHomeSummary();
    const QStringList names{"Home","Mail","Calendar","Note","设置"};
    const QStringList shortcuts{"Alt+H","Alt+M","Alt+C","Alt+N","Alt+S"};
    for(int i=0;i<names.size();++i){
        auto action=new QAction(names[i],this);action->setShortcut(QKeySequence(shortcuts[i]));action->setShortcutContext(Qt::ApplicationShortcut);addAction(action);
        connect(action,&QAction::triggered,this,[this,i]{navigate(i);});
    }
    auto lockAction=new QAction(this);lockAction->setShortcut(QKeySequence("Alt+L"));lockAction->setShortcutContext(Qt::ApplicationShortcut);addAction(lockAction);connect(lockAction,&QAction::triggered,this,&MainWindow::lock);
    auto full=new QAction(this);full->setShortcut(QKeySequence("F11"));addAction(full);connect(full,&QAction::triggered,this,[this]{isFullScreen()?showNormal():showFullScreen();});
    auto help=new QAction(this);help->setShortcut(QKeySequence::HelpContents);addAction(help);connect(help,&QAction::triggered,this,&MainWindow::showHelp);
    tray=new QSystemTrayIcon(style()->standardIcon(QStyle::SP_ComputerIcon),this);tray->setToolTip("Z.T.L");
    auto trayMenu=new QMenu(this);auto open=trayMenu->addAction("打开");connect(open,&QAction::triggered,this,[this]{openFromTray();});
    auto sync=trayMenu->addAction("立即收信");connect(sync,&QAction::triggered,&app->mail,[this]{app->mail.mailPolling(true);});
    auto privacy=trayMenu->addAction("立即锁定秘密");connect(privacy,&QAction::triggered,this,&MainWindow::lock);
    auto exit=trayMenu->addAction("退出");connect(exit,&QAction::triggered,this,[this]{quitting=true;close();});tray->setContextMenu(trayMenu);
    connect(tray,&QSystemTrayIcon::activated,this,[this](auto reason){if(reason==QSystemTrayIcon::DoubleClick){openFromTray();}});
    if(QSystemTrayIcon::isSystemTrayAvailable())tray->show();
    connect(&app->ddl,&DDLDepartment::reminder,this,[this](const QString &text){
        const auto message=!isActiveWindow()?"有事务到期，请打开 Z.T.L查看":text;
        notificationPage=2;
        if(tray->isVisible())tray->showMessage("Z.T.L · 事务提醒",message,QSystemTrayIcon::Information,10000);
    });
    connect(&app->mail,&MailGetService::pollingOver,this,[this](bool haveNew){if(haveNew&&tray->isVisible()){notificationPage=1;tray->showMessage("Z.T.L · 邮件","有新邮件，请前往 Mail 查看");}});
    connect(tray,&QSystemTrayIcon::messageClicked,this,[this]{openFromTray();navigate(notificationPage);});
    connect(mail,&MailWidget::taskDraft,this,[this](const QString &title,const QString &body){navigate(2);if(stack->currentWidget()==ddl)ddl->addDraft(title,body);});
    connect(mail,&MailWidget::noteDraft,this,[this](const QString &title,const QString &body){navigate(3);if(stack->currentWidget()==notes)notes->addDraft(title,body);});
    connect(settings,&SettingWidget::generalSettingsChanged,this,[this]{app->reloadSettings();app->home.refresh(true);});
    connect(settings,&SettingWidget::noteAction,this,[this](const QString &action){if(QStringList{"完整本地备份","恢复完整本地备份","存储统计","配置文件恢复"}.contains(action))dataAction(action);else notes->manage(action);});
    auto refreshSecretAction=[settings=settings,notes=&app->notes]{settings->setSecretInitialized(notes->exists());};
    connect(&app->notes,&NoteDepartment::changed,settings,refreshSecretAction);refreshSecretAction();
    connect(settings,&SettingWidget::saveStatus,this,[this](const QString &message){statusBar()->showMessage(message);});
    connect(CloudConnection::instance(),&CloudConnection::setupRequested,this,[this]{
        if(qApp->property("ztlSetupActive").toBool())return;
        if(!notes->prepareLeave())return;
        lock();qApp->setProperty("ztlSetupActive",true);app->mail.stop();
        CloudConnection::instance()->notifySettingsChanged();
        {SetupWizard wizard(false,this);wizard.exec();}
        qApp->setProperty("ztlSetupActive",false);CloudConnection::instance()->notifySettingsChanged();
        settings->resetUI();settings->setSecretInitialized(app->notes.exists());app->home.refresh(true);
    });
    auto draftWarning=[this](const QString &message){QTimer::singleShot(0,this,[this,message]{statusBar()->showMessage(message);});};
    connect(ddl,&DeadlineWidget::saveFailed,this,draftWarning);
    connect(notes,&NoteWidget::saveFailed,this,draftWarning);
    for(auto manager:{static_cast<QObject*>(ConfigManager::getInstance()),static_cast<QObject*>(CacheManager::getInstance()),static_cast<QObject*>(MailAccountManager::getInstance())})
        for(auto engine:manager->findChildren<HelpJsonIOEngine*>()){
            connect(engine,&HelpJsonIOEngine::writeFailed,this,[this](const QString &message){statusBar()->showMessage(message);});
            connect(engine,&HelpJsonIOEngine::writeRecovered,this,[this](const QString &message){statusBar()->showMessage(message,5000);});
        }
    activity.start();qApp->installEventFilter(this);qApp->installNativeEventFilter(this);
    app->clock.every("privacy",10,[this]{
        const int minutes=QSettings().value("idleMinutes",5).toInt();
        if(minutes>0&&activity.elapsed()>minutes*60000&&app->notes.unlocked())lock();
    });
    app->clock.every("calendar-resync",30,[this]{app->ddl.resyncTime();});
    app->clock.every("ddl-view",60,[this]{if(stack->currentWidget()==ddl)ddl->refresh();});
    auto geometry=QSettings().value("geometry").toByteArray();if(!geometry.isEmpty())restoreGeometry(geometry);
    setWindowState(Qt::WindowStates(QSettings().value("windowState",int(windowState())).toInt()) & (Qt::WindowMaximized|Qt::WindowFullScreen));
    stack->setCurrentWidget(home);
#ifdef Q_OS_WIN
    WTSRegisterSessionNotification(reinterpret_cast<HWND>(winId()),NOTIFY_FOR_THIS_SESSION);
#endif
    if(!app->tasks.ready())QMessageBox::critical(this,"事务数据库不可用",app->tasks.lastError());
    QTimer::singleShot(0,this,[this]{QFile f(Paths::data("logs/backup-operation.json"));if(f.open(QIODevice::ReadOnly)){auto r=QJsonDocument::fromJson(f.readAll()).object();f.close();QFile::remove(f.fileName());QMessageBox::information(this,"完整本地备份",r["ok"].toBool()?"操作完成："+r["archive"].toString():"操作失败："+r["error"].toString());}});
}
void MainWindow::dataAction(const QString &action) {
    if (action == "存储统计") {
        localData->showStatistics();
        return;
    }
    if (action == "配置文件恢复") {
        for (auto owner : QList<QObject*>{ConfigManager::getInstance(), CacheManager::getInstance(),
                                         MailAccountManager::getInstance()}) {
            for (auto engine : owner->findChildren<HelpJsonIOEngine*>()) {
                if (engine->readable()) continue;
                const auto choice = QMessageBox::warning(
                    this, "配置原件已保护",
                    engine->error() + "\n恢复备份后可选择重试；明确重建会先保留损坏原件，再保存当前配置。",
                    QMessageBox::Retry | QMessageBox::Reset | QMessageBox::Cancel, QMessageBox::Retry);
                QString error;
                if (choice != QMessageBox::Cancel && !engine->recover(choice == QMessageBox::Reset, &error))
                    QMessageBox::warning(this, "恢复失败", error);
            }
        }
        ConfigManager::getInstance()->reload();
        MailAccountManager::getInstance()->reload();
        settings->resetUI();
        app->reloadSettings();
        return;
    }
    const bool restore = action == "恢复完整本地备份";
    const QString filter = "Z.T.L 完整备份 (*.ztlbackup)";
    const auto path = restore
        ? QFileDialog::getOpenFileName(this, "选择完整本地备份", Paths::data("backups"), filter)
        : QFileDialog::getSaveFileName(this, "保存完整本地备份",
              Paths::data("backups/local-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") +
                          ".ztlbackup"), filter);
    if (path.isEmpty()) return;
    const auto message = restore
        ? "程序将退出，先备份现有数据，再恢复并重新启动。秘密仍使用原主密码；受 Windows 保护的凭据仅保证原用户可用。继续？"
        : "程序将保存并退出，由辅助工具创建一致的完整备份，完成后重新打开。继续？";
    if (QMessageBox::question(this, action, message) != QMessageBox::Yes || !notes->prepareLeave()) return;
    saveWindowState();
    const auto errors = app->flushAll();
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, "尚未保存", errors.join("\n"));
        return;
    }
    if (!LocalDataService::launchBackup(restore, path)) {
        QMessageBox::warning(this, "无法启动辅助工具", "请检查完整安装包中的 Python 运行时和 local_backup.py");
        return;
    }
    quitting = true;
    close();
}
MainWindow::~MainWindow(){
    qApp->removeEventFilter(this);qApp->removeNativeEventFilter(this);app->clock.remove("privacy");app->clock.remove("ddl-view");app->clock.remove("home-summary");app->clock.remove("calendar-resync");
#ifdef Q_OS_WIN
    WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(winId()));
#endif
}
void MainWindow::navigate(int page){
    if(page<0||page>=stack->count())return;
    if(stack->currentIndex()==3&&page!=3&&!notes->prepareLeave())return;
    if(page!=3&&app->notes.unlocked())app->notes.lock();
    if(page==4)settings->resetUI();
    const bool changed=stack->currentIndex()!=page;
    stack->setCurrentIndex(page);
    if(changed)UiMotion::install()->reveal(stack->currentWidget());
    activity.restart();
}
void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if(navigation)navigation->setCompact(width()<1080);
}
void MainWindow::refreshHomeSummary(){
    const auto today=QDate::currentDate();const auto counts=app->ddl.daySummary(today);
    const auto now=QDateTime::currentDateTime();QList<TaskOccurrence> context;
    for(const auto &o:app->ddl.occurrences(today,today))if(o.isLabel()||(o.isInterval()&&!o.paused&&o.issue.isEmpty()&&o.start<=now&&now<o.due))context.append(o);
    home->setCalendarItems(context);
    home->setSummary(today,counts.today,counts.tomorrow,counts.overdue,MailManager::getInstance()->unreadCount(),counts.available);
}
void MainWindow::lock(){
    app->notes.lock();activity.restart();
}
bool MainWindow::eventFilter(QObject *object,QEvent *event){
    if(event->type()==QEvent::KeyPress||event->type()==QEvent::MouseButtonPress||event->type()==QEvent::MouseMove)activity.restart();
    return QMainWindow::eventFilter(object,event);
}
bool MainWindow::nativeEventFilter(const QByteArray &,void *message,qintptr *){
#ifdef Q_OS_WIN
    auto msg=static_cast<MSG*>(message);
    if(msg->message==WM_TIMECHANGE||msg->message==WM_SETTINGCHANGE||(msg->message==WM_POWERBROADCAST&&(msg->wParam==PBT_APMRESUMEAUTOMATIC||msg->wParam==PBT_APMRESUMESUSPEND)))QTimer::singleShot(0,this,[this]{app->ddl.resyncTime();});
    if(msg->message==WM_WTSSESSION_CHANGE&&msg->wParam==WTS_SESSION_LOCK)lock();
#else
    Q_UNUSED(message)
#endif
    return false;
}
void MainWindow::closeEvent(QCloseEvent *event){
    if(!notes->prepareLeave()){event->ignore();return;}
    saveWindowState();
    if(!quitting&&tray->isVisible()&&QSettings().value("closeToTray",true).toBool()){
        lock();navigate(0);hide();event->ignore();return;
    }
    ddl->prepareLock();const auto errors=app->flushAll();
    if(!errors.isEmpty()){
        auto choice=QMessageBox::warning(this,"尚有内容未保存",errors.join("\n")+"\n请修复后重试；选择放弃将退出并丢弃尚未落盘的修改。",QMessageBox::Retry|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Retry);
        if(choice!=QMessageBox::Discard){quitting=false;event->ignore();return;}
    }
    quitting=true;app->shutdown();event->accept();qApp->quit();
}
void MainWindow::saveWindowState(){
    QSettings s;s.setValue("geometry",saveGeometry());s.setValue("windowState",int(windowState() & (Qt::WindowMaximized|Qt::WindowFullScreen)));s.sync();
}
void MainWindow::openFromTray(){
    auto state=Qt::WindowStates(QSettings().value("windowState",int(windowState())).toInt()) & (Qt::WindowMaximized|Qt::WindowFullScreen);
    setWindowState(state);show();raise();activateWindow();
}
void MainWindow::showHelp(){
    QMessageBox::information(this,"Z.T.L · 使用说明",
        "Alt+H 首页 · Alt+M 邮件 · Alt+C Calendar · Alt+N 私人信息\nAlt+S 设置 · Alt+L 立即锁定秘密 · F11 全屏 · F1 帮助\n"
        "Ctrl+N 在事务/Note新增；Ctrl+F检索。\n邮件删除仅进入本地回收站，不影响服务器。\n"
        "每日/每周事务按日期生成实例，完成本次不结束循环。\nNote：纸条双击展开，书签单击选中、点击编辑展开、双击小卡片打开；秘密双击验证密码。\nNote 删除需要确认，确认后永久删除。\n"
        "请用托盘菜单退出；程序真正退出后不会提醒。\n详细使用说明请阅读 instruct.md。");
}

















