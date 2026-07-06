#include "settingwidget.h"

SettingWidget::SettingWidget(QWidget *parent)
    : QWidget{parent}
{
    this->setAttribute(Qt::WA_StyledBackground, true);
    this->setObjectName("settingPage");
    this->setStyleSheet(ZTL::Setting::global::backgroundStyle);
    // 总布局
    QHBoxLayout *generalLayout = new QHBoxLayout(this);

    // 按钮们
    QVBoxLayout *buttonsLayout = new QVBoxLayout();
    m_buttons = new QButtonGroup(this);
    QStringList names = {"首页", "邮箱", "AI"};
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
    m_buttons->button(0)->setChecked(true);


    // 页面们
    m_pages = new QStackedWidget();
    for (int i = 0;i < 3;i++){
        QLabel *test = new QLabel(QString("page %1").arg(i), this);
        m_pages->addWidget(test);
    }
    m_pages->setCurrentIndex(0);

    // 连接们
    connect(m_buttons, &QButtonGroup::idClicked, this, [=](int id) {
        m_pages->setCurrentIndex(id);
    });

    // 组装
    generalLayout->addLayout(buttonsLayout);
    generalLayout->addWidget(m_pages);

}
