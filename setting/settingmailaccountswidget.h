#ifndef SETTINGMAILACCOUNTSWIDGET_H
#define SETTINGMAILACCOUNTSWIDGET_H



#include "mailaccountmanager.h"
#include "constants.h"


#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QProcess>

class SettingMailAccountsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SettingMailAccountsWidget(QWidget *parent = nullptr);
    void setAccount(const MailAccount& iacc);

signals:
    void cancel();


private:
    QLabel *m_typeText;
    QComboBox *m_typeCombo;
    QLabel *m_nameText;
    QLineEdit *m_nameInput;
    QLabel *m_addressText;
    QLineEdit *m_addressInput;
    QLabel *m_codeText;
    QLineEdit *m_codeInput;
    QLabel *m_serverText;
    QLineEdit *m_serverInput;
    QLabel *m_portText;
    QLineEdit *m_portInput;
    QPushButton *m_saveBtn;
    QPushButton *m_linkBtn;
    QPushButton *m_cancelBtn;

    void ckeckLinkable();

















};

#endif // SETTINGMAILACCOUNTSWIDGET_H
