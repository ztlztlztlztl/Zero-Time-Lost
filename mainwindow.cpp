#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 栈
    m_stackedWidget = new QStackedWidget(ui->centralwidget);
    // 页面们
    m_homeWidget = new HomeWidget(m_stackedWidget);
    m_settingWidget = new SettingWidget(m_stackedWidget);


    // 组装
    m_stackedWidget->addWidget(m_homeWidget);
    m_stackedWidget->addWidget(m_settingWidget);

    this->setCentralWidget(m_stackedWidget);
    m_stackedWidget->setCurrentWidget(m_homeWidget);


    // 换页处理
    QShortcut *homeShortcut = new QShortcut(ZTL::Main::homeShortcut, this);
    QShortcut *settingShortcut = new QShortcut(ZTL::Main::settingShortcut, this);

    connect(homeShortcut, &QShortcut::activated, this, [&](){
        m_stackedWidget->setCurrentWidget(m_homeWidget);
    });
    connect(settingShortcut, &QShortcut::activated, this, [&](){
        m_stackedWidget->setCurrentWidget(m_settingWidget);
    });

}

MainWindow::~MainWindow()
{
    delete ui;
}

















