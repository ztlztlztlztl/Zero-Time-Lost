#pragma once
#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QString>

// Shared by QSS, rich text and hand-painted Calendar/Mail/Note surfaces.
namespace UiPalette {
inline const QColor Base("#101216"), Surface("#191B20"), Raised("#22252B"), Line("#373B43");
inline const QColor Text("#E7E8EB"), Muted("#A0A4AD"), Faint("#737984");
inline const QColor Home("#58A9D6"), Mail("#F0B56D"), Calendar("#F08FA7"), Note("#A9C984"), Settings("#ADA0E8");
inline const QColor Accent = Home;
inline QColor tint(const QColor &accent, qreal amount = .14) {
    return QColor(qRound(Base.red()*(1-amount)+accent.red()*amount),
                  qRound(Base.green()*(1-amount)+accent.green()*amount),
                  qRound(Base.blue()*(1-amount)+accent.blue()*amount));
}
inline const QColor Selected = tint(Home), AccentFill = tint(Home,.24);
inline const QColor Danger("#E7979E"), Warning("#F0B56D"), Success("#A9C984");
inline QColor pageAccent(int index) {
    switch(index){case 1:return Mail;case 2:return Calendar;case 3:return Note;case 4:return Settings;default:return Home;}
}
inline const QHash<QString, QColor> &swatches() {
    static const auto values = [] {
        QHash<QString, QColor> result;
        auto group = [&](const QColor &value, std::initializer_list<const char *> names) {
            for (auto name : names) result.insert(QString::fromLatin1(name), value);
        };
        group(Base, {"#101216", "#0e1014", "#11161c", "#0d1c27"});
        group(Surface, {"#14171b", "#14171c", "#14171d", "#191b23", "#191c22", "#19232d", "#142936"});
        group(Raised, {"#171a20", "#1a1e26", "#1b1c23", "#1c1921", "#20242d", "#242832", "#243746", "#1b3645", "#18313f"});
        group(Selected, {"#202534", "#224a32", "#232630", "#24202a", "#30202a", "#343b55", "#36271c"});
        group(Line, {"#25282e", "#28362e", "#292d35", "#292f35", "#302820", "#30333a", "#30473b", "#343841", "#35383e", "#373b43", "#46372a", "#514133", "#51576a", "#344956", "#496474"});
        group(Line, {"#304b5b", "#233e4d"});
        group(Home, {"#0c7362", "#80bece", "#79d4c8"});
        group(Mail, {"#754218"});
        group(Calendar, {"#7f2636", "#ce5975", "#e87993", "#f2adc0"});
        group(Note, {"#176a39", "#3caa70", "#64e9a2"});
        group(Settings, {"#463d73", "#7774dd", "#8780d0", "#8984d2"});
        group(tint(Home), {"#244858"});
        group(tint(Home,.24), {"#285366"});
        group(tint(Calendar,.24), {"#a23750"});
        group(tint(Calendar), {"#30202a", "#24202a"});
        group(tint(Note), {"#224a32"});
        group(tint(Mail), {"#36271c"});
        group(Faint, {"#555b64", "#555b66", "#646a73", "#666d78", "#63717c"});
        group(Muted, {"#678573", "#737a86", "#777b87", "#777e8a", "#789183", "#81868e", "#8c939d", "#9298a2", "#929a9f", "#a1a6af"});
        group(Text, {"#acb8e2", "#b3bac3", "#b9bec7", "#c7cbd2", "#cbd0d8", "#d0d0d0", "#d5d4ff", "#d5dcda", "#d8d8d2", "#dce0e5", "#dddded", "#e0e4e3", "#eeeeee", "#efeff5", "#e6edf3", "#dce5ee", "#edf4fa"});
        group(Calendar, {"#ce8394", "#d08c9c"});
        group(Warning, {"#d3af80", "#dccbb5"});
        group(Danger, {"#b8445d", "#bf705e", "#cfa895", "#dca79d"});
        group(Text, {"#e5eef2"});group(Muted, {"#9fb6c3"});group(Faint, {"#718c9d"});
        group(Danger, {"#dc8e99"});group(Warning, {"#d4ab73"});group(Success, {"#8bb7a6"});
        return result;
    }();
    return values;
}
inline QColor color(const QString &value) { return swatches().value(value.toLower(), QColor(value)); }
inline QColor color(int r, int g, int b, int a = 255) {
    auto value = (r == 127 && g == 38 && b == 54) ? Calendar : QColor(r, g, b);
    value.setAlpha(a);
    return value;
}
inline QString style(QString value, const QColor &accent = {}) {
    static const QRegularExpression hex("#[0-9a-fA-F]{6}(?![0-9a-fA-F])");
    auto matches = hex.globalMatch(value);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const auto key = match.captured().toLower();
        auto mapped = color(key);
        // Re-theme only generic control tokens; semantic page colors stay explicit.
        if(accent.isValid()){
            if(mapped==Home)mapped=accent;
            else if(mapped==Selected)mapped=tint(accent);
            else if(mapped==AccentFill)mapped=tint(accent,.24);
        }
        value.replace(match.capturedStart(), 7, mapped.name());
    }
    value.replace("Microsoft YaHei UI", "DengXian");
    value.replace("Segoe UI", "Bahnschrift");
    return value;
}
} // namespace UiPalette
