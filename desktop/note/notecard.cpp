#include "uipalette.h"
#include "notecard.h"
#include "notestyle.h"
#include "settingcontrols.h"

namespace {
// Plain text summaries wrap to a fixed number of lines and elide the last one.
class SummaryLabel:public QLabel {
public:
    SummaryLabel(int lines,int pixels,const QColor &color):lines(lines){
        QFont face("DengXian");face.setPixelSize(pixels);setFont(face);
        setStyleSheet(QString("color:%1;background:transparent;font-family:'DengXian';font-size:%2px;").arg(color.name()).arg(pixels));
        setTextFormat(Qt::PlainText);setAttribute(Qt::WA_TransparentForMouseEvents);
        setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        setFixedHeight(QFontMetrics(face).lineSpacing()*lines);
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);painter.setPen(palette().color(QPalette::WindowText));
        QTextLayout text(textForLayout(),font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);text.setTextOption(option);
        text.beginLayout();
        for(int i=0;i<lines;++i){
            auto line=text.createLine();if(!line.isValid())break;line.setLineWidth(width());
            const qreal y=i*fontMetrics().lineSpacing();
            if(i==lines-1&&line.textStart()+line.textLength()<text.text().size()){
                auto tail=text.text().mid(line.textStart());const int newline=tail.indexOf(QChar::LineSeparator);
                if(newline>=0)tail=tail.left(newline)+QChar(0x2026);
                painter.drawText(QPointF(0,y+fontMetrics().ascent()),fontMetrics().elidedText(tail,Qt::ElideRight,width()));
            }else line.draw(&painter,QPointF(0,y));
        }
        text.endLayout();
    }
private:
    QString textForLayout() const {auto value=QLabel::text();return value.replace('\n',QChar::LineSeparator);}
    int lines;
};
class LockMark:public QWidget {
public:
    LockMark(){setObjectName("noteLockMark");setFixedSize(18,21);setToolTip("正文已加密，双击验证后查看");}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(UiPalette::color("#678573"),1.4));
        painter.drawRoundedRect(QRectF(3,9,12,10),2,2);painter.drawArc(QRectF(5,2,8,13),0,180*16);painter.drawLine(QPointF(9,13),QPointF(9,16));
    }
};
}

