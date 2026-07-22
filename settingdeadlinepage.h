#ifndef SETTINGDEADLINEPAGE_H
#define SETTINGDEADLINEPAGE_H



#include "constants.h"
#include "configmanager.h"
#include "settingmailpage.h"



#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QWheelEvent>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QButtonGroup>









class SettingDeadlinePage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingDeadlinePage(QWidget *parent = nullptr);

    void resetUI();

signals:




private:
    // ui们
    // 总的滚动元件
    QScrollArea *m_mainScroll;
    // 标题
    QLabel *m_lblTitle;
    // 基础配置
    QLabel *m_lblBasicalConfigTitle;
    // past数量
    QLabel *m_initPastText;
    QLabel *m_initPastNumber;
    QLineEdit *m_initPastInput;
    QPushButton *m_initPastBtn;
    // 保存时长
    QLabel *m_saveDurationText;
    NoWheelComboBox *m_saveDurationCombo;
    // 提醒时长
    QLabel *m_remindTimeText;
    NoWheelComboBox *m_remindTimeCombo;
    // 常用tag
    QLabel *m_lblCommonTagTest;
    QPushButton *m_upTagBtn;
    QPushButton *m_downTagBtn;
    QPushButton *m_deleteTagBtn;
    QVBoxLayout *m_contentTagLayout;
    // 添加tag
    QLabel *m_lblAddTagTest;
    QLineEdit *m_AddTagInput;
    QPushButton *m_AddTagSubmit;

    // 交互
    int m_clickedTagIndex;

    // 数据
    DeadlineBasicalConfig m_deadlineBasical;



};

#endif // SETTINGDEADLINEPAGE_H
