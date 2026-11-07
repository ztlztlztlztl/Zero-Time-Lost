"""Pure validation for persisted business records; never coerces malformed input."""
from datetime import date, datetime, timedelta
import calendar as month_calendar
import re
from urllib.parse import urlsplit
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError


def need(condition, path, reason):
    if not condition:
        raise ValueError(path + ": " + reason)


def text(value, path, required=False):
    need(isinstance(value, str) and (not required or bool(value.strip())), path, "需要文本" if not required else "不可为空")


def day(value, path):
    try:
        need(isinstance(value, str) and re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}", value), path, "日期格式无效")
        return date.fromisoformat(value)
    except (ValueError, TypeError):
        raise ValueError(path + ": 日期无效") from None


def instant(value, path):
    try:
        need(isinstance(value, str) and "T" in value, path, "需要 ISO 日期时间")
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except (ValueError, TypeError):
        raise ValueError(path + ": 时间无效") from None


def tags(value, path, maximum=None):
    need(isinstance(value, list) and all(isinstance(v, str) for v in value), path, "需要标签列表")
    need(len(value) == len(set(value)), path, "标签重复")
    if maximum is not None:
        need(len(value) <= maximum, path, "最多三个标签")
    for i, tag in enumerate(value):
        weight = sum(1 if ord(c) < 128 else 2 for c in tag)
        need(tag.strip() == tag and 0 < weight <= 10 and not any(c in tag for c in ",，\n\r") and all(ord(c) >= 32 for c in tag), f"{path}[{i}]", "标签无效")


def calendar(p):
    p = {key: value for key, value in p.items() if value is not None}
    text(p.get("title"), "calendar.title", True)
    kind, repeat = p.get("type", "ddl"), p.get("repeat", "none")
    need(kind in ("ddl", "label", "interval"), "calendar.type", "未知类别")
    need(repeat in ("none", "daily", "weekly", "monthly"), "calendar.repeat", "未知循环")
    need(type(p.get("hasDue", False)) is bool, "calendar.hasDue", "需要布尔值")
    has_due = p.get("hasDue", False)
    for field in ("paused", "countLimited", "done", "deleted", "remind"):
        if field in p:
            need(type(p[field]) is bool, "calendar." + field, "需要布尔值")
    need(repeat == "none" or has_due, "calendar.hasDue", "循环必须指定时间")
    end = instant(p.get("due"), "calendar.due") if has_due else None
    zone = p.get("zone")
    if zone:
        try:
            ZoneInfo(zone)
        except (ZoneInfoNotFoundError, TypeError, ValueError):
            raise ValueError("calendar.zone: 无效时区") from None
    if kind == "label":
        need(has_due and repeat == "none", "calendar.label", "全天标签需要日期且不能循环")
        day(p.get("labelDate", str(end.date())), "calendar.labelDate")
        need(not p.get("note") and not p.get("tags") and not p.get("remind", False), "calendar.label", "全天标签无备注、标签或提醒")
    if kind == "interval":
        begin = instant(p.get("start"), "calendar.start")
        if zone and begin.tzinfo and end and end.tzinfo:
            begin, end = begin.astimezone(ZoneInfo(zone)), end.astimezone(ZoneInfo(zone))
        try:
            valid = has_due and begin < end and begin.date() == end.date()
        except TypeError:
            valid = False
        need(valid, "calendar.interval", "开始和结束须在同一天，且开始早于结束")
        need(not p.get("remind", False), "calendar.remind", "区间不提醒")
    for field, top in (("weekdays", 7), ("monthdays", 31)):
        if field in p or repeat == "weekly" and top == 7:
            days = p.get(field)
            need(isinstance(days, list) and all(type(d) is int and 1 <= d <= top for d in days) and len(days) == len(set(days)), "calendar." + field, "循环日期无效")
            need(bool(days) or repeat != ("weekly" if top == 7 else "monthly"), "calendar." + field, "至少选择一个日期")
    if p.get("countLimited"):
        need(type(p.get("countLimit")) is int and 1 <= p["countLimit"] <= 1000000, "calendar.countLimit", "次数超出范围")
    if p.get("countLimited") and end:
        first_end = instant(p["due"], "calendar.due")
        if zone and first_end.tzinfo:
            first_end = first_end.astimezone(ZoneInfo(zone))
        first = first_end.date()
        selected = p.get("weekdays" if repeat == "weekly" else "monthdays", [])
        if repeat == "monthly" and "monthdays" not in p:
            selected = [first.day]
        n = p["countLimit"]
        if repeat == "daily":
            need(n <= (date(9999, 12, 31) - first).days + 1, "calendar.countLimit", "末次日期超过 9999 年")
        elif repeat == "weekly":
            available = sum(1 for d in selected if d >= first.isoweekday())
            if n > available:
                remaining = n - available - 1
                weeks, offset = divmod(remaining, len(selected))
                days = (8 - first.isoweekday()) + weeks * 7 + sorted(selected)[offset] - 1
                need(days <= (date(9999, 12, 31) - first).days, "calendar.countLimit", "末次日期超过 9999 年")
        elif repeat == "monthly":
            def before(value):
                years, months = value.year - 1, value.month - 1
                leaps = years // 4 - years // 100 + years // 400
                total = 0
                for d in selected:
                    if d <= 28: total += years * 12 + months
                    elif d == 29: total += years * 11 + leaps + months - int(months >= 2 and not month_calendar.isleap(value.year))
                    elif d == 30: total += years * 11 + months - int(months >= 2)
                    else: total += years * 7 + sum(m < value.month for m in (1, 3, 5, 7, 8, 10, 12))
                    if d < value.day and d <= month_calendar.monthrange(value.year, value.month)[1]: total += 1
                return total
            last = date(9999, 12, 31)
            available = before(last) - before(first) + int(31 in selected)
            need(n <= available, "calendar.countLimit", "末次日期超过 9999 年")
    if p.get("until"):
        limit_date = day(p["until"], "calendar.until")
        need(end is None or limit_date >= end.date(), "calendar.until", "结束日期早于首次日期")
    for field in ("note", "tags"):
        if field in p:
            text(p[field], "calendar." + field)
    if p.get("tags"):
        tags(p["tags"].split(","), "calendar.tags", 3)
    instances = p.get("instances", {})
    need(isinstance(instances, dict), "calendar.instances", "需要对象")
    for key, value in instances.items():
        need(isinstance(value, dict), "calendar.instances." + key, "需要对象")
        if repeat != "none":
            day(key, "calendar.instances.key")
        for field in ("done", "deleted"):
            if field in value:
                need(type(value[field]) is bool, "calendar.instances." + key + "." + field, "需要布尔值")
        for field in ("due", "start"):
            if field in value:
                instant(value[field], "calendar.instances." + key + "." + field)
        if kind == "interval" and ("start" in value or "due" in value):
            nominal = day(key, "calendar.instances.key") if repeat != "none" else None
            defaults = {field: instant(p[field], 'calendar.' + field).replace(year=nominal.year, month=nominal.month, day=nominal.day).isoformat() if nominal else p[field] for field in ('start', 'due')}
            candidate = dict(p, instances={}, templateHistory=[], start=value.get("start", defaults["start"]), due=value.get("due", defaults["due"]))
            calendar(candidate)
    for field in ("excluded", "consumed"):
        need(isinstance(p.get(field, []), list), "calendar." + field, "需要日期列表")
        for value in p.get(field, []):
            day(value, "calendar." + field)
    need(isinstance(p.get("templateHistory", []), list), "calendar.templateHistory", "需要列表")
    for value in p.get("templateHistory", []):
        need(isinstance(value, dict) and isinstance(value.get("fields"), dict), "calendar.templateHistory", "模板无效")
        day(value.get("from"), "calendar.templateHistory.from")
        instant(value.get("until"), "calendar.templateHistory.until")
        candidate = p | value['fields'] | dict(templateHistory=[], instances={})
        calendar(candidate)


