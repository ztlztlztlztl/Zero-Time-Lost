#include "pageheader.h"
#include "notewidget.h"
#include "bookmarktarget.h"
#include "configmanager.h"
#include "notestyle.h"
#include <algorithm>
#include <limits>
#include "paths.h"
#include "popuppolicy.h"
#include "draftstore.h"
namespace {
// Route Esc, window-close and popup dismissal through the same draft guard.
class DraftDialog:public QDialog {
public:
    using QDialog::QDialog;
    std::function<bool(int)> canClose;
    void done(int result) override {
        if(deciding)return;
        QScopedValueRollback<bool> guard(deciding,true);
        if((property("outsideDismiss").toBool()&&objectName()!="noteSecretEditor")||!canClose||canClose(result))QDialog::done(result);
    }
private:
    bool deciding=false;
};

QString passwordInput(QWidget *parent,const QString &title,const QString &label,bool *ok){
    QInputDialog dialog(parent);dialog.setObjectName("notePasswordDialog");dialog.setWindowTitle(title);dialog.setStyleSheet(NoteStyle::sheet);
    dialog.setInputMode(QInputDialog::TextInput);dialog.setTextEchoMode(QLineEdit::Password);dialog.setLabelText(label);dialog.setOption(QInputDialog::NoButtons);
    auto buttons=new QDialogButtonBox;auto accept=new NoteStyle::Button("确认"),cancel=new NoteStyle::Button("取消");
    buttons->addButton(accept,QDialogButtonBox::AcceptRole);buttons->addButton(cancel,QDialogButtonBox::RejectRole);dialog.layout()->addWidget(buttons);
    accept->setDefault(true);cancel->setAutoDefault(false);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    for(auto input:dialog.findChildren<QLineEdit*>())QObject::connect(input,&QLineEdit::returnPressed,&dialog,&QDialog::accept);
    for(auto caption:dialog.findChildren<QLabel*>())caption->setWordWrap(true);
    NoteStyle::placeDialog(dialog,parent,QSize(480,220),false);
    QTimer::singleShot(0,&dialog,[&]{NoteStyle::placeDialog(dialog,parent,QSize(480,220),false);});
    *ok=PopupPolicy::exec(dialog)==QDialog::Accepted;auto value=dialog.textValue();dialog.setTextValue({});return value;
}

void addDialogFields(QVBoxLayout *layout,NoteFields *fields){
    auto scroll=new QScrollArea;scroll->setObjectName("noteDialogScroll");scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumSize(0,0);scroll->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);scroll->setWidget(fields);layout->addWidget(scroll,1);
}

