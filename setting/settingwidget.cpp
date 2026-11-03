#include "pageheader.h"
#include "settingwidget.h"
#include "settinghomepage.h"
#include "settingmailpage.h"
#include "settingdeadlinepage.h"
#include "generalsettings.h"
#include "settingcontrols.h"
#include "uistyle.h"
#include <QtWidgets>
SettingWidget::SettingWidget(QWidget *parent):QWidget(parent){
    setAttribute(Qt::WA_StyledBackground,true);setObjectName("settingPage");setStyleSheet(UiStyle::sheet);
    setFocusPolicy(Qt::StrongFocus);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(24,16,24,16);
    addPageHeader(layout,"设置");
    auto scroll=new QScrollArea;scroll->setObjectName("settingsScroll");scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);auto content=new QWidget;
    auto groups=new QVBoxLayout(content);groups->setSpacing(0);groups->setContentsMargins(0,0,12,20);groups->setSizeConstraint(QLayout::SetMinAndMaxSize);
    m_homePage=new SettingHomePage(this);m_mailPage=new SettingMailPage(this);m_deadlinePage=new SettingDeadlinePage;
    const QList<QWidget*> existing{m_homePage,m_mailPage,m_deadlinePage};
    const QStringList names{"Home","Mail","DDL","Note","general"};
    const QList<QColor> accents{QColor("#0c7362"),QColor("#754218"),QColor("#7f2636"),QColor("#176a39"),QColor("#463d73")};
    for(int i=0;i<names.size();++i){
        if(i>0){auto separator=SettingControls::boundary(accents[i-1],accents[i],content);separator->setObjectName("settingsBoundary"+names[i-1]+names[i]);groups->addWidget(separator);}
        auto group=new QWidget;group->setObjectName("settingsGroup"+names[i]);SettingControls::applyTheme(group,accents[i]);auto section=new QVBoxLayout(group);
        section->setContentsMargins(0,0,0,0);section->setSpacing(16);section->setSizeConstraint(QLayout::SetMinAndMaxSize);
        auto heading=new QWidget;heading->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Preferred);auto header=new QVBoxLayout(heading);header->setContentsMargins(0,0,0,4);header->setSpacing(0);
        auto title=new QLabel(i==4?"Advance":i==2?"Calendar":names[i]);title->setProperty("role","settingTitle");title->setProperty("afterBoundary",i>0);title->setObjectName("settingsTitle"+names[i]);title->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        title->setIndent(0);title->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
        auto band=new QFrame;band->setObjectName("settingsBand"+names[i]);band->setFixedHeight(4);
        band->setStyleSheet("border:0;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 "+accents[i].name()+",stop:1 #101216);");
        header->addWidget(title);header->addWidget(band);section->addWidget(heading,0,Qt::AlignLeft);
        if(i!=1){
            auto options=new GeneralSettings(static_cast<GeneralSettings::Section>(i),group);section->addWidget(options);
            connect(options,&GeneralSettings::applied,this,&SettingWidget::generalSettingsChanged);
            connect(options,&GeneralSettings::noteAction,this,&SettingWidget::noteAction);
            connect(options,&GeneralSettings::saveStatus,this,&SettingWidget::saveStatus);
        }
        if(i<3){
            auto page=existing[i];
            page->layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);section->addWidget(page);
        }
        if(i==3)section->addWidget(new SettingDeadlinePage(group,true));
        groups->addWidget(group);
    }
    groups->addStretch();scroll->setWidget(content);layout->addWidget(scroll);
}
void SettingWidget::resetUI(){m_homePage->resetUI();m_mailPage->resetUI();m_deadlinePage->resetUI();}
void SettingWidget::showEvent(QShowEvent *event){
    QWidget::showEvent(event);
    for(auto list:findChildren<QListWidget*>()){list->setCurrentRow(-1);list->clearSelection();}
    // Give initial keyboard focus to the page, keeping controls neutral until interacted with.
    setFocus(Qt::OtherFocusReason);
}
