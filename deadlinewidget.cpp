#include "deadlinewidget.h"

DeadlineWidget::DeadlineWidget(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[DeadlineWidget] 启动。";
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_leftStack = new QStackedWidget();
    m_waittingPage = new QLabel("waitting page");
    m_deadlineAddWidget = new DeadlineAddWidget(this);
    m_leftStack->addWidget(m_waittingPage);
    m_leftStack->addWidget(m_deadlineAddWidget);
    m_leftStack->setCurrentIndex(0);
    QWidget *rightHolder = new QWidget();
    QVBoxLayout* rightLyt = new QVBoxLayout(rightHolder);
    QPushButton *awakeAdd = new QPushButton("Add");
    QPushButton *awakeModify = new QPushButton("Modify");
    QPushButton *cancelBtn = new QPushButton("Cancel");
    m_mapView = new DeadlineTimeMapView();
    m_mapView->setAlignment(Qt::AlignCenter);
    rightLyt->addWidget(awakeAdd);
    rightLyt->addWidget(awakeModify);
    rightLyt->addWidget(cancelBtn);
    rightLyt->addWidget(m_mapView);
    m_splitter->addWidget(m_leftStack);
    m_splitter->addWidget(rightHolder);
    m_splitter->setSizes(QList<int>() << 100 << 500);
    mainLayout->addWidget(m_splitter);

    m_action = 0;

    connect(awakeAdd, &QPushButton::clicked, this, [=](){
        m_deadlineAddWidget->awake(false, DeadlineEntity());
        m_leftStack->setCurrentIndex(1);
        m_action = 1;
    });
    connect(awakeModify, &QPushButton::clicked, this, [=](){
        m_deadlineAddWidget->awake(true, DeadlineEntity());
        m_leftStack->setCurrentIndex(1);
        m_action = 0;
    });
    connect(cancelBtn, &QPushButton::clicked, this, [=](){
        m_leftStack->setCurrentIndex(0);
        m_action = 0;
    });

    connect(m_deadlineAddWidget, &DeadlineAddWidget::addNewDeadlineRequest, this,
            [=](const DeadlineEntity& newDDL){
        if (m_action == 1) DeadlineManager::getInstance()->addDDL(newDDL);
        else if (m_action == 2) DeadlineManager::getInstance()->updateDDL(newDDL);
        m_leftStack->setCurrentIndex(0);
        m_action = 0;

    });




}
