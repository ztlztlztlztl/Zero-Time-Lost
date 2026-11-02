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
#include <QtWidgets>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wtsapi32.h>
#endif
MainWindow::MainWindow(AppContext *context,QWidget *parent):QMainWindow(parent),app(context){
    setWindowTitle("Z.T.L");resize(1180,760);
    stack=new QStackedWidget;setCentralWidget(stack);
    home=new HomeWidget(&app->home);settings=new SettingWidget;mail=new MailWidget(&app->mail);ddl=new DeadlineWidget(&app->ddl);notes=new NoteWidget(&app->notes);
    stack->addWidget(home);stack->addWidget(mail);stack->addWidget(ddl);stack->addWidget(notes);stack->addWidget(settings);
    connect(home,&HomeWidget::ddlRequested,this,[this](const QDate &date){navigate(2);ddl->showDay(date);});
    connect(home,&HomeWidget::mailRequested,this,[this]{navigate(1);mail->showUnread();});
    connect(home,&HomeWidget::summaryRefreshRequested,this,&MainWindow::refreshHomeSummary);
    connect(&app->ddl,&DDLDepartment::changed,this,&MainWindow::refreshHomeSummary);
    connect(MailManager::getInstance(),&MailManager::changed,this,&MainWindow::refreshHomeSummary);
    connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state){
        if(state==Qt::ApplicationActive)refreshHomeSummary();
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
    auto sync=trayMenu->addAction("立即收信");connect(sync,&QAction::triggered,&app->mail,&MailGetService::mailPolling);
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
    connect(settings,&SettingWidget::noteAction,notes,&NoteWidget::manage);
    connect(settings,&SettingWidget::saveStatus,this,[this](const QString &message){statusBar()->showMessage(message);});
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
    app->clock.every("ddl-view",60,[this]{if(stack->currentWidget()==ddl)ddl->refresh();});
    auto geometry=QSettings().value("geometry").toByteArray();if(!geometry.isEmpty())restoreGeometry(geometry);
    setWindowState(Qt::WindowStates(QSettings().value("windowState",int(windowState())).toInt()) & (Qt::WindowMaximized|Qt::WindowFullScreen));
    stack->setCurrentWidget(home);
#ifdef Q_OS_WIN
    WTSRegisterSessionNotification(reinterpret_cast<HWND>(winId()),NOTIFY_FOR_THIS_SESSION);
#endif
    if(!app->tasks.ready())QMessageBox::critical(this,"事务数据库不可用",app->tasks.lastError());
}
MainWindow::~MainWindow(){
    qApp->removeEventFilter(this);qApp->removeNativeEventFilter(this);app->clock.remove("privacy");app->clock.remove("ddl-view");app->clock.remove("home-summary");
#ifdef Q_OS_WIN
    WTSUnRegisterSessionNotification(reinterpret_cast<HWND>(winId()));
#endif
}
void MainWindow::navigate(int page){
    if(stack->currentIndex()==3&&page!=3&&!notes->prepareLeave())return;
    if(page!=3&&app->notes.unlocked())app->notes.lock();
    if(page==4)settings->resetUI();
    stack->setCurrentIndex(page);activity.restart();
}
void MainWindow::refreshHomeSummary(){
    const auto today=QDate::currentDate();const auto counts=app->ddl.daySummary(today);
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
    quitting=true;ddl->prepareLock();app->shutdown();event->accept();qApp->quit();
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

















