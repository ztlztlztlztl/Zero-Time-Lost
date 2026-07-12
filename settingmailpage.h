#ifndef SETTINGMAILPAGE_H
#define SETTINGMAILPAGE_H

#include "constants.h"

#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QStackedWidget>

class SettingMailPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingMailPage(QWidget *parent = nullptr);

signals:



private:
    // ui们
    // 总的滚动元件
    QScrollArea *m_mainScroll;
    // 标题
    QLabel *m_lblTitle;
    // 基础配置部分
    QLabel *m_lblBasicalConfigTitle;
    QLabel *m_lblPollingIntervalTest; // 轮询间隔
    QComboBox *m_pollingIntervalCombo;
    QLabel *m_lblInitialNumberTest; // 开机从本地抓取的邮件卡片数
    QComboBox *m_initialNumberCombo;
    QLabel *m_lblEarlierNumberTest; // 每次查看更早新看到的的邮件卡片数
    QComboBox *m_earlierNumberCombo;
    QLabel *m_lblStorageLimitTest; // 本地储存邮件的上限
    QLabel *m_storageLimitDisplay;
    QLineEdit *m_storageLimitInput;
    QPushButton *m_storageSubmitBtn;
    // 邮箱账号展示
    QLabel *m_lblAccountsTitle;
    QLabel *m_lblSavedAccountsTest;
    QListWidget *m_accountsListWidget; // 展示邮箱的列表
    QLabel *m_lblAddAccountsTest;
    QStackedWidget *m_addAccountStack; // 添加邮箱的部分


};

#endif // SETTINGMAILPAGE_H
