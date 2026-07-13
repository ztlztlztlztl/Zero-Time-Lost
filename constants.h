#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QColor>
#include <QList>
#include <QPoint>
#include <QShortcut>
#include <QPair>

namespace ZTL{

namespace Main{

const QKeySequence homeShortcut =  QKeySequence(Qt::ALT | Qt::Key_H);
const QKeySequence settingShortcut =  QKeySequence(Qt::ALT | Qt::Key_S);
const QKeySequence mailShortcut =  QKeySequence(Qt::ALT | Qt::Key_M);

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

namespace Service{

const QString gaodeKey = "8b5465dc2621b97ccbcbef47ad0dec13";

const int timeInterval = 1200000;



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

namespace homepage{

const QList<QString> tags = {"无", "emo", "上午", "下午", "人生", "会议", "傍晚",
                             "锋利", "励志", "午间", "历史", "友情", "哲学",
                             "坚定", "夜晚", "工作", "希望", "幽默", "怀旧",
                             "成长", "技术", "放松", "时间", "死亡", "治愈",
                             "浪漫", "深夜", "清晨", "清醒", "温柔", "热血",
                             "爱情", "独处", "现实", "理想", "相思", "真理",
                             "禅意", "离别", "编码", "自由", "轻松", "释然", };

const QString scrollBarStyle = R"qss(
            QScrollBar:vertical {
                border: none;
                background-color: #101216;
                width: 8px;
                margin: 0px 0px 0px 0px;
            }

            QScrollBar::handle:vertical {
                background-color: #7F847F;
                min-height: 20px;
                border-radius: 4px;
            }

            QScrollBar::handle:vertical:hover,
            QScrollBar::handle:vertical:pressed {
                background-color: #EEEEEE;
            }

        )qss";

const QString backgroundStyle = R"qss(
            QWidget#homeBackground {
                background-color: #101216;
            }
        )qss";

const QString firstTitleStyle = R"qss(
            font: bold 24px "Microsoft YaHei";
            padding: 3px 3px 10px 3px;
            background-color: #101216;
            border-bottom: 2px solid #333;
        )qss";

const QString secondTitleStyle = R"qss(
            font: bold 20px "Microsoft YaHei";
            padding: 3px 3px 10px 3px;
            background-color: #101216;
        )qss";

const QString thirdTitleStyle = R"qss(
            font: bold 16px "Microsoft YaHei";
            margin: 3px 3px 15px 3px;
            padding: 3px 3px 6px 3px;
            background-color: #101216;
            border-top: transparent;
            border-bottom: transparent;
            border-left: 3px solid #EEEEEE;
            border-right: transparent;
        )qss";


const QString simpleContentStyle = R"qss(
            font: 16px "Microsoft YaHei";
            margin: 3px 3px 15px 3px;
            padding: 3px 3px 6px 3px;
            background-color: #101216;
        )qss";

const QString firstInFourButtonStyle = R"qss(
            QPushButton {
                font: 16px "Microsoft YaHei";
                margin: 8px 2px 3px 20px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 1px solid #EE7755;
                border-right: transparent;
            }

            QPushButton:hover {
                color: #EE7755;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 2px 3px 20px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #EE7755;
                border-right: transparent;
            }

            QPushButton:pressed {
                color: #EE7755;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 2px 3px 20px;
                padding: 2px 0px 0px 2px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #EE7755;
                border-right: transparent;
            }

        )qss";

const QString secondInFourButtonStyle = R"qss(
            QPushButton {
                font: 16px "Microsoft YaHei";
                margin: 8px 11px 3px 11px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 1px solid #66EE55;
                border-right: transparent;
            }

            QPushButton:hover {
                color: #66EE55;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 11px 3px 11px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #66EE55;
                border-right: transparent;
            }

            QPushButton:pressed {
                color: #66EE55;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 11px 3px 11px;
                padding: 2px 0px 0px 2px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #66EE55;
                border-right: transparent;
            }

        )qss";


const QString thirdInFourButtonStyle = R"qss(
            QPushButton {
                font: 16px "Microsoft YaHei";
                margin: 8px 11px 3px 11px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 1px solid #3377EE;
                border-right: transparent;
            }

            QPushButton:hover {
                color: #3377EE;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 11px 3px 11px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #3377EE;
                border-right: transparent;
            }

            QPushButton:pressed {
                color: #3377EE;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 11px 3px 11px;
                padding: 2px 0px 0px 2px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #3377EE;
                border-right: transparent;
            }

        )qss";


const QString forthInFourButtonStyle = R"qss(
            QPushButton {
                font: 16px "Microsoft YaHei";
                margin: 8px 20px 3px 2px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 1px solid #FF6666;
                border-right: transparent;
            }

            QPushButton:hover {
                color: #FF6666;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 20px 3px 2px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #FF6666;
                border-right: transparent;
            }

            QPushButton:pressed {
                color: #FF6666;
                font: bold 16px "Microsoft YaHei";
                margin: 8px 20px 3px 2px;
                padding: 2px 0px 0px 2px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #FF6666;
                border-right: transparent;
            }

        )qss";

const QString cityInputStyle = R"qss(
            QLineEdit {
                background-color: #1A1D24;
                font: 14px "Microsoft YaHei";
                color: #FFFFFF;
                border-radius: 4px;
                padding: 2px 8px;
                border-top: transparent;
                border-bottom: 1px solid #AAAAAA;
                border-left: transparent;
                border-right: transparent;
                selection-background-color: #005555;
                selection-color: #FFFFFF;
            }

            QLineEdit:hover {
                border-top: transparent;
                border-bottom: 1px solid #005555;
                border-left: transparent;
                border-right: transparent;
            }

            QLineEdit:focus {
                border-top: transparent;
                border-bottom: 1px solid #005555;
                border-left: transparent;
                border-right: transparent;
                background-color: #131519;
            }

            QLineEdit:placeholder {
                color: #005555;
                font: 14px "Microsoft YaHei" italic;
            }

        )qss";

