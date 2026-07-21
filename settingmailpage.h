#ifndef SETTINGMAILPAGE_H
#define SETTINGMAILPAGE_H

#include "constants.h"
#include "configmanager.h"
#include "settingmailaccountswidget.h"

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
#include <QRegularExpressionValidator>
#include <QWheelEvent>



class NoWheelComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit NoWheelComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
    void wheelEvent(QWheelEvent *e) override {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        e->ignore(); // Qt6 写法：直接忽略事件，交给父级（比如 ScrollArea）去正常滚动
#else
        // Qt5 写法：如果不希望影响父级滚动，直接不调用基类 QComboBox::wheelEvent(e); 即可
        // 如果希望父级滚动，可以调用 e->ignore();
#endif
    }
};







class SettingMailPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingMailPage(QWidget *parent = nullptr);
    void resetUI();


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
    NoWheelComboBox *m_pollingIntervalCombo;
    QLabel *m_lblInitialNumberTest; // 开机从本地抓取的邮件卡片数
    NoWheelComboBox *m_initialNumberCombo;
    QLabel *m_lblEarlierNumberTest; // 每次查看更早新看到的的邮件卡片数
    NoWheelComboBox *m_earlierNumberCombo;
    QLabel *m_lblStorageLimitTest; // 本地储存邮件的上限
    QLabel *m_storageLimitDisplay;
    QLineEdit *m_storageLimitInput;
    QPushButton *m_storageSubmitBtn;
    // 邮箱账号展示
    QLabel *m_lblAccountsTitle;
    QLabel *m_lblSavedAccountsTest;
    QWidget *m_accountsWidget; // 展示邮箱的列表
    QVBoxLayout *m_accountWidgetLyt;
    QLabel *m_lblMakeAccountsTest;
    QStackedWidget *m_makeAccountStack; // 添加邮箱的部分
    QPushButton *m_makeAccountWaiting;
    SettingMailAccountsWidget *m_makeAccountDo;

    // 数据
    MailBasicalConfig m_mailBasicalConfig;
    QList<MailAccount> m_mailAccounts;


};

#endif // SETTINGMAILPAGE_H
