#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QColor>
#include <QList>
#include <QPoint>
#include <QShortcut>

namespace ZTL{

namespace Main{

const QKeySequence homeShortcut =  QKeySequence(Qt::ALT | Qt::Key_H);
const QKeySequence settingShortcut =  QKeySequence(Qt::ALT | Qt::Key_S);

}

namespace Home{

namespace Background{

const double timeInterval = 0.050f; // 计时器间隔

}

namespace Info{
const QString locationStyle = "color: #FFFFFF; font-family: 'JetBrains Mono', 'Segoe UI'; font-weight: bold;";
const QString locationStyleResize = "color: #FFFFFF; font-size: %1px; font-family: 'JetBrains Mono', 'Segoe UI'; font-weight: 500;";
const QString tempStyle = "color: #FFFFFF; font-family: 'JetBrains Mono', 'Segoe UI'; font-weight: bold;";
const QString tempStyleResize = "color: #FFFFFF; font-size: %1px; font-family: 'JetBrains Mono', 'Segoe UI'; font-weight: 500;";
const QString weatherStyle = "color: #FFFFFF; font-family: 'Microsoft YaHei';";
const QString weatherStyleResize = "color: #FFFFFF; font-size: %1px; font-family: 'Microsoft YaHei';";
const QString timeStyle = "color: #FFFFFF; font-family: 'JetBrains Mono', 'Consolas'; font-weight: 300;";
const QString timeStyleResize = "color: #FFFFFF; font-size: %1px; font-family: 'JetBrains Mono', 'Consolas'; font-weight: 300;";
const QString dateStyle = "color: #FFFFFF; font-family: 'JetBrains Mono', 'Microsoft YaHei';";
const QString dateStyleResize = "color: #FFFFFF; font-size: %1px; font-family: 'JetBrains Mono', 'Microsoft YaHei';";
const QString quoteStyle = "color: #FFFFFF; font-family: 'Microsoft YaHei'; font-weight: bold; letter-spacing: 8px;";
const QString quoteStyleResize = "color: #FFFFFF; font-size: %1px; font-family: 'Microsoft YaHei'; font-weight: 500; letter-spacing: %2px;";



}

}

namespace Setting{

namespace global{
const QString backgroundStyle =
            "#settingPage {"
            "background-color: #101216;"
            "}";

}

namespace buttonlist{

const int minwidth = 80;
const int maxwidth = 80;

const QString buttonStyle = R"qss(
            QPushButton {
                background-color: #101216;
                color: #404040;
                border: 2px dashed #303030;
                font-size: 14px;
                border-radius: 4px;
            }

            QPushButton:checked,
            QPushButton:hover:checked {
                color: #d0d0d0;
                border: 2px dashed #505050;
                font-size: 16px;
                font-weight: bold;
            }

            QPushButton:hover {
                background-color: #16181c;
                color: #a0a0a0;
                border: 2px dashed #303030;
                font-size: 14px;
            }


            )qss";

}



}








}



#endif // CONSTANTS_H
