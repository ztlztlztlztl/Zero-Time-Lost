#include "mailbodyreader.h"
#include "mailstyle.h"
#include <QDesktopServices>

namespace {
class RestrictedBrowser:public QTextBrowser {
public:using QTextBrowser::QTextBrowser;
protected:QVariant loadResource(int,const QUrl &) override {return {};}
};

// Work on the parsed document, so inline CSS, font tags and table-cell colours
// cannot override the dark reading palette. The cached HTML stays untouched.
void normalizeDark(QTextDocument *document){
    auto clearFrame=[&](auto &&self,QTextFrame *frame)->void{
        auto format=frame->frameFormat();format.clearProperty(QTextFormat::BackgroundBrush);format.clearProperty(QTextFormat::ForegroundBrush);format.setBorderBrush(QColor("#555b66"));frame->setFrameFormat(format);
        if(auto table=qobject_cast<QTextTable*>(frame))for(int r=0;r<table->rows();++r)for(int c=0;c<table->columns();++c){
            auto cell=table->cellAt(r,c);auto cellFormat=cell.format();cellFormat.clearProperty(QTextFormat::BackgroundBrush);cellFormat.clearProperty(QTextFormat::ForegroundBrush);cell.setFormat(cellFormat);
        }
        for(auto child:frame->childFrames())self(self,child);
    };
    clearFrame(clearFrame,document->rootFrame());
    struct Span {int start,length;QTextCharFormat format;};QList<Span> spans;
    for(auto block=document->begin();block.isValid();block=block.next()){
        QTextCursor cursor(block);auto blockFormat=block.blockFormat();blockFormat.clearProperty(QTextFormat::BackgroundBrush);blockFormat.clearProperty(QTextFormat::ForegroundBrush);cursor.setBlockFormat(blockFormat);
        for(auto it=block.begin();!it.atEnd();++it){
            const auto fragment=it.fragment();if(!fragment.isValid())continue;
            auto format=fragment.charFormat();format.clearProperty(QTextFormat::ForegroundBrush);format.clearProperty(QTextFormat::BackgroundBrush);
            if(format.isAnchor())format.setForeground(QColor("#d3af80"));
            spans.append({fragment.position(),fragment.length(),format});
        }
    }
    for(const auto &span:spans){QTextCursor cursor(document);cursor.setPosition(span.start);cursor.setPosition(span.start+span.length,QTextCursor::KeepAnchor);cursor.setCharFormat(span.format);}
}
}

MailBodyReader::MailBodyReader(QWidget *parent):QWidget(parent){
    setObjectName("mailReader");auto outer=new QHBoxLayout(this);outer->setContentsMargins(0,8,0,0);outer->setSpacing(0);
    auto content=new QWidget;content->setObjectName("mailReadingColumn");content->setMaximumWidth(840);content->setMinimumWidth(0);content->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    outer->addStretch();outer->addWidget(content,1);outer->addStretch();
    auto layout=new QVBoxLayout(content);layout->setContentsMargins(0,8,0,0);layout->setSpacing(12);
    auto label=[&](const QString &name){auto result=new QLabel;result->setObjectName(name);result->setTextFormat(Qt::PlainText);result->setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);result->setWordWrap(true);result->setMinimumWidth(0);return result;};
    subject=label("mailSubject");sender=label("mailSender");recipient=label("mailRecipient");date=label("mailDate");date->setWordWrap(false);date->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Preferred);
    layout->addWidget(subject);layout->addWidget(sender);
    auto metadata=new QHBoxLayout;metadata->setSpacing(20);metadata->addWidget(recipient,1);metadata->addWidget(date);layout->addLayout(metadata);
    auto divider=new QFrame;divider->setObjectName("mailReaderDivider");divider->setFixedHeight(1);layout->addWidget(divider);
    auto display=new QHBoxLayout;auto caption=new QLabel("正文显示");caption->setProperty("role","muted");display->addWidget(caption);
    mode=new QComboBox;mode->setObjectName("mailDisplayMode");mode->addItems({"自动","浅色","深色","纯文本"});mode->setAccessibleName("正文显示方式");mode->setMinimumWidth(110);display->addWidget(mode);display->addStretch();layout->addLayout(display);
    bodyStack=new QStackedWidget;bodyStack->setObjectName("mailBodyStack");layout->addWidget(bodyStack,1);
    browser=new RestrictedBrowser;browser->setObjectName("mailContent");browser->setReadOnly(true);browser->setFrameShape(QFrame::NoFrame);browser->setOpenExternalLinks(false);browser->setOpenLinks(false);
    browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);browser->document()->setUndoRedoEnabled(false);
    auto option=browser->document()->defaultTextOption();option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);browser->document()->setDefaultTextOption(option);
    bodyStack->addWidget(browser);
    auto state=new QWidget;auto stateLayout=new QVBoxLayout(state);stateLayout->setContentsMargins(24,24,24,24);stateLayout->addStretch();
    stateText=new QLabel;stateText->setObjectName("mailBodyStatus");stateText->setAlignment(Qt::AlignCenter);stateText->setWordWrap(true);stateText->setTextFormat(Qt::PlainText);stateText->setProperty("role","muted");stateLayout->addWidget(stateText);
    retry=new MailStyle::Button("重试",true);retry->setObjectName("mailBodyRetry");stateLayout->addWidget(retry,0,Qt::AlignHCenter);stateLayout->addStretch();bodyStack->addWidget(state);
    connect(retry,&QPushButton::clicked,this,&MailBodyReader::retryRequested);
    connect(mode,&QComboBox::currentIndexChanged,this,&MailBodyReader::render);
    connect(browser,&QTextBrowser::anchorClicked,this,[](const QUrl &url){if(url.scheme()=="https"||url.scheme()=="http")QDesktopServices::openUrl(url);});
}

