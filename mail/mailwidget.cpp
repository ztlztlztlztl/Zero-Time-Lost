#include "mailwidget.h"
#include "mailmanager.h"
#include "mailstyle.h"

MailWidget::MailWidget(MailGetService *department,QWidget *parent):QWidget(parent),m_mailGetService(department){
    setAttribute(Qt::WA_StyledBackground,true);setObjectName("mailPage");setStyleSheet(MailStyle::sheet);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(24,16,24,14);layout->setSpacing(12);
    pages=new QStackedWidget;pages->setObjectName("mailPages");layout->addWidget(pages,1);
    m_mailSimpleDisplay=new MailSimpleDisplayWidget;pages->addWidget(m_mailSimpleDisplay);
    auto reading=new QWidget;reading->setObjectName("mailReadingPage");auto right=new QVBoxLayout(reading);right->setContentsMargins(0,0,0,0);right->setSpacing(12);
    toolbar=new QGridLayout;toolbar->setContentsMargins(0,0,0,0);toolbar->setHorizontalSpacing(14);toolbar->setVerticalSpacing(8);right->addLayout(toolbar);
    backButton=new MailStyle::Button("← 返回",true);backButton->setObjectName("mailBack");
    draftActions=new QWidget;auto drafts=new QHBoxLayout(draftActions);drafts->setContentsMargins(0,0,0,0);drafts->setSpacing(10);
    taskButton=new MailStyle::Button("转为事务",true);taskButton->setObjectName("mailToTask");noteButton=new MailStyle::Button("保存纸条",true);noteButton->setObjectName("mailToNote");drafts->addWidget(taskButton);drafts->addWidget(noteButton);
    mailActions=new QWidget;auto actions=new QHBoxLayout(mailActions);actions->setContentsMargins(0,0,0,0);actions->setSpacing(10);
    starButton=new MailStyle::Button("收藏",true);starButton->setObjectName("mailStar");deleteButton=new MailStyle::Button("删除",true);deleteButton->setObjectName("mailReadingDelete");deleteButton->setProperty("danger",true);actions->addWidget(starButton);actions->addWidget(deleteButton);
    arrangeToolbar();
    auto bodyPage=new QWidget;bodyPage->setObjectName("mailBodyPage");auto bodyLayout=new QVBoxLayout(bodyPage);bodyLayout->setContentsMargins(0,0,0,0);
    m_mailBodyReader=new MailBodyReader;bodyLayout->addWidget(m_mailBodyReader);right->addWidget(bodyPage,1);pages->addWidget(reading);
    syncStatus=new QLabel("就绪");syncStatus->setObjectName("mailSyncStatus");syncStatus->setTextFormat(Qt::PlainText);syncStatus->setFixedHeight(22);syncStatus->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);layout->addWidget(syncStatus);
    connect(backButton,&QPushButton::clicked,this,[this]{
        pages->setCurrentIndex(0);selected=MailBrief{};m_mailBodyReader->loadMail(QString());m_mailSimpleDisplay->reload();m_mailSimpleDisplay->findChild<QTreeWidget*>("mailRows")->setFocus();
    });
    auto backKey=new QShortcut(QKeySequence(Qt::Key_Escape),reading);backKey->setContext(Qt::WidgetWithChildrenShortcut);connect(backKey,&QShortcut::activated,backButton,&QPushButton::click);
    connect(starButton,&QPushButton::clicked,this,[this]{selected.starred=!selected.starred;MailManager::getInstance()->setStar(selected.ownerEmail,selected.uid,selected.starred);starButton->setText(selected.starred?"取消收藏":"收藏");});
    connect(deleteButton,&QPushButton::clicked,this,[this]{MailManager::getInstance()->deleteMail(selected.ownerEmail,selected.uid);backButton->click();});
    connect(m_mailSimpleDisplay,&MailSimpleDisplayWidget::refreshRequest,department,&MailGetService::mailPolling);
    connect(m_mailSimpleDisplay,&MailSimpleDisplayWidget::historyRequest,department,&MailGetService::mailHistory);
    connect(m_mailSimpleDisplay,&MailSimpleDisplayWidget::restoreRequest,department,&MailGetService::restoreMail);
    connect(department,&MailGetService::restoreFinished,m_mailSimpleDisplay,&MailSimpleDisplayWidget::restoreFinished);
    connect(department,&MailGetService::pollingOver,this,[this]{m_mailSimpleDisplay->enablePollingBtn();});
    connect(department,&MailGetService::status,this,[this](const QString &message){syncStatus->setText(message.isEmpty()?"就绪":QString(message).replace('\n',' '));syncStatus->setToolTip(message);});
    connect(m_mailSimpleDisplay,&MailSimpleDisplayWidget::readRequest,this,[this](const MailAccount &,const MailBrief &brief){
        selected=brief;pages->setCurrentIndex(1);starButton->setText(brief.starred?"取消收藏":"收藏");requestBody();backButton->setFocus();
    });
    connect(department,&MailGetService::newMailBody,this,[](const QString &address,const QString &id,const QString &body){MailManager::getInstance()->updateMailBody(address,id,body);});
    connect(department,&MailGetService::bodyFailed,this,[this](const QString &address,const QString &id,const QString &message){if(address==selected.ownerEmail&&id==selected.uid)bodyError=message;});
    connect(department,&MailGetService::getBodyOver,this,[this](const QString &address,const QString &id){
        if(address!=selected.ownerEmail||id!=selected.uid)return;
        const auto body=MailManager::getInstance()->getMailBody(address,id);
        if(body.isEmpty())m_mailBodyReader->showError(bodyError.isEmpty()?"未能获取邮件正文。请检查网络和邮箱账号设置。":bodyError);
        else {m_mailBodyReader->loadMail(selected,body);taskButton->setEnabled(true);noteButton->setEnabled(true);}
    });
    connect(m_mailBodyReader,&MailBodyReader::retryRequested,this,&MailWidget::requestBody);
    auto source=[this]{return "来源邮件："+selected.sender+" / "+selected.subject+"\n账号："+selected.ownerEmail+"\n"+m_mailBodyReader->plainText();};
    connect(taskButton,&QPushButton::clicked,this,[this,source]{if(!selected.uid.isEmpty()&&taskButton->isEnabled())emit taskDraft(selected.subject,source());});
    connect(noteButton,&QPushButton::clicked,this,[this,source]{if(!selected.uid.isEmpty()&&noteButton->isEnabled())emit noteDraft(selected.subject,source());});
}
void MailWidget::requestBody(){
    if(selected.uid.isEmpty())return;
    bodyError.clear();taskButton->setEnabled(false);noteButton->setEnabled(false);
    const auto body=MailManager::getInstance()->getMailBody(selected.ownerEmail,selected.uid);m_mailBodyReader->loadMail(selected,body);
    if(!body.isEmpty()){taskButton->setEnabled(true);noteButton->setEnabled(true);return;}
    const auto account=MailAccountManager::getInstance()->getMailAcc(selected.ownerEmail);
    if(account.address.isEmpty()){m_mailBodyReader->showError("原邮箱账号尚未配置，请先在设置中添加账号。");return;}
    m_mailBodyReader->showLoading();m_mailGetService->mailBody(account,selected.uid);
}
void MailWidget::showUnread(){if(pages->currentIndex()==1)backButton->click();m_mailSimpleDisplay->showUnread();}
void MailWidget::arrangeToolbar(){
    toolbar->removeWidget(backButton);toolbar->removeWidget(draftActions);toolbar->removeWidget(mailActions);
    toolbar->setColumnStretch(1,1);toolbar->addWidget(backButton,0,0,Qt::AlignLeft);
    if(width()<850){toolbar->addWidget(mailActions,0,2,Qt::AlignRight);toolbar->addWidget(draftActions,1,0,1,3,Qt::AlignLeft);}
    else{toolbar->addWidget(draftActions,0,1,Qt::AlignRight);toolbar->addWidget(mailActions,0,2,Qt::AlignRight);}
}
void MailWidget::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);arrangeToolbar();}
