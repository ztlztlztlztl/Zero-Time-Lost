#pragma once
#include <QtWidgets>

class SettingButton : public QPushButton {
public:
    using QPushButton::QPushButton;
protected:
    void paintEvent(QPaintEvent *event) override;
};

class SettingsListWidget : public QListWidget {
public:
    using QListWidget::QListWidget;
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    QPersistentModelIndex toggleCandidate;
    QPoint pressPosition;
};

// Let the enclosing settings page scroll without changing the value under the pointer.
class NoWheelComboBox : public QComboBox {
    Q_OBJECT
public:
    explicit NoWheelComboBox(QWidget *parent=nullptr):QComboBox(parent){}
protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};
class NoWheelSpinBox : public QSpinBox {
    Q_OBJECT
public:
    explicit NoWheelSpinBox(QWidget *parent=nullptr):QSpinBox(parent){setKeyboardTracking(false);}
protected:
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};
namespace SettingControls {
inline constexpr int ListRowHeight=48;
void applyTheme(QWidget *widget,const QColor &accent);
QColor accentColor(const QWidget *widget);
QWidget *boundary(const QColor &above,const QColor &below,QWidget *parent=nullptr);
void configureList(QListWidget *list);
QWidget *buttonSlot(QPushButton *button,bool listAction=true);
QFormLayout *form(QWidget *widget);
QLabel *label(const QString &text);
QDialog *popup(QWidget *owner,const QString &title,const QString &name);
void showPopup(QDialog *dialog,QWidget *anchor);
}
