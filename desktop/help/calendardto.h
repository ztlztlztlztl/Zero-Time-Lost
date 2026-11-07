#pragma once
#include "recurrence.h"
#include "tagrules.h"
// Validate serialized dates in their source timezone, never in the receiving PC's timezone.
namespace CalendarDTO {
inline QString typeError(const QJsonObject &t) {
    const auto kind = t["type"].toString("ddl"), repeat = t["repeat"].toString("none");
    if (!QStringList{"ddl", "label", "interval"}.contains(kind))
        return "事务类别无效";
    const auto end = QDateTime::fromString(t["due"].toString(), Qt::ISODate);
    if (kind == "label" && (!t["hasDue"].toBool() || repeat != "none" || !end.isValid()))
        return "全天标签需要日期且不能循环";
    if (kind == "interval") {
        const auto begin = QDateTime::fromString(t["start"].toString(), Qt::ISODate);
        const auto zone = Recurrence::zone(t);
        const bool sameDay = t.contains("zone") ? begin.toTimeZone(zone).date() == end.toTimeZone(zone).date()
                                                : begin.date() == end.date();
        if (!t["hasDue"].toBool() || !begin.isValid() || !end.isValid() || begin >= end || !sameDay)
            return "区间事务必须在同一天，且开始时间早于结束时间";
    }
    return {};
}
inline QString validate(QJsonObject input, bool instances = true) {
    // Existing local commands may serialize an absent optional field as null.
    // Treat null as absence in both implementations, but reject wrong concrete types.
    for (const auto &field : input.keys()) if (input[field].isNull()) input.remove(field);
    const QJsonObject t = std::move(input);
    auto error = typeError(t);
    if (!error.isEmpty())
        return error;
    if (!t["title"].isString() || t["title"].toString().trimmed().isEmpty())
        return "calendar.title：标题不可为空";
    const auto repeat = t["repeat"].toString("none");
    if (!QStringList{"none", "daily", "weekly", "monthly"}.contains(repeat))
        return "calendar.repeat：循环规则无效";
    if (t.contains("hasDue") && !t["hasDue"].isBool())
        return "calendar.hasDue：需要布尔值";
    for (auto field : {"paused", "countLimited", "done", "deleted", "remind"})
        if (t.contains(field) && !t[field].isBool())
            return QString("calendar.%1：需要布尔值").arg(field);
    for (auto field : {"note", "tags"})
        if (t.contains(field) && !t[field].isString())
            return QString("calendar.%1：需要文本").arg(field);
    if (t["type"] == "label" && (!t["note"].toString().isEmpty() || !t["tags"].toString().isEmpty() || t["remind"].toBool()))
        return "全天标签不支持备注、标签或提醒";
    if (t["type"] == "interval" && t["remind"].toBool())
        return "区间事务不提醒";
    if (t["countLimited"].toBool() && (!t["countLimit"].isDouble() || t["countLimit"].toDouble() != t["countLimit"].toInt()))
        return "循环次数需要整数";
    if (t["hasDue"].toBool() && !QDateTime::fromString(t["due"].toString(), Qt::ISODate).isValid())
        return "calendar.due：时间无效";
    if (repeat != "none" && !t["hasDue"].toBool())
        return "循环事务需要时间";
    if (t.contains("zone") && !QTimeZone(t["zone"].toString().toUtf8()).isValid())
        return "calendar.zone：时区无效";
    for (auto field : {"weekdays", "monthdays"})
        if (t.contains(field)) {
            if (!t[field].isArray())
                return "循环日期需要列表";
            QSet<int> seen;
            for (auto v : t[field].toArray()) {
                const int top = QString(field) == "weekdays" ? 7 : 31;
                if (!v.isDouble() || v.toDouble() != v.toInt() || v.toInt() < 1 || v.toInt() > top ||
                    seen.contains(v.toInt()))
                    return "循环日期无效或重复";
                seen.insert(v.toInt());
            }
        }
    if (repeat != "none") {
        error = Recurrence::validate(t);
        if (!error.isEmpty())
            return error;
    }
    if (t["type"] != "label") {
        error = TagRules::selectionError(t["tags"].toString());
        if (!error.isEmpty())
            return error;
    }
    if (t.contains("labelDate") && !QDate::fromString(t["labelDate"].toString(), Qt::ISODate).isValid())
        return "全天标签日期无效";
    const auto until = t["until"].toString();
    if (t.contains("until") && (!t["until"].isString() || (!until.isEmpty() && (!QDate::fromString(until, Qt::ISODate).isValid() || QDate::fromString(until, Qt::ISODate) < Recurrence::start(t).date()))))
        return "循环结束日期无效或早于首次日期";
    for (auto field : {"excluded", "consumed"}) {
        if (t.contains(field) && !t[field].isArray()) return "循环排除日期需要列表";
        for (const auto &value : t[field].toArray())
            if (!QDate::fromString(value.toString(), Qt::ISODate).isValid()) return "循环排除日期无效";
    }
    if (t.contains("instances") && !t["instances"].isObject())
        return "calendar.instances：需要对象";
    if (instances) {
        const auto values = t["instances"].toObject();
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (!it.value().isObject() ||
                (repeat != "none" && !QDate::fromString(it.key(), Qt::ISODate).isValid()))
                return "单次例外格式无效";
            const auto v = it.value().toObject();
            for (auto field : {"done", "deleted"})
                if (v.contains(field) && !v[field].isBool()) return "单次例外状态需要布尔值";
            for (auto field : {"due", "start"})
                if (v.contains(field) && !QDateTime::fromString(v[field].toString(), Qt::ISODate).isValid())
                    return "单次例外时间无效";
            if (t["type"] == "interval" && (v.contains("due") || v.contains("start"))) {
                auto merged = t;
                const auto day = QDate::fromString(it.key(), Qt::ISODate);
                if (day.isValid()) {
                    merged["due"] = Recurrence::endAt(t, day).toString(Qt::ISODate);
                    merged["start"] = QDateTime(day,
                                                QDateTime::fromString(t["start"].toString(), Qt::ISODate)
                                                    .toTimeZone(Recurrence::zone(t))
                                                    .time(),
                                                Recurrence::zone(t))
                                          .toString(Qt::ISODate);
                }
                for (auto name : {"start", "due"})
                    if (v.contains(name))
                        merged[name] = v[name];
                error = typeError(merged);
                if (!error.isEmpty())
                    return error;
            }
        }
    }
    if (t.contains("templateHistory") && !t["templateHistory"].isArray()) return "循环模板历史需要列表";
    for (const auto &value : t["templateHistory"].toArray()) {
        const auto history = value.toObject();
        if (!value.isObject() || !history["fields"].isObject() || !QDate::fromString(history["from"].toString(), Qt::ISODate).isValid() || !QDateTime::fromString(history["until"].toString(), Qt::ISODate).isValid()) return "循环模板历史格式无效";
        auto candidate = t;
        const auto fields = history["fields"].toObject();
        for (auto it=fields.begin();it!=fields.end();++it) candidate[it.key()]=it.value();
        candidate.remove("templateHistory");candidate.remove("instances");
        error = validate(candidate, false);
        if (!error.isEmpty()) return error;
    }
    return {};
}
} // namespace CalendarDTO
