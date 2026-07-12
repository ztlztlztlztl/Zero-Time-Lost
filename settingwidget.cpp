#include "settingwidget.h"

SettingWidget::SettingWidget(QWidget *parent)
    : QWidget{parent}
{
    qDebug() << "[SettingWidget] 启动。";
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setObjectName("settingPage");
    this->setStyleSheet(ZTL::Setting::global::backgroundStyle);
    // 总布局
    QHBoxLayout *generalLayout = new QHBoxLayout(this);

    // 按钮们
    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    m_buttons = new QButtonGroup(this);
    QStringList names = {"首页", "邮件", "待补充"};
    for (int i = 0; i < 3;i++){
        QPushButton *btn = new QPushButton(names[i]);
        btn->setCheckable(true);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        btn->setMinimumWidth(ZTL::Setting::buttonlist::minwidth);
        btn->setMaximumWidth(ZTL::Setting::buttonlist::maxwidth);
        btn->setStyleSheet(ZTL::Setting::buttonlist::buttonStyle);
        buttonsLayout->addWidget(btn);
        m_buttons->addButton(btn, i);
    }
    m_buttons->button(1)->setChecked(true);

    // 页面们
    m_pages = new QStackedWidget();
    m_homePage = new SettingHomePage(m_pages);
    m_pages->addWidget(m_homePage);
    m_mailPage = new SettingMailPage(m_pages);
    m_pages->addWidget(m_mailPage);
    m_waitingPage = new QLabel("未完待续", m_pages);
    m_pages->addWidget(m_waitingPage);
    m_pages->setCurrentIndex(1);

    // 连接们
    connect(m_buttons, &QButtonGroup::idClicked, this, [=](int id) {
        m_pages->setCurrentIndex(id);
        qDebug() << "[SettingWidget] 切换到了" << names[id] << "页";
    });

    // 组装
    generalLayout->addLayout(buttonsLayout);
    generalLayout->addWidget(m_pages);

}

void SettingWidget::resetUI(){
    qDebug() << "[SettingWidget] 刷新setting页。";
    m_homePage->resetUI();
}