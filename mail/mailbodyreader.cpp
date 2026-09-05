#include "mailbodyreader.h"

MailBodyReader::MailBodyReader(QWidget *parent)
    : QWidget{parent}
{
    qInfo() << "[MailBodyReader] 启动。";
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20); // 留出呼吸感
    mainLayout->setSpacing(15);

    m_lblSubject = new QLabel("邮件标题将显示在这里", this);
    m_lblSubject->setStyleSheet(ZTL::Mail::reader::LblSubjectStyle);
    m_lblSubject->setWordWrap(true); // 标题太长允许换行

    m_lblAddress = new QLabel("收件邮箱", this);
    m_lblAddress->setStyleSheet(ZTL::Mail::reader::LblInfoStyle);
    m_lblSender = new QLabel("发件人", this);
    m_lblSender->setStyleSheet(ZTL::Mail::reader::LblInfoStyle);


    m_lblDate = new QLabel("时间", this);
    m_lblDate->setStyleSheet(ZTL::Mail::reader::LblInfoStyle);



    mainLayout->addWidget(m_lblSubject);
    mainLayout->addWidget(m_lblAddress);
    mainLayout->addWidget(m_lblSender);
    mainLayout->addWidget(m_lblDate);

    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);

    mainLayout->addWidget(line);

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setStyleSheet(ZTL::Mail::reader::TestBrowserStyle);

    m_textBrowser->setFrameStyle(QFrame::NoFrame);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setOpenExternalLinks(true);

    mainLayout->addWidget(m_textBrowser, 1);
}



void MailBodyReader::loadMail(const MailBrief &mailBrief, const QString &htmlBody) {
    m_lblSubject->setText(mailBrief.subject);
    m_lblAddress->setText(QString("收件邮箱：%1").arg(mailBrief.ownerEmail));
    m_lblSender->setText(QString("Sender：%1").arg(mailBrief.sender));
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(mailBrief.timestamp);
    m_lblDate->setText(QString("Time：%1").arg(dateTime.toString("yyyy-MM-dd hh:mm:ss")));

    m_textBrowser->setHtml(htmlBody);
}

void MailBodyReader::loadMail(const QString &htmlBody) {
    m_textBrowser->setHtml(htmlBody);
}



























