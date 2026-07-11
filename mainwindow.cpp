#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // 开机自检
    qInfo() << "[main] 开机自检与构建。";
    // 启动 Manager
    ConfigManager::getInstance()->initialize();
    CacheManager::getInstance()->initialize();
    // 脚本文件夹
    qInfo() << "[main] 自检脚本。";
    QFile fileScripts("./scripts");
    if (!fileScripts.exists()) {
        qWarning() << "[main] 脚本文件夹不存在！";
    } else {
        QList<QString> scripts = {"getWeather.py", "getQuote.py"};
        for (const QString &filename:scripts){
            QFile fileThis("./scripts/" + filename);
            if (!fileThis.exists()) {
                qWarning() << "[main] 脚本文件不存在：" << filename;
            }
        }
    }
    qInfo() << "[main] 脚本自检完毕。";
    qInfo() << "[main] 启动UI。";
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
    m_stackedWidget->setCurrentIndex(0);


    // 换页处理
    QShortcut *homeShortcut = new QShortcut(ZTL::Main::homeShortcut, this);
    QShortcut *settingShortcut = new QShortcut(ZTL::Main::settingShortcut, this);

    connect(homeShortcut, &QShortcut::activated, this, [=](){
        m_stackedWidget->setCurrentWidget(m_homeWidget);
    });
    connect(settingShortcut, &QShortcut::activated, this, [=](){
        m_stackedWidget->setCurrentWidget(m_settingWidget);
        m_settingWidget->resetUI();
    });

    // 链接



}

void MainWindow::closeEvent(QCloseEvent *event){
    qDebug() << "[main] 程序将要关闭。";
    ConfigManager::getInstance()->saveConfig();

}

MainWindow::~MainWindow()
{
    delete ui;
}

