const QString codeInputStyle = R"qss(
            QLineEdit {
                background-color: #1A1D24;
                font: 14px "Microsoft YaHei";
                color: #FFFFFF;
                border-radius: 4px;
                padding: 2px 8px;
                border-top: transparent;
                border-bottom: 1px solid #AAAAAA;
                border-left: transparent;
                border-right: transparent;
                selection-background-color: #005555;
                selection-color: #FFFFFF;
            }

            QLineEdit:hover {
                border-top: transparent;
                border-bottom: 1px solid #005555;
                border-left: transparent;
                border-right: transparent;
            }

            QLineEdit:focus {
                border-top: transparent;
                border-bottom: 1px solid #005555;
                border-left: transparent;
                border-right: transparent;
                background-color: #131519;
            }

            QLineEdit:placeholder {
                color: #005555;
                font: 14px "Microsoft YaHei" italic;
            }

        )qss";

const QString citySubmitStyle = R"qss(
            QPushButton {
                font: 14px "Microsoft YaHei";
                margin: 0px 0px 0px 0px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 1px solid #005555;
                border-right: transparent;
            }

            QPushButton:hover {
                font: bold 14px "Microsoft YaHei";
                margin: 0px 0px 0px 0px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #005555;
                border-right: transparent;
            }

            QPushButton:pressed {
                font: bold 14px "Microsoft YaHei";
                margin: 0px 0px 0px 0px;
                padding: 2px 0px 0px 2px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #005555;
                border-right: transparent;
            }

        )qss";

const QString themeComboStyle = R"qss(
            QComboBox {
                font: 14px "Microsoft YaHei";
                background-color: #1A1D24;
                color: #FFFFFF;
                border-top: transparent;
                border-bottom: 1px solid #AAAAAA;
                border-left: transparent;
                border-right: transparent;
                border-radius: 4px;
                padding: 2px 8px;
            }

            QComboBox:hover {
                border-top: transparent;
                border-bottom: 1px solid #992244;
                border-left: transparent;
                border-right: transparent;
            }

            QComboBox:on {
                border-top: transparent;
                border-bottom: 1px solid #992244;
                border-left: transparent;
                border-right: transparent;
            }

            QComboBox:placeholder {
                color: #992244;
                font: 14px "Microsoft YaHei" italic;
            }
        )qss";

const QString themeSubmitStyle = R"qss(
            QPushButton {
                font: 14px "Microsoft YaHei";
                margin: 0px 0px 0px 0px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 1px solid #992244;
                border-right: transparent;
            }

            QPushButton:hover {
                font: bold 14px "Microsoft YaHei";
                margin: 0px 0px 0px 0px;
                padding: 0px 0px 0px 0px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #992244;
                border-right: transparent;
            }

            QPushButton:pressed {
                font: bold 14px "Microsoft YaHei";
                margin: 0px 0px 0px 0px;
                padding: 2px 0px 0px 2px;
                background-color: #101216;
                border-top: transparent;
                border-bottom: transparent;
                border-left: 3px solid #992244;
                border-right: transparent;
            }

        )qss";

const QString listButtonStyle = R"qss(
                QPushButton {
                    font: 14px "Microsoft YaHei";
                    margin: 3px 11px 3px 11px;
                    padding: 0px 0px 0px 0px;
                    background-color: #101216;
                    border-top: transparent;
                    border-bottom: transparent;
                    border-left: 3px solid #333333;
                    border-right: 1px solid #333333;
                }

                QPushButton:hover {
                    color: #6688EC;
                    font: bold 14px "Microsoft YaHei";
                    margin: 3px 11px 3px 11px;
                    padding: 0px 0px 0px 0px;
                    background-color: #101216;
                    border-top: transparent;
                    border-bottom: transparent;
                    border-left: 3px solid #6688EC;
                    border-right: 1px solid #6688EC;
                }

                QPushButton:pressed {
                    color: #3377EE;
                    font: bold 14px "Microsoft YaHei";
                    margin: 3px 11px 3px 11px;
                    padding: 2px 0px 0px 2px;
                    background-color: #101216;
                    border-left: 3px solid #3377EE;
                    border-right: 1px solid #3377EE;
                }

                QPushButton:checked {
                    color: #3377EE;
                    font: bold 14px "Microsoft YaHei";
                    margin: 3px 11px 3px 11px;
                    padding: 0px 0px 0px 0px;
                    background-color: #101216;
                    border-left: 3px solid #3377EE;
                    border-right: 1px solid #3377EE;
                }
            )qss";



}

namespace mailpage{

const QList<QPair<QString, int>> intervals = {
    {"从不",  -1},
    {"5m",    5},
    {"10m",  10},
    {"30m",  30},
    {"1h",   60},
    {"2h",  120},
    {"6h",  360},
};

const QList<QPair<QString, int>> initNumbers = {
    {"5",     5},
    {"20",   20},
    {"50",   50},
    {"100", 100},
    {"500", 500},
};

const QList<QPair<QString, int>> pollNumbers = {
    {"1",    1},
    {"3",    3},
    {"5",    5},
    {"10",  10},
    {"20",  20},
};

const QList<QString> mailTypes = {"QQ邮箱", "163邮箱", "其他/企业邮箱"};
const QList<QString> mailServers = {"imap.qq.com", "imap.163.com", ""};


}


}








}



#endif // CONSTANTS_H
