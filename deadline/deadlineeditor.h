#pragma once
#include <QtWidgets>
class DeadlineTagPicker:public QWidget {
    Q_OBJECT
public:
    explicit DeadlineTagPicker(const QString &tags={},QWidget *parent=nullptr,bool noteScope=false);
    QString selectedTags() const {return chosen.join(',');}
    int selectedCount() const {return chosen.size();}
signals:
    void selectionChanged();
private:
    void refresh();
    QStringList chosen,originalTags;
    bool noteScope=false;
    QListWidget *selected,*available;
    QLabel *summary;
};
class DeadlineDateTimeInput:public QWidget {
    Q_OBJECT
public:
    explicit DeadlineDateTimeInput(const QDateTime &value,QWidget *parent=nullptr,bool dateOnly=false);
    static QDateTime roundUp(const QDateTime &value);
    QDateTime dateTime() const;
    void setDate(const QDate &value);
    void setTime(const QTime &value);
    void setTimeOnly(bool enabled);
signals:
    void changed();
private:
    QSpinBox *year,*month,*day,*hour,*minute;
    int intendedDay=1,seconds=0,millis=0;
    bool adjusting=false;
};
class DeadlineEditor:public QDialog {
    Q_OBJECT
public:
    explicit DeadlineEditor(const QJsonObject &task,const QDateTime &due,bool editing,bool onlyThis,QWidget *parent=nullptr);
    QJsonObject values() const;
    void setError(const QString &message);
signals:
    void submitted(const QJsonObject &values);
protected:
    void done(int result) override;
private:
    QJsonObject original,initialValues;
    QLineEdit *title;
    QTextEdit *note;
    DeadlineTagPicker *tags;
    DeadlineDateTimeInput *due,*start,*labelDate;
    QComboBox *repeat,*star,*remind,*type;
    QCheckBox *repeatEnabled;
    QSpinBox *remainingCount;
    QWidget *weekdays,*monthdays;
    QLabel *feedback;
    bool deciding=false;
    bool endTimeEdited=false,adjustingInterval=false;
    qint64 intervalDuration=3600;
};
