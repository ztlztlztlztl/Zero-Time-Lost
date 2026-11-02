#pragma once
#include <QList>
#include <QPair>
#include <QString>

namespace ZTL::Setting {
namespace mailpage {
inline const QList<QPair<QString, int>> intervals = {
    {"从不",  -1},
    {"5m",    5},
    {"10m",  10},
    {"30m",  30},
    {"1h",   60},
    {"2h",  120},
    {"6h",  360},
};

inline const QList<QPair<QString, int>> initNumbers = {
    {"5",     5},
    {"20",   20},
    {"50",   50},
    {"100", 100},
    {"500", 500},
};

inline const QList<QPair<QString, int>> pollNumbers = {
    {"1",    1},
    {"3",    3},
    {"5",    5},
    {"10",  10},
    {"20",  20},
};

inline const QList<QString> mailTypes = {"QQ邮箱", "163邮箱", "其他/企业邮箱"};

inline const QList<QString> mailServers = {"imap.qq.com", "imap.163.com", ""};
}

namespace deadlinePage {
inline const QList<QPair<QString, qint64>> remindTime = {
    {"从不",        -1},
    {"10 Min",    600},
    {"30 Min",   1800},
    {"1 Hour",   3600},
    {"2 Hour",   7200},
    {"6 Hour",  21600},
    {"1 Day",   86400},
    {"2 Day",  172800},
    {"1 Week", 604800},
    };
}
}