def settings(p):
    cities = p["weather"].get("savedCities", [])
    need(isinstance(cities, list) and len(cities) <= 100, "settings.weather.savedCities", "城市列表无效")
    codes = set()
    for i, city in enumerate(cities):
        path = f"settings.weather.savedCities[{i}]"
        need(isinstance(city, dict), path, "需要城市对象")
        text(city.get("name"), path + ".name", True)
        need(isinstance(city.get("code"), str) and re.fullmatch(r"[0-9]{6}", city["code"]), path + ".code", "需要六位城市编码")
        need(city["code"] not in codes, path, "城市重复")
        codes.add(city["code"])
    selected = p["weather"].get("currentCityIndex", -1)
    need(type(selected) is int and -1 <= selected < max(1, len(cities)), "settings.weather.currentCityIndex", "城市索引无效")
    for field, low, high in (("initialNumber", 1, 1000), ("earlierNumber", 1, 100), ("storageLimit", 1, 100000)):
        if field in p["mail"]:
            need(type(p["mail"][field]) is int and low <= p["mail"][field] <= high, "settings.mail." + field, "超出范围")
    text(p["weatherKey"], "settings.weatherKey")
    tags(p["noteTags"], "settings.noteTags")
    tags(p["calendar"].get("commonTags", []), "settings.calendar.commonTags")
    addresses = set()
    for account in p["accounts"]:
        need(account["address"] not in addresses, "settings.accounts.address", "邮箱重复")
        addresses.add(account["address"])
        if "name" in account:
            text(account["name"], "settings.accounts.name")


def note(p):
    text(p.get("title"), "note.title", True)
    if p.get("kind") == 2:
        text(p.get("body", ""), "note.body")
    else:
        target = p.get("target", "")
        text(target, "note.target", True)
        url = urlsplit(target)
        need(url.scheme in ("http", "https") and bool(url.hostname), "note.target", "网站地址无效")
    text(p.get("tags", ""), "note.tags")
    if p.get("tags"):
        tags(p["tags"].split(","), "note.tags", 3)
