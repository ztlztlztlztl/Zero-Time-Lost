#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    HomeWidget *home = new HomeWidget(ui->centralwidget);

    QVBoxLayout *layout = new QVBoxLayout(ui->centralwidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(home); // 纯代码流大组装成功！
}

MainWindow::~MainWindow()
{
    delete ui;
}