void addDialogHeader(QVBoxLayout *layout,QDialog *dialog){
    auto row=new QHBoxLayout;auto heading=new QLabel(dialog->windowTitle());heading->setObjectName("noteDialogHeading");
    auto close=new NoteStyle::Button("×",true);close->setObjectName("popupClose");close->setAccessibleName("关闭");close->setFixedWidth(36);
    row->addWidget(heading,1);row->addWidget(close);layout->addLayout(row);QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::reject);
}

}
NoteWidget::NoteWidget(NoteDepartment *department,QWidget *parent):QWidget(parent),dept(department){
    setAttribute(Qt::WA_StyledBackground,true);setObjectName("notePage");setStyleSheet(NoteStyle::sheet);setFocusPolicy(Qt::StrongFocus);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(28,22,28,20);layout->setSpacing(18);
    addPageHeader(layout,"Note");
    auto bar=new QHBoxLayout;tabs=new QTabBar;tabs->setObjectName("noteTabs");tabs->setExpanding(false);for(const auto &name:{"纸条","书签","秘密"})tabs->addTab(name);
    filterButton=new NoteStyle::Button("启用筛选",true);filterButton->setObjectName("noteFilter");
    add=new NoteStyle::Button("新增纸条");add->setObjectName("noteAdd");
    bar->addWidget(tabs);bar->addStretch();bar->addWidget(add);layout->addLayout(bar);
    auto filters=new QHBoxLayout;filters->addWidget(filterButton);filters->addStretch();layout->addLayout(filters);
    notice=new QLabel;notice->setObjectName("noteMigrationNotice");notice->setWordWrap(true);notice->setProperty("role","muted");layout->addWidget(notice);
    scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setObjectName("noteScroll");
    canvas=new QWidget;canvas->setObjectName("noteCanvas");canvas->setAcceptDrops(true);canvas->installEventFilter(this);scroll->setWidget(canvas);scroll->viewport()->installEventFilter(this);layout->addWidget(scroll,1);
    emptyGroup=new QWidget(canvas);auto emptyLayout=new QVBoxLayout(emptyGroup);emptyLayout->setContentsMargins(16,26,16,26);emptyLayout->setSpacing(18);
    empty=new QLabel;empty->setObjectName("noteEmpty");empty->setAlignment(Qt::AlignCenter);empty->setWordWrap(true);emptyLayout->addWidget(empty);
    emptyAction=new NoteStyle::Button("新增纸条");emptyAction->setObjectName("noteEmptyAction");emptyLayout->addWidget(emptyAction,0,Qt::AlignHCenter);
    connect(filterButton,&QPushButton::clicked,this,&NoteWidget::toggleFilter);
    connect(emptyAction,&QPushButton::clicked,this,[this]{if(filter.active()){filter={};filterCards();}else edit();});
    connect(add,&QPushButton::clicked,this,[this]{edit();});
    connect(tabs,&QTabBar::currentChanged,this,[this](int index){
        if(!prepareLeave()){QSignalBlocker blocker(tabs);tabs->setCurrentIndex(activeTab);return;}
        activeTab=index;filter={};appliedFilter={};add->setText(index==0?"新增纸条":index==1?"新增书签":"新增秘密");
        refresh();
    });
    connect(dept,&NoteDepartment::changed,this,[this]{QTimer::singleShot(0,this,&NoteWidget::refresh);});
    connect(ConfigManager::getInstance(),&ConfigManager::noteTagsChanged,this,&NoteWidget::refresh);
    auto create=new QShortcut(QKeySequence("Ctrl+N"),this);create->setContext(Qt::WidgetWithChildrenShortcut);connect(create,&QShortcut::activated,this,[this]{edit();});
    auto find=new QShortcut(QKeySequence::Find,this);find->setContext(Qt::WidgetWithChildrenShortcut);connect(find,&QShortcut::activated,this,&NoteWidget::openFilter);
    longPress.setSingleShot(true);longPress.setInterval(450);connect(&longPress,&QTimer::timeout,this,&NoteWidget::startCardDrag);
    dropMarker=new QFrame(canvas);dropMarker->setStyleSheet("background:#3caa70;");dropMarker->hide();
    qApp->installEventFilter(this);refresh();
}
bool NoteWidget::ensureUnlocked() {
    if(dept->unlocked()) return true;
    bool ok=false;QString password=passwordInput(this,dept->exists()?"解锁私人信息":"首次创建加密密库",
        dept->exists()?"请输入主密码":"设置至少10字符的主密码；遗忘无法找回",&ok);
    if(!ok){password.fill(QChar(0));return false;}
    bool result;
    if(!dept->exists()) {
        auto confirm=passwordInput(this,"确认主密码","再次输入",&ok);const bool matches=confirm==password;confirm.fill(QChar(0));
        if(!ok||!matches) {password.fill(QChar(0));QMessageBox::warning(this,"未创建","两次密码不一致");return false;}
        result=dept->create(password);
    } else result=dept->unlock(password);
    password.fill(QChar(0));
    if(!result) QMessageBox::warning(this,"无法解锁",dept->error());return result;
}

