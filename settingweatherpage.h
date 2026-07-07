#ifndef SETTINGWEATHERPAGE_H
#define SETTINGWEATHERPAGE_H

#include "datatype.h"
#include "configmanager.h"

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>

class SettingWeatherPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingWeatherPage(QWidget *parent = nullptr);

signals:

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
    // 信息们
    HomeWeatherConfig m_locationConfig;




};

#endif // SETTINGWEATHERPAGE_H
