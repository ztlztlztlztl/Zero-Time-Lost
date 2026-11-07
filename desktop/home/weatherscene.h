#pragma once
#include <QString>
#include <QTime>
#include <QtMath>
#include <cmath>

// Pure scene inputs. Neither the renderer nor this mapping performs network I/O.
namespace WeatherScene {
struct Parameters {
    float clouds=0, rain=0, snow=0, fog=0, storm=0;
};
inline Parameters fromCondition(const QString &condition) {
    const auto value=condition.trimmed();
    Parameters result;
    if(value.isEmpty())return result; // Missing/failed weather is a clear, gentle sea.
    if(value.contains("云"))result.clouds=value.contains("少云")?.20f:.48f;
    if(value.contains("阴"))result.clouds=.86f;
    if(value.contains("雨")||value.contains("降水")||value.contains("冰雹")){
        result.rain=value.contains("暴")||value.contains("极端")?.95f:value.contains("大")||value.contains("强")?.78f:value.contains("中")?.58f:.36f;
        result.clouds=.90f;
    }
    if(value.contains("雪")){
        result.snow=value.contains("暴")?.95f:value.contains("大")?.78f:value.contains("中")?.58f:.40f;
        result.clouds=.82f;
        if(value.contains("雨")){result.rain=.38f;result.snow=.42f;}
    }
    if(value.contains("雷")){result.storm=1;result.clouds=1;result.rain=qMax(.68f,result.rain);}
    if(value.contains("雾")||value.contains("霾")||value.contains("沙")||value.contains("尘"))result.fog=value.contains("浓")||value.contains("重")?.85f:.55f;
    return result;
}
inline Parameters blend(const Parameters &a,const Parameters &b,float t) {
    return {a.clouds+(b.clouds-a.clouds)*t,a.rain+(b.rain-a.rain)*t,
            a.snow+(b.snow-a.snow)*t,a.fog+(b.fog-a.fog)*t,a.storm+(b.storm-a.storm)*t};
}
struct Celestial {
    float sunProgress=0,moonProgress=0,sunOpacity=0;
};
inline Celestial celestial(float hours) {
    hours=std::fmod(hours+24.f,24.f);
    auto smooth=[](float a,float b,float x){const auto t=qBound(0.f,(x-a)/(b-a),1.f);return t*t*(3-2*t);};
    return {qBound(0.f,(hours-6)/12,1.f),
            qBound(0.f,(hours>=18?hours-18:hours+6)/12,1.f),
            smooth(5.75f,6.25f,hours)*(1-smooth(17.75f,18.25f,hours))};
}
}
