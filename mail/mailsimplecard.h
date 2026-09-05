#ifndef MAILSIMPLECARD_H
#define MAILSIMPLECARD_H

#include "datatype.h"
#include "constants.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>









class ElidedLabel : public QLabel {
    Q_OBJECT
public:
    explicit ElidedLabel(QWidget *parent = nullptr) : QLabel(parent) {}

    void setFullText(const QString &text) {
        m_fullText = text;
        setToolTip(text);
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        QFontMetrics metrics(font());
        QString elidedText = metrics.elidedText(m_fullText, Qt::ElideRight, width());

        drawFrame(&painter);
        painter.drawText(rect(), alignment(), elidedText);
    }

private:
    QString m_fullText;
};





class MailSimpleCard : public QWidget
{
    Q_OBJECT
public:
    explicit MailSimpleCard(const MailBrief& mailBrief, QString name, QWidget *parent = nullptr);

signals:
    void readRequest();
    void checkRequest();
    void deleteRequest();

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;


private:
    QLabel *m_lblAccount;
    QPushButton *m_checkBtn;
    QPushButton *m_deleteBtn;
    QLabel *m_lblSender;
    ElidedLabel *m_lblSubject;
    QLabel *m_lblTime;











};

#endif // MAILSIMPLECARD_H
