#include "deadlineaddtagselector.h"

DeadlineAddTagSelector::DeadlineAddTagSelector(QWidget *parent)
    : QWidget{parent}
{
    QVBoxLayout *mainLyt = new QVBoxLayout(this);
    QHBoxLayout *firstHLyt = new QHBoxLayout();
    QPushButton *m_firstTagBtn = new QPushButton("+");
    QPushButton *m_secondTagBtn = new QPushButton("+");
    firstHLyt->addWidget(m_firstTagBtn);
    firstHLyt->addWidget(m_secondTagBtn);
    m_tagBtns.append(m_firstTagBtn);
    m_tagBtns.append(m_secondTagBtn);
    mainLyt->addLayout(firstHLyt);
    QHBoxLayout *secondHLyt = new QHBoxLayout();
    QPushButton *m_thirdTagBtn = new QPushButton("+");
    QPushButton *m_forthTagBtn = new QPushButton("+");
    secondHLyt->addWidget(m_thirdTagBtn);
    secondHLyt->addWidget(m_forthTagBtn);
    m_tagBtns.append(m_thirdTagBtn);
    m_tagBtns.append(m_forthTagBtn);
    mainLyt->addLayout(secondHLyt);
    QHBoxLayout *thirdHLyt = new QHBoxLayout();
    m_customTagInput = new QLineEdit();
    QRegularExpression _re(R"(^(?:[\x{4e00}-\x{9fa5}_]{1,4}|[a-zA-Z_]{1,12})$)");
    QRegularExpressionValidator *_validator = new QRegularExpressionValidator(_re, this);
    m_customTagInput->setValidator(_validator);
    m_customTagInput->setPlaceholderText("描述");
    m_customTagBtn = new QPushButton("添加");
    thirdHLyt->addWidget(m_customTagInput);
    thirdHLyt->addWidget(m_customTagBtn);
    mainLyt->addLayout(thirdHLyt);
    m_commonTagList = new QListWidget();
    m_commonTagList->setFixedHeight(100);
    mainLyt->addWidget(m_commonTagList);

    connect(m_firstTagBtn, &QPushButton::clicked, this, [=](){
        if (m_selectTags.length() < 1) return;
        m_selectTags.removeAt(0);
        refreshBtns();
    });
    connect(m_secondTagBtn, &QPushButton::clicked, this, [=](){
        if (m_selectTags.length() < 2) return;
        m_selectTags.removeAt(1);
        refreshBtns();
    });
    connect(m_thirdTagBtn, &QPushButton::clicked, this, [=](){
        if (m_selectTags.length() < 3) return;
        m_selectTags.removeAt(2);
        refreshBtns();
    });
    connect(m_forthTagBtn, &QPushButton::clicked, this, [=](){
        if (m_selectTags.length() < 4) return;
        m_selectTags.removeAt(3);
        refreshBtns();
    });

    connect(m_customTagBtn, &QPushButton::clicked, this, [=](){
        if (m_selectTags.length() < 4) {
            if (m_selectTags.contains(m_customTagInput->text())) return;
            m_selectTags.append(m_customTagInput->text());
            refreshBtns();
        }
    });


}


void DeadlineAddTagSelector::loadData(const QList<QString>& commonTags,
              const QList<QString>& selectedTags){
    m_selectTags = selectedTags;
    m_commonTagList->clear();
    for (const QString& tag: commonTags){
        QListWidgetItem *item = new QListWidgetItem(m_commonTagList);
        item->setText(tag);
        item->setTextAlignment(Qt::AlignCenter);



    }
    connect(m_commonTagList, &QListWidget::itemClicked, this, [=](QListWidgetItem *item) {
        if (m_selectTags.contains(item->text())) return;
        qDebug() << "用户点击了 Item：" << item->text();
        if (m_selectTags.length() < 4) {
            m_selectTags.append(item->text());
            refreshBtns();
        }
    });

    refreshBtns();
    m_customTagInput->clear();





}


const QList<QString>& DeadlineAddTagSelector::text(){
    return m_selectTags;
}


void DeadlineAddTagSelector::refreshBtns(){
    for (int i = 0;i < m_selectTags.length();i++){
        m_tagBtns[i]->setText(m_selectTags[i]);
    }
    for (int i = m_selectTags.length();i < 4;i++){
        m_tagBtns[i]->setText("+");
    }
}






















