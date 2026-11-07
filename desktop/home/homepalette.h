#pragma once
#include <QColor>
#include <array>
#include <cmath>
namespace HomePalette {
inline QColor blend(const QColor &a,const QColor &b,qreal amount){
    return QColor::fromRgbF(a.redF()*(1-amount)+b.redF()*amount,a.greenF()*(1-amount)+b.greenF()*amount,a.blueF()*(1-amount)+b.blueF()*amount);
}
inline std::array<QColor,3> colors(qreal seconds){
    const QColor cyan("#58A9D6"),azure("#ADA0E8"),skyBlue("#F0B56D");
    const qreal mix=.5+.5*std::sin(seconds/36.0);
    return {blend(cyan,azure,mix),blend(azure,skyBlue,mix),blend(skyBlue,cyan,mix)};
}
}
