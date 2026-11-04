#include "advancedfilter.h"
#include "deadlineeditor.h"
#include "settingcontrols.h"
#include "uistyle.h"
#include "dialoglayout.h"
#include "popuppolicy.h"
#include <QtWidgets>
class AdvancedFilterDialog::Inputs:public QObject {
public:
    using QObject::QObject;
    QLineEdit *words=nullptr;
    DeadlineTagPicker *tags=nullptr;
    QComboBox *star=nullptr,*read=nullptr,*type=nullptr;
    QListWidget *accounts=nullptr;
    QCheckBox *dates=nullptr,*ddl=nullptr,*intervals=nullptr;
    DeadlineDateTimeInput *from=nullptr,*to=nullptr;
};
AdvancedFilterDialog::AdvancedFilterDialog(Scope scope,const QStringList &accounts,QWidget *parent)
    :QDialog(parent,Qt::Tool|Qt::FramelessWindowHint),inputs(new Inputs(this)){
    setObjectName("advancedFilter");setWindowTitle("高级筛选");
    SettingControls::applyTheme(this,QColor(scope==Mail?"#754218":scope==Calendar?"#7f2636":"#176a39"));
    setStyleSheet(UiStyle::sheet+styleSheet());
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(18,14,18,18);layout->setSpacing(12);
    auto header=new QHBoxLayout;auto close=new SettingButton("×");close->setObjectName("popupClose");close->setFixedWidth(36);
    header->addWidget(close);header->addWidget(new QLabel("高级筛选"),1);layout->addLayout(header);
    connect(close,&QPushButton::clicked,this,&QDialog::reject);
    auto form=new QFormLayout;form->setVerticalSpacing(10);layout->addLayout(form);
    if(scope==Calendar){auto classes=new QWidget;auto row=new QHBoxLayout(classes);row->setContentsMargins(0,0,0,0);
        inputs->ddl=new QCheckBox("DDL");inputs->ddl->setObjectName("filterDDL");inputs->ddl->setChecked(true);inputs->intervals=new QCheckBox("事务");inputs->intervals->setObjectName("filterIntervals");inputs->intervals->setChecked(true);row->addWidget(inputs->ddl);row->addWidget(inputs->intervals);row->addStretch();form->addRow("筛选对象",classes);}
    inputs->words=new QLineEdit;inputs->words->setObjectName("filterKeywords");inputs->words->setPlaceholderText("多个关键词用空格分隔，全部匹配标题");form->addRow("关键词",inputs->words);
    if(scope!=Mail&&scope!=Secret){inputs->tags=new DeadlineTagPicker({},nullptr,scope!=Calendar);form->addRow("标签",inputs->tags);}
    auto tri=[&](const QString &name,const QString &yes,const QString &no){auto c=new NoWheelComboBox;c->setObjectName(name);c->addItem("不限",-1);c->addItem(yes,1);c->addItem(no,0);return c;};
    if(scope==Bookmark){inputs->type=new NoWheelComboBox;inputs->type->setObjectName("filterBookmarkType");inputs->type->addItem("不限",QString());inputs->type->addItem("网站","website");inputs->type->addItem("文件","file");inputs->type->addItem("文件夹","folder");form->addRow("类型",inputs->type);}
    if(scope==Mail){
        inputs->accounts=new QListWidget;inputs->accounts->setObjectName("filterAccounts");inputs->accounts->setMaximumHeight(104);
        for(const auto &address:accounts){auto item=new QListWidgetItem(address,inputs->accounts);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Unchecked);}
        form->addRow("收信邮箱账号",inputs->accounts);
        inputs->read=tri("filterRead","已读","未读");form->addRow("状态",inputs->read);
    }
    if(scope==Calendar||scope==Mail){
        inputs->star=tri("filterStar",scope==Mail?"已收藏":"标星",scope==Mail?"未收藏":"不标星");form->addRow(scope==Mail?"是否收藏":"是否标星",inputs->star);
        inputs->dates=new QCheckBox("限制时间范围");inputs->dates->setObjectName("filterLimitDates");form->addRow(inputs->dates);
        inputs->from=new DeadlineDateTimeInput(QDateTime(QDate::currentDate(),QTime(0,0)),nullptr,true);inputs->from->setObjectName("filterFrom");
        inputs->to=new DeadlineDateTimeInput(QDateTime(QDate::currentDate().addYears(100),QTime(0,0)),nullptr,true);inputs->to->setObjectName("filterTo");
        form->addRow("起始日期",inputs->from);form->addRow("结束日期",inputs->to);
        inputs->from->setEnabled(false);inputs->to->setEnabled(false);
        connect(inputs->dates,&QCheckBox::toggled,inputs->from,&QWidget::setEnabled);connect(inputs->dates,&QCheckBox::toggled,inputs->to,&QWidget::setEnabled);
    }
    auto feedback=new QLabel;feedback->setObjectName("filterError");feedback->setWordWrap(true);layout->addWidget(feedback);
    auto apply=new SettingButton("筛选");apply->setObjectName("filterApply");layout->addWidget(apply);
    connect(apply,&QPushButton::clicked,this,[this,feedback]{const auto f=criteria();if(!f.calendarDDL&&!f.calendarIntervals){feedback->setText("请至少选择一种筛选对象");return;}if(f.limitDates&&f.from>f.to){feedback->setText("结束日期不能早于起始日期");return;}accept();});
    resize(620,sizeHint().height());
}
AdvancedFilter AdvancedFilterDialog::criteria() const {
    AdvancedFilter f;if(inputs->ddl){f.calendarDDL=inputs->ddl->isChecked();f.calendarIntervals=inputs->intervals->isChecked();}f.keywords=inputs->words->text().trimmed();
    if(inputs->tags)f.tags=inputs->tags->selectedTags().split(',',Qt::SkipEmptyParts);
    if(inputs->type)f.bookmarkType=inputs->type->currentData().toString();
    if(inputs->star)f.starred=inputs->star->currentData().toInt();
    if(inputs->read)f.read=inputs->read->currentData().toInt();
    if(inputs->accounts)for(int i=0;i<inputs->accounts->count();++i)if(inputs->accounts->item(i)->checkState()==Qt::Checked)f.accounts.append(inputs->accounts->item(i)->text());
    if(inputs->dates){f.limitDates=inputs->dates->isChecked();f.from=inputs->from->dateTime().date();f.to=inputs->to->dateTime().date();}
    return f;
}
bool AdvancedFilterDialog::open(Scope scope,QPushButton *anchor,AdvancedFilter &result,const QStringList &accounts){
    for(auto popup:anchor->window()->findChildren<QDialog*>("advancedFilter"))if(popup->isVisible())return false;
    AdvancedFilterDialog dialog(scope,accounts,anchor->parentWidget());
    const auto bounds=DialogLayout::fit(dialog,anchor,dialog.sizeHint().expandedTo(QSize(600,0)));
    dialog.move(DialogLayout::clamp(anchor->mapToGlobal(QPoint(0,anchor->height()+6)),dialog.size(),bounds));
    if(PopupPolicy::exec(dialog)!=QDialog::Accepted)return false;
    result=dialog.criteria();return true;
}
