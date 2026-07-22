#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

#include "constants.h"
#include "settinghomepage.h"
#include "settingmailpage.h"
#include "settingdeadlinepage.h"

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QDebug>

class SettingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SettingWidget(QWidget *parent = nullptr);

    void resetUI();

signals:

private:
    // 左侧按钮区
    QButtonGroup *m_buttons;
    // 栈
    QStackedWidget *m_pages;
    // 页面们
    SettingHomePage *m_homePage;
    SettingMailPage *m_mailPage;
    SettingDeadlinePage *m_deadlinePage;
    QLabel *m_waitingPage;



};

#endif // SETTINGWIDGET_H