NoteFields::NoteFields(const QJsonObject &e,QWidget *parent):QWidget(parent),original(e){
    setObjectName("noteFields");
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(8);
    auto label=[&](const QString &text){auto caption=new QLabel(text);caption->setProperty("noteRole","field");layout->addWidget(caption);};
    auto pending=[this]{setError({});status->setText("等待保存…");autosave.start();};
    autosave.setSingleShot(true);autosave.setInterval(500);
    connect(&autosave,&QTimer::timeout,this,[this]{status->setText("保存中…");emit saveRequested();});
    label("标题");title=new QLineEdit(e["title"].toString());title->setObjectName("noteTitle");title->setPlaceholderText("输入标题");layout->addWidget(title);title->installEventFilter(this);
    if(e["kind"].toInt()==1){
        label("目标");targetType=new NoWheelComboBox;targetType->setObjectName("noteTargetType");targetType->addItem("网站","website");targetType->addItem("文件","file");targetType->addItem("文件夹","folder");
        targetType->setCurrentIndex(qMax(0,targetType->findData(e["targetType"].toString())));layout->addWidget(targetType);
        auto row=new QHBoxLayout;target=new QLineEdit(e["target"].toString());target->setObjectName("noteTarget");target->installEventFilter(this);
        auto browse=new NoteStyle::Button("选择…",true);browse->setObjectName("noteBrowse");row->addWidget(target,1);row->addWidget(browse);layout->addLayout(row);
        auto browserLabel=new QLabel("浏览器");layout->addWidget(browserLabel);browser=new NoWheelComboBox;browser->setObjectName("noteBrowser");browser->addItem("默认浏览器","default");browser->addItem("Edge","edge");browser->addItem("Chrome","chrome");browser->setCurrentIndex(qMax(0,browser->findData(e["browser"].toString("default"))));layout->addWidget(browser);connect(browser,&QComboBox::currentIndexChanged,this,pending);
        auto mode=[this,browse,browserLabel]{bool website=targetType->currentData()=="website";target->setReadOnly(!website);target->setPlaceholderText(website?"https://example.edu":"请选择本地目标");browse->setVisible(!website);browser->setVisible(website);browserLabel->setVisible(website);};
        connect(targetType,&QComboBox::currentIndexChanged,this,[mode,pending]{mode();pending();});mode();
        connect(target,&QLineEdit::textChanged,this,pending);
        connect(browse,&QPushButton::clicked,this,[this]{
            auto path=targetType->currentData()=="file"?QFileDialog::getOpenFileName(this,"选择文件",target->text(),"全部文件 (*)"):QFileDialog::getExistingDirectory(this,"选择文件夹",target->text(),QFileDialog::ShowDirsOnly);
            if(!path.isEmpty()){target->setText(QDir::toNativeSeparators(path));emit saveRequested();}
        });
    }else{
        label("正文");body=new QTextEdit;body->setObjectName("noteBody");body->setAcceptRichText(false);body->setPlainText(e["body"].toString());body->setPlaceholderText(e["kind"].toInt()==0?"输入需要加密保存的内容":"记下值得留住的片段…");body->installEventFilter(this);body->setMinimumHeight(110);layout->addWidget(body,1);
        connect(body,&QTextEdit::textChanged,this,pending);
    }
    if(e["kind"].toInt()!=0){
        label("标签");tags=new DeadlineTagPicker(e["tags"].toString(),nullptr,true);tags->setObjectName("noteTagChoices");
        tags->setStyleSheet(UiPalette::style("QListWidget {border:1px solid #343841;background:#14171d;outline:0;} QListWidget::item {height:24px;padding:2px 8px;border:1px solid transparent;} QListWidget::item:hover,QListWidget::item:selected {border:1px solid #176a39;background:transparent;} QListWidget::item:disabled {color:#555b64;}"));
        layout->addWidget(tags);tags->findChild<QListWidget*>("ddlSelectedTags")->setObjectName("noteSelectedTags");tags->findChild<QListWidget*>("ddlAvailableTags")->setObjectName("noteAvailableTags");
        connect(tags,&DeadlineTagPicker::selectionChanged,this,[this]{autosave.stop();status->setText("保存中…");emit saveRequested();});
    }
    if(target)layout->addStretch();
    feedback=new QLabel;feedback->setObjectName("noteError");feedback->setTextFormat(Qt::PlainText);feedback->setWordWrap(true);feedback->hide();layout->addWidget(feedback);
    status=new QLabel("已保存");status->setObjectName("noteSaveStatus");layout->addWidget(status);
    connect(title,&QLineEdit::textChanged,this,pending);
}
QJsonObject NoteFields::values() const {
    auto e=original;e["title"]=title->text().trimmed();
    if(tags)e["tags"]=tags->selectedTags();else e.remove("tags");
    if(body)e["body"]=body->toPlainText();
    if(target){e["targetType"]=targetType->currentData().toString();e["target"]=target->text().trimmed();e.remove("body");if(targetType->currentData()=="website")e["browser"]=browser->currentData().toString();else e.remove("browser");}
    return e;
}
bool NoteFields::dirty() const {
    auto e=values(),baseline=original;
    for(const auto &field:{"title","body","tags","target","targetType","browser"}){
        const auto before=QString(field)=="browser"&&baseline["targetType"]=="website"?baseline[field].toString("default"):baseline[field].toString();
        if(e[field].toString()!=before)return true;
    }
    return false;
}
void NoteFields::setCreationMode(){status->hide();}
void NoteFields::committed(const QJsonObject &e){
    autosave.stop();QSignalBlocker a(title);title->setText(e["title"].toString());
    if(body&&body->toPlainText()!=e["body"].toString()){QSignalBlocker b(body);body->setPlainText(e["body"].toString());}
    if(tags){QSignalBlocker b(tags);tags->setSelectedTags(e["tags"].toString());}
    if(target){QSignalBlocker b(target),c(targetType),d(browser);target->setText(e["target"].toString());targetType->setCurrentIndex(qMax(0,targetType->findData(e["targetType"].toString())));browser->setCurrentIndex(qMax(0,browser->findData(e["browser"].toString("default"))));}
    original=e;setError({});status->setText("已保存");
}
void NoteFields::setError(const QString &message){
    feedback->setText(message);feedback->setVisible(!message.isEmpty());status->setProperty("failed",!message.isEmpty());
    status->style()->unpolish(status);status->style()->polish(status);
    if(!message.isEmpty())status->setText("保存失败，输入已保留");
}
bool NoteFields::eventFilter(QObject *object,QEvent *event){
    if(event->type()==QEvent::FocusOut){autosave.stop();QTimer::singleShot(0,this,[this]{emit saveRequested();});}
    return QWidget::eventFilter(object,event);
}
NoteCard::NoteCard(const QJsonObject &e,NoteDepartment *department,QWidget *parent):QFrame(parent),dept(department),record(e){
    setObjectName("noteCard");setProperty("noteId",id());setProperty("selected",false);setFocusPolicy(Qt::StrongFocus);setAttribute(Qt::WA_StyledBackground,true);
    layout=new QVBoxLayout(this);layout->setContentsMargins(16,14,16,14);layout->setSpacing(0);
    small=new QWidget;small->setObjectName("noteCardSummary");auto content=new QVBoxLayout(small);content->setContentsMargins(0,0,0,0);content->setSpacing(7);
    heading=new SummaryLabel(2,16,UiPalette::color("#d5dcda"));heading->setObjectName("noteCardTitle");content->addWidget(heading);
    type=new SummaryLabel(1,12,UiPalette::color("#789183"));type->setObjectName("noteCardType");content->addWidget(type);
    preview=new SummaryLabel(e["kind"].toInt()==2?3:1,14,UiPalette::color("#929a9f"));preview->setObjectName("noteCardPreview");content->addWidget(preview);
    content->addStretch();
    meta=new SummaryLabel(1,12,UiPalette::color("#789183"));meta->setObjectName("noteCardTags");content->addWidget(meta);
    if(e["kind"].toInt()==0)content->addWidget(new LockMark,0,Qt::AlignRight);
    if(e["kind"].toInt()==1){
        auto actions=new QHBoxLayout;actions->setSpacing(4);
        auto open=new NoteStyle::Button("打开",true),locate=new NoteStyle::Button("定位",true),edit=new NoteStyle::Button("编辑",true);
        open->setObjectName("noteBookmarkOpen");locate->setObjectName("noteBookmarkLocate");edit->setObjectName("noteBookmarkEdit");
        open->setToolTip("用所选浏览器或系统默认程序打开目标");locate->setToolTip("在资源管理器中定位文件");
        for(auto button:{open,locate,edit})actions->addWidget(button);content->addLayout(actions);
        connect(open,&QPushButton::clicked,this,[this]{emit activated(this);});
        connect(locate,&QPushButton::clicked,this,[this]{emit locateRequested(this);});
        connect(edit,&QPushButton::clicked,this,[this]{emit selected(this,false);emit expansionRequested(this);});
    }
    layout->addWidget(small);updateRecord(e);
}
void NoteCard::updateRecord(const QJsonObject &e){
    if(auto locate=findChild<QPushButton*>("noteBookmarkLocate"))locate->setVisible(e["targetType"]=="file");
    record=e;heading->setText(e["title"].toString());heading->setToolTip(e["title"].toString());setAccessibleName(e["title"].toString());
    const int kind=e["kind"].toInt();type->setVisible(kind==1);preview->setVisible(kind!=0);meta->setVisible(kind!=0&&!e["tags"].toString().isEmpty());
    meta->setText(e["tags"].toString().replace(",","   "));meta->setToolTip(e["tags"].toString());
    if(kind==2)preview->setText(e["body"].toString());
    else if(kind==1){
        const auto target=e["target"].toString(),targetType=e["targetType"].toString();
        type->setText(targetType=="website"?"网站":targetType=="folder"?"文件夹":"文件");
        preview->setText(targetType=="website"?QUrl(target).host():QFileInfo(QDir::fromNativeSeparators(target)).fileName());
        if(preview->text().isEmpty())preview->setText(QDir::toNativeSeparators(target));
        preview->setToolTip(target);small->setToolTip(target);
    }else{preview->clear();meta->clear();}
}
void NoteCard::select(bool selected){
    if(property("selected").toBool()==selected)return;
    setProperty("selected",selected);style()->unpolish(this);style()->polish(this);update();
}
void NoteCard::expand(){
    if(editor||record["kind"].toInt()==0)return;
    small->hide();auto scroller=new QScrollArea;scroller->setObjectName("noteInlineScroll");scroller->viewport()->setObjectName("noteInlineViewport");scroller->setWidgetResizable(true);scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroller->setMinimumSize(0,0);large=scroller;
    auto inner=new QWidget;inner->setObjectName("noteCardEditor");scroller->setWidget(inner);auto content=new QVBoxLayout(inner);content->setContentsMargins(0,0,4,0);content->setSpacing(16);
    auto bar=new QHBoxLayout;auto hint=new QLabel(record["kind"].toInt()==1?"编辑书签":"编辑纸条");hint->setProperty("noteRole","field");bar->addWidget(hint);bar->addStretch();
    hint->setObjectName("noteDragHandle");hint->setToolTip("长按拖动排序");
    auto close=new NoteStyle::Button("收起",true),remove=new NoteStyle::Button("删除",true);close->setObjectName("noteCollapse");remove->setObjectName("noteDelete");remove->setProperty("danger",true);
    bar->addWidget(close);bar->addWidget(remove);content->addLayout(bar);
    editor=new NoteFields(record);content->addWidget(editor,1);layout->addWidget(large);
    conflictActions=new QWidget;auto actions=new QHBoxLayout(conflictActions);actions->setContentsMargins(0,0,0,0);
    auto copy=new NoteStyle::Button("保存输入为副本",true),latest=new NoteStyle::Button("采用仓库最新内容",true);actions->addWidget(copy);actions->addWidget(latest);content->addWidget(conflictActions);conflictActions->hide();
    connect(copy,&QPushButton::clicked,this,[this]{auto e=editor->values();e.remove("id");e["title"]=e["title"].toString()+"（副本）";if(!dept->save(e)){editor->setError(dept->error());return;}editor->committed(record);collapse();});
    connect(latest,&QPushButton::clicked,this,[this]{auto e=dept->entry(id());if(e.isEmpty()){editor->committed(record);collapse();emit collapseRequested(this);return;}updateRecord(e);editor->committed(e);conflictActions->hide();});
    connect(editor,&NoteFields::saveRequested,this,[this]{save();});
    connect(close,&QPushButton::clicked,this,[this]{emit collapseRequested(this);});
    connect(remove,&QPushButton::clicked,this,[this]{emit deleteRequested(this);});
}
bool NoteCard::save(){
    if(!editor)return true;
    auto e=editor->values(),latest=dept->entry(id());
    if(!editor->dirty()){if(!latest.isEmpty()){updateRecord(latest);editor->committed(latest);}return true;}
    QStringList conflicts;
    if(latest.isEmpty())conflicts.append("记录已被删除");
    else for(const auto &field:{"title","body","tags","target","targetType","browser"}){
        if(e[field]==record[field]){if(latest.contains(field))e[field]=latest[field];else e.remove(field);}
        else if(latest[field]!=record[field]&&latest[field]!=e[field])conflicts.append(QString::fromLatin1(field));
    }
    if(!conflicts.isEmpty()){editor->setError("仓库内容已改变："+conflicts.join("、")+"。输入已保留，请保存副本或采用最新内容。");conflictActions->show();return false;}
    if(!dept->save(e)){editor->setError(dept->error());emit saveFailed(dept->error());return false;}
    updateRecord(dept->entry(id()));editor->committed(record);return true;
}
void NoteCard::collapse(){
    if(!large)return;
    large->hide();layout->removeWidget(large);disconnect(editor,nullptr,this,nullptr);large->deleteLater();large=nullptr;editor=nullptr;conflictActions=nullptr;small->show();
}
void NoteCard::mousePressEvent(QMouseEvent *event){
    if(event->button()==Qt::LeftButton){emit selected(this,true);event->accept();return;}
    QFrame::mousePressEvent(event);
}
void NoteCard::mouseDoubleClickEvent(QMouseEvent *event){
    if(event->button()==Qt::LeftButton){if(expanded()){QFrame::mouseDoubleClickEvent(event);return;}emit selected(this,false);if(record["kind"].toInt()==2)emit expansionRequested(this);else emit activated(this);event->accept();return;}
    QFrame::mouseDoubleClickEvent(event);
}
void NoteCard::keyPressEvent(QKeyEvent *event){
    if(event->modifiers()==Qt::AltModifier&&(event->key()==Qt::Key_Up||event->key()==Qt::Key_Down)){emit moveRequested(this,event->key()==Qt::Key_Up?-1:1);return;}
    if(expanded()){QFrame::keyPressEvent(event);return;}
    if(event->key()==Qt::Key_Space){emit selected(this,true);event->accept();return;}
    if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter){emit selected(this,false);if(record["kind"].toInt()==2)emit expansionRequested(this);else emit activated(this);return;}
    QFrame::keyPressEvent(event);
}
void NoteCard::focusInEvent(QFocusEvent *event){
    QFrame::focusInEvent(event);setProperty("keyboardFocus",event->reason()==Qt::TabFocusReason||event->reason()==Qt::BacktabFocusReason);style()->unpolish(this);style()->polish(this);
}
void NoteCard::focusOutEvent(QFocusEvent *event){
    QFrame::focusOutEvent(event);setProperty("keyboardFocus",false);style()->unpolish(this);style()->polish(this);
}


void NoteCard::setDragReady(bool ready){setProperty("dragReady",ready);update();emit dragReadyChanged(ready);}
void NoteCard::paintEvent(QPaintEvent *event){
    QFrame::paintEvent(event);
    if(property("dragReady").toBool()){QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);painter.setPen(QPen(UiPalette::color("#64e9a2"),3));painter.drawLine(8,2,width()-8,2);}
}
