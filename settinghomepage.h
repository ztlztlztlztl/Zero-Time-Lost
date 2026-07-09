#ifndef SETTINGHOMEPAGE_H
#define SETTINGHOMEPAGE_H

#include "constants.h"
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
#include <QButtonGroup>
#include <QComboBox>

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
    // 总的滚动元件
    QScrollArea *m_mainScroll;
    // 标题
    QLabel *m_lblTitle;
    // 天气部分
    QLabel *m_lblWeatherTitle;
    // 当前城市
    QLabel *m_lblLocationTest;
    QLabel *m_lblLocationCity;
    // 收藏城市列表
    QLabel *m_lblCollectedLocationTest;
    QPushButton *m_upLocationBtn;
    QPushButton *m_downLocationBtn;
    QPushButton *m_selectLocationBtn;
    QPushButton *m_deleteLocationBtn;
    QVBoxLayout *m_contentLocationLayout;
    int m_clickedLocationIndex;
    // 添加城市
    QLabel *m_lblAddLocationTest;
    QLineEdit *m_addCityInput;
    QLineEdit *m_addCodeInput;
    QPushButton *m_addLocationSubmit;
    // 每日一言部分
    QLabel *m_lblQuoteTitle;
    // 当前主题
    QLabel *m_lblThemeTest;
    QLabel *m_lblThemeContent;
    // 收藏主题列表
    QLabel *m_lblCollectedThemeTest;
    QPushButton *m_upThemeBtn;
    QPushButton *m_downThemeBtn;
    QPushButton *m_selectThemeBtn;
    QPushButton *m_deleteThemeBtn;
    QVBoxLayout *m_contentThemeLayout;
    int m_clickedThemeIndex;
    // 添加主题
    QLabel *m_lblAddThemeTest;
    QComboBox *m_addThemeComboBox;
    QPushButton *m_addThemeSubmit;
    // 信息们
    HomeWeatherConfig m_locationConfig;
    HomeQuoteConfig m_themeConfig;

    // 函数们
    bool addCity();
    bool addTheme();




};

#endif // SETTINGHOMEPAGE_H