void MailBodyReader::loadMail(const MailBrief &brief,const QString &htmlBody){
    const auto key=brief.ownerEmail+"/"+brief.uid;if(identity!=key){identity=key;QSignalBlocker blocker(mode);mode->setCurrentIndex(0);}
    subject->setText(brief.subject.isEmpty()?"（无主题）":brief.subject);sender->setText("发件人："+brief.sender);recipient->setText("收件账号："+brief.ownerEmail);
    date->setText(QDateTime::fromSecsSinceEpoch(brief.timestamp).toString("yyyy-MM-dd HH:mm:ss"));date->ensurePolished();date->setMinimumWidth(date->fontMetrics().horizontalAdvance(date->text())+2);loadMail(htmlBody);
}
void MailBodyReader::loadMail(const QString &htmlBody){source=htmlBody;render();}
void MailBodyReader::showLoading(){
    source.clear();text.clear();browser->clear();mode->setEnabled(false);bodyStack->setCurrentIndex(1);stateText->setText("正在获取正文…");retry->hide();
}
void MailBodyReader::showError(const QString &message){
    mode->setEnabled(false);bodyStack->setCurrentIndex(1);stateText->setText(message+"\n可重试，或返回列表稍后阅读。");retry->show();retry->setEnabled(true);
}
void MailBodyReader::render(){
    if(source.isEmpty()){showLoading();return;}
    static const QRegularExpression styled("<(?:table|img|style)\\b|\\b(?:style|bgcolor|color|background)\\s*=",QRegularExpression::CaseInsensitiveOption);
    const int selected=mode->currentIndex();const bool light=selected==1||(selected==0&&source.contains(styled));
    const QString base=light?"#f3f3f0":"#14171d",foreground=light?"#20242a":"#d0d0d0",link=light?"#754218":"#d3af80";
    browser->setProperty("readingTheme",light?"light":"dark");
    browser->setStyleSheet(QString("QTextBrowser#mailContent {background:%1;color:%2;border:1px solid %3;border-radius:5px;padding:6px;selection-background-color:%4;selection-color:%2;font-size:16px;}").arg(base,foreground,light?"#d8d8d2":"#343841",light?"#dccbb5":"#46372a"));
    QFont font("Microsoft YaHei UI");font.setPixelSize(16);browser->document()->setDefaultFont(font);browser->document()->setDocumentMargin(14);
    browser->document()->setDefaultStyleSheet(QString("body {color:%1;} p {margin-top:0px;margin-bottom:12px;} a {color:%2;} pre {white-space:pre-wrap;font-family:'Microsoft YaHei UI';}").arg(foreground,link));
    browser->setHtml(source);text=browser->toPlainText();
    if(selected==3)browser->setPlainText(text);
    else if(!light)normalizeDark(browser->document());
    mode->setEnabled(true);bodyStack->setCurrentIndex(0);
}
