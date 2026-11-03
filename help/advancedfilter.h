#pragma once
#include <QtCore>
#include <QtWidgets/QDialog>
struct AdvancedFilter {
    QString keywords;
    QStringList tags,accounts;
    QString bookmarkType;
    int starred=-1,read=-1;
    bool limitDates=false;
    QDate from=QDate::currentDate(),to=from.addYears(100);
    QStringList words() const {return keywords.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);}
    bool active() const {return !words().isEmpty()||!tags.isEmpty()||!accounts.isEmpty()||!bookmarkType.isEmpty()||starred>=0||read>=0||limitDates;}
    bool matchesTitle(const QString &title) const {for(const auto &word:words())if(!title.contains(word,Qt::CaseInsensitive))return false;return true;}
    bool matchesTags(const QString &value) const {const auto all=value.split(',',Qt::SkipEmptyParts);for(const auto &tag:tags)if(!all.contains(tag))return false;return true;}
    bool matchesDate(const QDateTime &date) const {return !limitDates||(date.isValid()&&date.toLocalTime().date()>=from&&date.toLocalTime().date()<=to);}
};
class QPushButton;
class AdvancedFilterDialog:public QDialog {
public:
    enum Scope {Calendar,Paper,Bookmark,Secret,Mail};
    AdvancedFilterDialog(Scope scope,const QStringList &accounts,QWidget *parent);
    AdvancedFilter criteria() const;
    static bool open(Scope scope,QPushButton *anchor,AdvancedFilter &result,const QStringList &accounts={});
private:
    class Inputs;
    Inputs *inputs;
};
