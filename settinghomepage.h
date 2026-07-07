#ifndef SETTINGHOMEPAGE_H
#define SETTINGHOMEPAGE_H

#include "datatype.h"
#include "configmanager.h"

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QTimer>

class SettingHomePage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingHomePage(QWidget *parent = nullptr);

    void resetUI();

signals:
    void weatherSaveRequest();

private:
    // ui们
    // 标题
    QLabel *m_lblTitle;
    // 当前城市
    QLabel *m_lblLocationTest;
    QLabel *m_lblLocationCity;
    // 收藏城市列表
    QLabel *m_lblCollectedTest;
    QScrollArea *m_collectedScroll;
    QVBoxLayout *m_contentLayout;
    // 添加城市
    QLabel *m_lblAddTest;
    QLineEdit *m_addCityInput;
    QLineEdit *m_addCodeInput;
    QPushButton *m_addSubmit;
    // 信息们
    HomeWeatherConfig m_locationConfig;

    // 函数们
    bool addCity();




};

#endif // SETTINGHOMEPAGE_H