bool NoteWidget::prepareLeave(){
    for(auto card:ordered){
        if(!card->save()){scroll->ensureWidgetVisible(card);return false;}
    }
    for(auto card:ordered){card->collapse();card->select(false);}relayout();return true;
}
void NoteWidget::refresh(){
    if(refreshing)return;refreshing=true;
    auto records=dept->catalog();if(dept->needsMigration())records.append(QJsonObject{{"id","legacy-vault"},{"kind",0},{"title","旧版密库：双击解锁迁移"},{"legacy",true}});
    QSet<QString> seen;ordered.clear();
    for(auto value:records){
        const auto e=value.toObject();if(e["deleted"].toBool())continue;const auto id=e["id"].toString();seen.insert(id);
        auto card=cards.value(id);
        if(!card){
            card=new NoteCard(e,dept,canvas);cards.insert(id,card);
            connect(card,&NoteCard::saveFailed,this,&NoteWidget::saveFailed);
            connect(card,&NoteCard::selected,this,[this](NoteCard *chosen,bool toggle){const bool selected=!toggle||!chosen->property("selected").toBool();for(auto c:cards)c->select(c==chosen&&selected);});
            connect(card,&NoteCard::expansionRequested,this,&NoteWidget::expand);
            connect(card,&NoteCard::collapseRequested,this,[this](NoteCard *c){if(c->save()){c->collapse();relayout();}});
            connect(card,&NoteCard::deleteRequested,this,[this](NoteCard *c){
                if(QMessageBox::question(this,"永久删除","删除「"+c->entry()["title"].toString()+"」？此操作无法撤销。")!=QMessageBox::Yes)return;
                if(!dept->remove(c->id()))QMessageBox::warning(this,"删除失败",dept->error());else refresh();
            });
            connect(card,&NoteCard::locateRequested,this,[this](NoteCard *c){QString error;if(!BookmarkTarget::open(c->entry(),&error,true))QMessageBox::warning(this,"无法定位书签",error);});
            connect(card,&NoteCard::moveRequested,this,[this](NoteCard *c,int direction){
                QList<NoteCard*> visible;for(auto item:ordered)if(!item->isHidden())visible.append(item);
                const int at=visible.indexOf(c),next=at+direction;if(at>=0&&next>=0&&next<visible.size())moveCard(c->id(),visible[next]->id());
            });
            connect(card,&NoteCard::activated,this,[this](NoteCard *c){
                if(c->entry()["kind"].toInt()==0)secretEditor(c->entry());
                else{QString error;if(!BookmarkTarget::open(c->entry(),&error))QMessageBox::warning(this,"无法打开书签",error);}
            });
        }else if(!card->expanded())card->updateRecord(e);
        ordered.append(card);
    }
    for(auto it=cards.begin();it!=cards.end();){if(!seen.contains(it.key())){it.value()->hide();it.value()->deleteLater();it=cards.erase(it);}else ++it;}
    std::stable_sort(ordered.begin(),ordered.end(),[](NoteCard *a,NoteCard *b){return a->entry()["createdAt"].toString()>b->entry()["createdAt"].toString();});
    for(int kind=0;kind<3;++kind){
        const auto ids=dept->order(kind);QList<NoteCard*> group;QList<int> positions;
        for(int i=0;i<ordered.size();++i)if(ordered[i]->entry()["kind"].toInt()==kind){positions.append(i);group.append(ordered[i]);}
        std::stable_sort(group.begin(),group.end(),[&](NoteCard *a,NoteCard *b){
            const int x=ids.indexOf(a->id()),y=ids.indexOf(b->id());
            return x<0?false:y<0?true:x<y;
        });
        for(int i=0;i<positions.size();++i)ordered[positions[i]]=group[i];
    }
    notice->setText(dept->migrationNotice().isEmpty()?dept->error():dept->migrationNotice());notice->setVisible(!notice->text().isEmpty());
    refreshing=false;filterCards();
}
bool NoteWidget::filterCards(){
    if(refreshing)return true;
    const int kind=activeTab==0?2:activeTab==1?1:0;
    QList<QPair<NoteCard*,bool>> matches;
    for(auto c:ordered){
        auto e=c->draft();
        bool match=e["kind"].toInt()==kind&&filter.matchesTitle(e["title"].toString())&&(kind==0||filter.matchesTags(e["tags"].toString()))&&(kind!=1||filter.bookmarkType.isEmpty()||filter.bookmarkType==e["targetType"].toString());
        if(!match&&c->expanded()){
            if(!c->save()){
                filter=appliedFilter;return false;
            }
            c->collapse();
        }
        matches.append({c,match});
    }
    appliedFilter=filter;filterButton->setText(filter.active()?"关闭筛选":"启用筛选");for(auto pair:matches)pair.first->setVisible(pair.second);relayout();return true;
}
void NoteWidget::relayout(){
    // Monotonic cursor preserves order and leaves holes before a moved large card.
    // Occupancy lets subsequent small cards fill the large card's right side.
    constexpr int preferred=200,gap=16;const int width=qMax(1,scroll->viewport()->width());
    const int columns=qMax(1,(width+gap)/(preferred+gap));const int cell=qMax(1,(width-gap*(columns-1))/columns);
    QSet<quint64> occupied;auto key=[](int r,int c){return (quint64(r)<<32)|quint32(c);};
    int cursor=0,bottom=0,count=0;
    for(auto card:ordered){
        if(card->isHidden())continue;++count;
        int span=card->expanded()?qMin(3,columns):1,heightSpan=card->expanded()?3:1;
        int row=0,column=0;
        for(;;++cursor){
            row=cursor/columns;column=cursor%columns;if(column+span>columns)continue;bool free=true;
            for(int r=row;r<row+heightSpan;++r)for(int c=column;c<column+span;++c)if(occupied.contains(key(r,c)))free=false;
            if(free)break;
        }
        for(int r=row;r<row+heightSpan;++r)for(int c=column;c<column+span;++c)occupied.insert(key(r,c));
        const int cardWidth=card->expanded()&&columns<3?width:span*cell+(span-1)*gap;
        card->setGeometry(column*(cell+gap),row*(cell+gap),cardWidth,heightSpan*cell+(heightSpan-1)*gap);
        bottom=qMax(bottom,card->geometry().bottom()+1);++cursor;
    }
    const bool filtered=filter.active();
    empty->setText(filtered?"没有匹配的条目，试试其他关键词或标签。":activeTab==0?"记下第一个值得留住的片段。":activeTab==1?"把常用网站、文件和文件夹收在这里。":dept->creationReady()?"秘密正文加密保存，每次查看时验证主密码。":"还没有秘密，请先到设置 → Note 初始化秘密加密。");
    emptyAction->setText(filtered?"清除筛选":activeTab==0?"新增纸条":activeTab==1?"新增书签":"新增秘密");
    emptyAction->setVisible(filtered||activeTab!=2||dept->creationReady());
    emptyGroup->setVisible(count==0);emptyGroup->setGeometry(0,24,width,qMax(150,emptyGroup->sizeHint().height()));canvas->setMinimumHeight(qMax(bottom+16,220));
}
bool NoteWidget::eventFilter(QObject *object,QEvent *event){
    if(object==canvas&&(event->type()==QEvent::DragEnter||event->type()==QEvent::DragMove||event->type()==QEvent::Drop)){
        auto drop=static_cast<QDropEvent*>(event);
        if(!drop->mimeData()->hasFormat("application/x-ztl-note"))return false;
        const auto source=QString::fromUtf8(drop->mimeData()->data("application/x-ztl-note"));
        auto target=dropTarget(drop->position().toPoint());
        if(!cards.contains(source)||!target){drop->ignore();return true;}
        if(event->type()==QEvent::Drop){dropMarker->hide();moveCard(source,target->id());}
        else{const auto r=target->geometry();dropMarker->setGeometry(r.left(),r.top(),r.width(),3);dropMarker->raise();dropMarker->show();
            const auto point=canvas->mapTo(scroll->viewport(),drop->position().toPoint());
            if(point.y()<40)scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->value()-24);
            if(point.y()>scroll->viewport()->height()-40)scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->value()+24);}
        drop->acceptProposedAction();return true;
    }
    if(object==canvas&&event->type()==QEvent::DragLeave)dropMarker->hide();
    if(event->type()==QEvent::MouseButtonRelease||event->type()==QEvent::ApplicationDeactivate){longPress.stop();dragCandidate=nullptr;}
    if(event->type()==QEvent::MouseMove&&dragCandidate){
        auto mouse=static_cast<QMouseEvent*>(event);if((mouse->globalPosition().toPoint()-dragPress).manhattanLength()>QApplication::startDragDistance()){longPress.stop();dragCandidate=nullptr;}
    }

    if(event->type()==QEvent::MouseButtonPress){
        if(auto widget=qobject_cast<QWidget*>(object)){
            NoteCard *clicked=nullptr;for(auto ancestor=widget;ancestor;ancestor=ancestor->parentWidget())if(auto c=qobject_cast<NoteCard*>(ancestor)){if(cards.value(c->id())==c)clicked=c;break;}
            if(clicked&&static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton){
                const auto point=clicked->mapFromGlobal(static_cast<QMouseEvent*>(event)->globalPosition().toPoint());
                bool editable=qobject_cast<QLineEdit*>(widget)||qobject_cast<QTextEdit*>(widget)||qobject_cast<QAbstractButton*>(widget)||qobject_cast<QAbstractScrollArea*>(widget);
                if(!editable&&(!clicked->expanded()||point.y()<64||widget->objectName()=="noteDragHandle")){
                    dragCandidate=clicked;dragPress=static_cast<QMouseEvent*>(event)->globalPosition().toPoint();longPress.start();
                }
            }
            if(!clicked){for(auto c:cards)c->select(false);}
            else if(widget!=clicked&&(qobject_cast<QAbstractButton*>(widget)||widget->focusPolicy()!=Qt::NoFocus)){for(auto c:cards)c->select(c==clicked);}
        }
    }
    if(event->type()==QEvent::ApplicationDeactivate)for(auto c:cards)c->select(false);
    if(object==scroll->viewport()&&event->type()==QEvent::Resize)QTimer::singleShot(0,this,&NoteWidget::relayout);
    return QWidget::eventFilter(object,event);
}
void NoteWidget::showEvent(QShowEvent *event){QWidget::showEvent(event);setFocus(Qt::OtherFocusReason);}
void NoteWidget::expand(NoteCard *card){
    card->expand();relayout();
    QPointer<NoteCard> target=card;
    QTimer::singleShot(0,this,[this,target]{if(!target||!target->expanded())return;scroll->verticalScrollBar()->setValue(qMax(0,target->y()-16));});
}
void NoteWidget::addDraft(const QString &title,const QString &body){
    if(!prepareLeave())return;tabs->setCurrentIndex(0);edit({{"kind",2},{"title",title},{"body",body}});
}
void NoteWidget::edit(QJsonObject e){
    for(auto dialog:findChildren<QDialog*>("noteEditor"))if(dialog->isVisible())return;
    const int kind=activeTab==0?2:activeTab==1?1:0;
    if(kind==0&&!dept->creationReady()){QMessageBox::information(this,"先初始化秘密加密","请到设置 → Note → 初始化。完成一次设置后，新增无需密码，查看时才验证。");return;}
    for(auto card:ordered)if(!card->save())return;
    e["kind"]=kind;e["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(kind==1){e["targetType"]="website";e["target"]="";}
    const QString draftKey="note/new/"+QString::number(kind);const auto recovery=DraftStore::read(draftKey);
    if(!recovery.isEmpty())e=recovery;
    DraftDialog dialog(this,Qt::Dialog|Qt::FramelessWindowHint);dialog.setObjectName("noteEditor");dialog.setWindowTitle("新增"+QString(kind==2?"纸条":kind==1?"书签":"秘密"));dialog.setStyleSheet(NoteStyle::sheet);
    auto layout=new QVBoxLayout(&dialog);layout->setContentsMargins(22,18,22,20);layout->setSpacing(18);addDialogHeader(layout,&dialog);
    NoteFields fields(e);fields.setCreationMode();addDialogFields(layout,&fields);
    auto create=new NoteStyle::Button("创建");create->setObjectName("noteCreate");layout->addWidget(create,0,Qt::AlignRight);
    bool failed=false;
    dialog.canClose=[&](int result){if(result==QDialog::Accepted||(!failed&&!fields.dirty()))return true;return QMessageBox::question(&dialog,"放弃新增","条目尚未保存，放弃这些输入？")==QMessageBox::Yes;};
    connect(create,&QPushButton::clicked,&dialog,[&]{if(dept->save(fields.values()))dialog.accept();else{failed=true;fields.setError(dept->error());}});
    NoteStyle::placeDialog(dialog,this,QSize(550,kind==0?480:620),true);
    if(PopupPolicy::exec(dialog)==QDialog::Accepted){
        DraftStore::clear(draftKey);
        filter={};refresh();
    }else if(dialog.property("outsideDismiss").toBool()){
        if(!DraftStore::save(draftKey,fields.values()))emit saveFailed("新增 Note 草稿保存失败");
    }else DraftStore::clear(draftKey);
}
void NoteWidget::secretEditor(QJsonObject e){
    dept->lock();if(!ensureUnlocked())return;
    if(e.value("legacy").toBool()){dept->lock();refresh();return;}
    e=dept->entry(e.value("id").toString());if(e.isEmpty()){dept->lock();return;}
    DraftDialog dialog(this);dialog.setObjectName("noteSecretEditor");dialog.setWindowTitle("秘密");dialog.setStyleSheet(NoteStyle::sheet);
    auto layout=new QVBoxLayout(&dialog);layout->setContentsMargins(22,18,22,20);layout->setSpacing(18);addDialogHeader(layout,&dialog);NoteFields fields(e);addDialogFields(layout,&fields);
    const auto recovered=dept->recoveryDraft(e["id"].toString());
    bool hasRecovery=!recovered.isEmpty();
    if(hasRecovery){
        QSignalBlocker a(fields.title),b(fields.body);
        fields.title->setText(recovered["title"].toString());fields.body->setPlainText(recovered["body"].toString());
        fields.findChild<QLabel*>("noteSaveStatus")->setText("已恢复未保存的输入");
    }else if(!dept->error().isEmpty())fields.setError(dept->error());
    auto buttons=new QHBoxLayout;auto copy=new NoteStyle::Button("复制秘密",true),remove=new NoteStyle::Button("删除",true),close=new NoteStyle::Button("关闭",true);remove->setProperty("danger",true);
    buttons->addWidget(copy);buttons->addStretch();buttons->addWidget(remove);buttons->addWidget(close);layout->addLayout(buttons);
    bool forcedClose=false;
    auto commit=[&]{if(!fields.dirty()&&!hasRecovery){fields.committed(dept->entry(e.value("id").toString()));return true;}if(!dept->save(fields.values())){fields.setError(dept->error());return false;}hasRecovery=false;fields.committed(dept->entry(e.value("id").toString()));return true;};
    dialog.canClose=[&](int){if(forcedClose||commit())return true;if(dialog.property("outsideDismiss").toBool()){const bool durable=dept->preserveDraft(fields.values());emit saveFailed(durable?"已封存秘密恢复草稿":dept->error());return true;}return false;};
    connect(&fields,&NoteFields::saveRequested,&dialog,[&]{commit();});
    connect(close,&QPushButton::clicked,&dialog,&QDialog::reject);
    connect(copy,&QPushButton::clicked,&dialog,[&]{dept->copySecret(fields.body->toPlainText());});
    connect(remove,&QPushButton::clicked,&dialog,[&]{
        if(QMessageBox::question(&dialog,"永久删除","删除这个秘密？此操作无法撤销。")!=QMessageBox::Yes)return;
        if(dept->remove(e.value("id").toString())){forcedClose=true;dialog.accept();}else fields.setError(dept->error());
    });
    connect(dept,&NoteDepartment::aboutToLock,&dialog,[&]{
        if(!commit()){
            const bool durable=dept->preserveDraft(fields.values());
            emit saveFailed(durable?"秘密未正式保存，已封存加密恢复草稿；下次验证密码后恢复输入。":dept->error());
        }
        forcedClose=true;
    });
    connect(dept,&NoteDepartment::locked,&dialog,&QDialog::reject);
    NoteStyle::placeDialog(dialog,this,QSize(560,520),false);PopupPolicy::exec(dialog);disconnect(dept,nullptr,&dialog,nullptr);disconnect(&fields,nullptr,&dialog,nullptr);fields.body->clear();fields.title->clear();e={};dept->lock(false);
}
void NoteWidget::manage(const QString &action){
    const auto name=action=="密码设置"?(dept->exists()?QString("修改密码"):QString("初始化")):action;
    if(name=="初始化"){
        dept->lock();if(!ensureUnlocked())return;dept->lock();QMessageBox::information(this,"秘密加密已就绪","此后新增秘密无需密码；每次打开已有秘密仍需验证主密码。");return;
    }
    if(name=="备份"){
        auto path=QFileDialog::getSaveFileName(this,"导出秘密备份",Paths::data("backups/secrets.ztlvault"),"ZTL 密库 (*.ztlvault)");
        if(!path.isEmpty()&&!dept->backup(path))QMessageBox::warning(this,"备份失败",dept->error());return;
    }
    if(name=="恢复备份"){
        const auto path=QFileDialog::getOpenFileName(this,"选择秘密备份",Paths::data("backups"),"ZTL 密库 (*.ztlvault);;全部 (*)");if(path.isEmpty())return;
        if(QMessageBox::question(this,"恢复","替换现有秘密？普通纸条和书签保留。")!=QMessageBox::Yes)return;
        bool ok=false;auto password=passwordInput(this,"备份密码","请输入备份密码",&ok);
        if(ok&&!dept->restore(path,password))QMessageBox::warning(this,"恢复失败",dept->error());password.fill(QChar(0));dept->lock();return;
    }
    if(name=="修改密码"){
        dept->lock();if(!ensureUnlocked())return;
        bool ok=false;auto password=passwordInput(this,"新密码","至少10字符",&ok);
        while(ok){
            auto confirm=passwordInput(this,"确认密码","再次输入",&ok);const bool matches=confirm==password;confirm.fill(QChar(0));
            if(!ok)break;
            if(!matches){QMessageBox::warning(this,"密码不一致","两次输入的密码不一致，密码未修改。请重新确认。");continue;}
            if(!dept->changePassword(password))QMessageBox::warning(this,"修改失败",dept->error());
            break;
        }
        password.fill(QChar(0));dept->lock();
    }
}

NoteCard *NoteWidget::dropTarget(const QPoint &point) const {
    NoteCard *best=nullptr;int distance=std::numeric_limits<int>::max();
    for(auto c:ordered)if(!c->isHidden()){
        if(c->geometry().contains(point))return c;
        const int d=(c->geometry().center()-point).manhattanLength();if(d<distance){best=c;distance=d;}
    }return best;
}
void NoteWidget::startCardDrag(){
    auto card=dragCandidate;dragCandidate=nullptr;if(!card||!card->isVisible()||!card->save())return;
    card->setDragReady(true);card->repaint();
    QDrag drag(this);auto mime=new QMimeData;mime->setData("application/x-ztl-note",card->id().toUtf8());drag.setMimeData(mime);
    drag.setPixmap(card->grab().scaled(180,180,Qt::KeepAspectRatio,Qt::SmoothTransformation));drag.exec(Qt::MoveAction);if(card)card->setDragReady(false);dropMarker->hide();
}
bool NoteWidget::moveCard(const QString &source,const QString &target){
    auto a=cards.value(source),b=cards.value(target);if(!a||!b||a==b||a->entry()["kind"]!=b->entry()["kind"])return false;
    const int kind=a->entry()["kind"].toInt();QStringList full,visible;QList<int> visibleSlots;
    for(auto c:ordered)if(c->entry()["kind"].toInt()==kind){if(!c->isHidden()){visibleSlots.append(full.size());visible.append(c->id());}full.append(c->id());}
    const int from=visible.indexOf(source),to=visible.indexOf(target);if(from<0||to<0)return false;visible.move(from,to);
    for(int i=0;i<visibleSlots.size();++i)full[visibleSlots[i]]=visible[i];
    if(!dept->reorder(kind,full)){emit saveFailed(dept->error());return false;}refresh();return true;
}

void NoteWidget::toggleFilter(){
    if(filter.active()){filter={};filterCards();return;}
    openFilter();
}
void NoteWidget::openFilter(){
    AdvancedFilter next;
    const auto scope=activeTab==0?AdvancedFilterDialog::Paper:activeTab==1?AdvancedFilterDialog::Bookmark:AdvancedFilterDialog::Secret;
    if(AdvancedFilterDialog::open(scope,filterButton,next)){filter=next;filterCards();}
}
