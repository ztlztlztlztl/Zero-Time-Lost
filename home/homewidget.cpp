#include "homewidget.h"
#include "homebackgroundshader.h"
#include "homeinfowidget.h"
#include "homedepartment.h"
#include <QtWidgets>

HomeWidget::HomeWidget(HomeDepartment *department,QWidget *parent):QWidget(parent){
    setObjectName("homePage");setFocusPolicy(Qt::StrongFocus);setAttribute(Qt::WA_StyledBackground);setStyleSheet("QWidget#homePage {background:#101216;} QLabel {background:transparent;font-family:'Microsoft YaHei UI';color:#8c939d;}");
    auto grid=new QGridLayout(this);grid->setContentsMargins(0,0,0,0);grid->setSpacing(0);
    background=new HomeBackgroundShader(this);grid->addWidget(background,0,0);
    auto overlay=new QWidget;overlay->setObjectName("homeOverlay");overlay->setStyleSheet("QWidget#homeOverlay {background:transparent;}");
    auto layout=new QVBoxLayout(overlay);layout->setContentsMargins(40,24,40,28);layout->setSpacing(0);
    layout->addStretch(1);info=new HomeInfoWidget;layout->addWidget(info);layout->addStretch(6);
    auto footer=new QHBoxLayout;footer->setSpacing(36);
    auto weatherGroup=new QWidget;weatherGroup->setObjectName("homeWeatherGroup");auto weatherLayout=new QVBoxLayout(weatherGroup);weatherLayout->setContentsMargins(0,0,0,0);weatherLayout->setSpacing(5);
    location=new QLabel("天气");location->setObjectName("homeLocation");location->setStyleSheet("font-size:12px;");location->setTextFormat(Qt::PlainText);weatherLayout->addWidget(location);
    auto conditions=new QHBoxLayout;conditions->setContentsMargins(0,0,0,0);conditions->setSpacing(10);
    temperature=new QLabel;temperature->setObjectName("homeTemperature");temperature->setStyleSheet("color:#b3bac3;font:22px 'Segoe UI';");
    weather=new QLabel("正在获取天气");weather->setObjectName("homeWeather");weather->setTextFormat(Qt::PlainText);weather->setStyleSheet("font-size:13px;");weather->setWordWrap(true);
    conditions->addWidget(temperature);conditions->addWidget(weather,1);weatherLayout->addLayout(conditions);
    summary=new QLabel("正在读取待办…");summary->setObjectName("homeSummary");summary->setStyleSheet("font-size:12px;");summary->setAlignment(Qt::AlignRight|Qt::AlignBottom);summary->setWordWrap(true);summary->setMinimumWidth(0);
    summary->setTextFormat(Qt::RichText);summary->setOpenExternalLinks(false);summary->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::LinksAccessibleByKeyboard);
    footer->addWidget(weatherGroup,1,Qt::AlignBottom);footer->addStretch();footer->addWidget(summary,2,Qt::AlignBottom);layout->addLayout(footer);grid->addWidget(overlay,0,0);
    connect(department,&HomeDepartment::weatherUpdated,this,&HomeWidget::updateWeather);
    connect(background,&HomeBackgroundShader::flowTimeChanged,info,&HomeInfoWidget::setFlowTime);
    connect(info,&HomeInfoWidget::dayChanged,this,&HomeWidget::summaryRefreshRequested);
    connect(summary,&QLabel::linkHovered,this,[this](const QString &link){if(hoveredLink!=link){hoveredLink=link;updateSummaryText();}});
    connect(summary,&QLabel::linkActivated,this,[this](const QString &link){if(link=="mail")emit mailRequested();else emit ddlRequested(link=="tomorrow"?summaryDate.addDays(1):summaryDate);});
}
void HomeWidget::setSummary(const QDate &date,int today,int tomorrow,int overdue,qint64 unread,bool available){
    summaryDate=date;todayCount=today;tomorrowCount=tomorrow;overdueCount=overdue;unreadCount=unread;summaryAvailable=available&&unread>=0;updateSummaryText();
}
void HomeWidget::updateSummaryText(){
    auto link=[&](const QString &key,const QString &text){return QString("<a href='%1' style='color:#8c939d;text-decoration:%2;'>%3</a>").arg(key,hoveredLink==key?"underline":"none",text.toHtmlEscaped());};
    QString text;
    if(!summaryAvailable)text="待办统计暂不可用";
    else if(todayCount>0){text=link("today",QString("今天还有 %1 个 DDL").arg(todayCount));if(unreadCount>0)text+="，"+link("mail",QString("还有 %1 封邮件未处理").arg(unreadCount));}
    else if(unreadCount>0)text=link("today","今天的 DDL 已清空")+"，"+link("mail",QString("还有 %1 封邮件未处理").arg(unreadCount));
    else if(tomorrowCount>0)text=link("tomorrow",QString("今天的事都干完了，明天尚有 %1 个 DDL").arg(tomorrowCount));
    else if(overdueCount>0)text=link("today",QString("今天暂无待办，仍有 %1 个逾期 DDL").arg(overdueCount));
    else text=link("today","最近的事都干完了");
    summary->setText(text);summary->setToolTip("DDL 统计今天截止且未完成的事务，排除暂停项；邮件统计本地未读且未删除的邮件。\n最近指今天和明天；逾期提示统计日程中保留的往日单次事务。点击文字进入对应页面。");
}
void HomeWidget::updateWeather(const QString &name,const QString &value,const QString &description){
    location->setText(name.isEmpty()||name=="无"?"天气":name);const bool unavailable=value=="--"||value.isEmpty();
    temperature->setText(unavailable?QString():value+"°");temperature->setVisible(!unavailable);weather->setText(unavailable?"天气暂不可用":description);
    location->setToolTip(description);weather->setToolTip(description);
}
void HomeWidget::showEvent(QShowEvent *event){QWidget::showEvent(event);setFocus(Qt::OtherFocusReason);info->refreshDateTime();emit summaryRefreshRequested();}
