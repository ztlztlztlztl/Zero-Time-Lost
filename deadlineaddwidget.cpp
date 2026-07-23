#include "deadlineaddwidget.h"

DeadlineAddWidget::DeadlineAddWidget(QWidget *parent)
    : QWidget{parent}
{

    QVBoxLayout *mainLyt = new QVBoxLayout(this);
    m_dueTimeText = new QLabel("截止时间");
    m_dueTimeEdit = new QDateTimeEdit(this);
    m_dueTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm");
    m_dueTimeEdit->setDateTime(QDateTime::currentDateTime());
    m_dueTimeEdit->setCalendarPopup(true);
    m_dueTimeEdit->setMinimumDateTime(QDateTime::currentDateTime());
    m_dueTimeEdit->setMaximumDate(QDate(2030, 12, 31));

    mainLyt->addWidget(m_dueTimeText);
    mainLyt->addWidget(m_dueTimeEdit);



    m_titleText = new QLabel("标题");
    m_titleInput = new QLineEdit();
    m_titleInput->setPlaceholderText("最多10个字");
    QRegularExpression re(R"(^(?:[\x{4e00}-\x{9fa5} ]{1,10}|[a-zA-Z ]{1,20})$)");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re, this);
    m_titleInput->setValidator(validator);

    mainLyt->addWidget(m_titleText);
    mainLyt->addWidget(m_titleInput);

    m_describeText = new QLabel("描述");
    m_describeInput = new QTextEdit();

    mainLyt->addWidget(m_describeText);
    mainLyt->addWidget(m_describeInput);

    m_tagsText = new QLabel("tag");
    m_tagsSelect = new DeadlineAddTagSelector(this);
    m_tagsSelect->loadData({},{});

    mainLyt->addWidget(m_tagsText);
    mainLyt->addWidget(m_tagsSelect);

    m_saveBtn = new QPushButton("保存");
    mainLyt->addWidget(m_saveBtn);

    mainLyt->addStretch();

    connect(m_saveBtn, &QPushButton::clicked, this, [=](){
        if (m_titleInput->text().isEmpty()) return;
        DeadlineEntity newDDL;
        newDDL.title = m_titleInput->text();
        newDDL.describe = m_describeInput->toPlainText();
        QDateTime dateTime = m_dueTimeEdit->dateTime();
        qint64 timestampSec = dateTime.toSecsSinceEpoch();
        newDDL.dueTimestamp = timestampSec;
        newDDL.tags = m_tagsSelect->text();
        emit addNewDeadlineRequest(newDDL);
    });




}

void DeadlineAddWidget::awake(bool flag, const DeadlineEntity& deadline){
    if (flag){
        m_dueTimeEdit->setDateTime(QDateTime::fromSecsSinceEpoch(deadline.dueTimestamp));
        m_titleInput->setText(deadline.title);
        m_describeInput->setText(deadline.describe);
        m_tagsSelect->loadData(ConfigManager::getInstance()->getDeadlineBasical().commonTags,
                               deadline.tags);
        m_dueTimeEdit->setMinimumDateTime(QDateTime::currentDateTime());
    }
    else{
        m_dueTimeEdit->setDateTime(QDateTime::currentDateTime());
        m_titleInput->setText("");
        m_describeInput->setText("");
        m_tagsSelect->loadData(ConfigManager::getInstance()->getDeadlineBasical().commonTags,
                               {});
        m_dueTimeEdit->setMinimumDateTime(QDateTime::currentDateTime());
    }
}

