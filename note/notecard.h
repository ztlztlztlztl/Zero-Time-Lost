#pragma once
#include <QtWidgets>
#include "notedepartment.h"
#include "deadlineeditor.h"

// Common inputs for the creation popup and independent inline editors.
class NoteFields:public QWidget {
    Q_OBJECT
public:
    explicit NoteFields(const QJsonObject &entry,QWidget *parent=nullptr);
    QJsonObject values() const;
    bool dirty() const;
    void committed(const QJsonObject &entry);
    void setError(const QString &message);
    void setCreationMode();
    QLineEdit *title;
    QTextEdit *body=nullptr;
    DeadlineTagPicker *tags=nullptr;
    QComboBox *targetType=nullptr;
    QLineEdit *target=nullptr;
signals:
    void saveRequested();
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    QJsonObject original;
    QLabel *feedback,*status;
    QTimer autosave;
};

class NoteCard:public QFrame {
    Q_OBJECT
public:
    explicit NoteCard(const QJsonObject &entry,NoteDepartment *department,QWidget *parent=nullptr);
    QString id() const {return record["id"].toString();}
    QJsonObject entry() const {return record;}
    QJsonObject draft() const {return editor?editor->values():record;}
    bool expanded() const {return editor!=nullptr;}
    void updateRecord(const QJsonObject &entry);
    void expand();
    bool save();
    void collapse();
    void select(bool selected);
signals:
    void selected(NoteCard *card,bool toggle);
    void expansionRequested(NoteCard *card);
    void collapseRequested(NoteCard *card);
    void deleteRequested(NoteCard *card);
    void activated(NoteCard *card);
    void saveFailed(const QString &message);
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
private:
    NoteDepartment *dept;
    QJsonObject record;
    QVBoxLayout *layout;
    QWidget *small,*large=nullptr;
    QLabel *heading,*preview,*meta,*type;
    NoteFields *editor=nullptr;
};
