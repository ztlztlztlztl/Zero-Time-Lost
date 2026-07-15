#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "homewidget.h"
#include "settingwidget.h"
#include "configmanager.h"
#include "mailwidget.h"
#include "mailaccountmanager.h"
#include "mailmanager.h"

#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QKeyEvent>
#include <QDebug>
#include <QFile>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;



protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    // 一个stackedwidget 作为所有页面的承载
    QStackedWidget *m_stackedWidget;
    // 页面们
    HomeWidget *m_homeWidget;
    SettingWidget *m_settingWidget;
    MailWidget *m_mailWidget;















};
#endif // MAINWINDOW_H
