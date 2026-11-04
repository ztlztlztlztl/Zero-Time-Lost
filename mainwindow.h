#pragma once
#include <QMainWindow>
#include <QElapsedTimer>
#include <QAbstractNativeEventFilter>
class AppContext;class QStackedWidget;class HomeWidget;class SettingWidget;class MailWidget;class DeadlineWidget;class NoteWidget;class QSystemTrayIcon;
class MainWindow:public QMainWindow,public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit MainWindow(AppContext *context,QWidget *parent=nullptr);
    ~MainWindow() override;
    void lock();
    void openFromTray();
    void saveWindowState();
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *object,QEvent *event) override;
    bool nativeEventFilter(const QByteArray &,void *,qintptr *) override;
private:
    void navigate(int page);
    void showHelp();
    void refreshHomeSummary();
    AppContext *app;
    QStackedWidget *stack;
    HomeWidget *home;
    SettingWidget *settings;
    MailWidget *mail;
    DeadlineWidget *ddl;
    NoteWidget *notes;
    QSystemTrayIcon *tray;
    QElapsedTimer activity;
    bool quitting=false;
    int notificationPage=2;
};
