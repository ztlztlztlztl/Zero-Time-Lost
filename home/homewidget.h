#ifndef HOMEWIDGET_H
#define HOMEWIDGET_H

#include "homebackgroundshader.h"
#include "homeinfowidget.h"
#include "homeweatherservice.h"
#include "homequoteservice.h"

#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QSpacerItem>

class HomeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HomeWidget(QWidget *parent = nullptr);
    ~HomeWidget();



signals:

private:
    HomeBackgroundShader *m_background; // 背景
    HomeInfoWidget *m_infoWidget; // 信息板

    HomeWeatherService *m_weatherService; // 天气服务器
    HomeQuoteService *m_quoteService; // 每日一言服务器
};

#endif // HOMEWIDGET_H
