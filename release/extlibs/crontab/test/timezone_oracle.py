"""Compare the C schedule against independent Python/IANA local calendars."""
import ctypes
import datetime as dt
import os
import sys
import time
from zoneinfo import ZoneInfo


def stamp(year, month, day, hour=0, minute=0, second=0):
    return int(dt.datetime(year, month, day, hour, minute, second, tzinfo=dt.timezone.utc).timestamp())


# 直接写独立匹配条件，不复制产品的解析器、位图或跳步算法。
RULES = (
    (b"* * * * *", lambda d: True),
    (b"*/17 * * * *", lambda d: d.minute % 17 == 0),
    (b"3-21/6 2-22/5 * * *", lambda d: 3 <= d.minute <= 21 and (d.minute-3) % 6 == 0
     and d.hour >= 2 and (d.hour-2) % 5 == 0),
    (b"5 12 * * 1-5", lambda d: d.minute == 5 and d.hour == 12 and d.weekday() < 5),
    (b"0 12 15 1 2", lambda d: d.minute == 0 and d.hour == 12 and d.month == 1
     and (d.day == 15 or d.weekday() == 1)),
    (b"0 12 */2 * 2", lambda d: d.minute == 0 and d.hour == 12
     and d.day % 2 == 1 and d.weekday() == 1),
    (b"0 12 1-31/2 * 0,7", lambda d: d.minute == 0 and d.hour == 12
     and (d.day % 2 == 1 or d.weekday() == 6)),
    (b"5/10 * * * *", lambda d: d.minute >= 5 and (d.minute-5) % 10 == 0),
)
SCENARIOS = {
    "UTC": [-121, -61, -60, -1, 0, 1, stamp(2040, 1, 1), stamp(2104, 2, 29)],
    "America/New_York": [-2840123039, -2717650858, stamp(2024, 3, 10, 6, 59),
                         stamp(2024, 11, 3, 5, 29), stamp(2024, 11, 3, 6, 29)],
    "Australia/Lord_Howe": [1712414640, 1712414700, 1728142140],
    "Asia/Kathmandu": [stamp(1985, 12, 31, 18, 29), stamp(2024, 1, 1)],
}


def main():
    if len(sys.argv) != 2 or not hasattr(time, "tzset"):
        raise SystemExit("usage: python3 timezone_oracle.py /absolute/path/libcrontab.so (POSIX required)")
    lib = ctypes.CDLL(os.path.abspath(sys.argv[1]))
    lib.xcron_is_valid.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    lib.xcron_is_valid.restype = ctypes.c_bool
    lib.xcron_matches.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int64]
    lib.xcron_matches.restype = ctypes.c_bool
    lib.xcron_next.argtypes = lib.xcron_matches.argtypes
    lib.xcron_next.restype = ctypes.c_int64
    checks, next_checks = 0, 0
    for name, anchors in SCENARIOS.items():
        zone = ZoneInfo(name)  # 缺少所需IANA数据会明确失败，不跳过时区。
        os.environ["TZ"] = name
        time.tzset()
        for anchor in anchors:
            for delta in (-61, -1, 0, 1, 59, 60, 61):
                moment = anchor+delta
                local = dt.datetime.fromtimestamp(moment, zone)
                for rule, match in RULES:
                    assert lib.xcron_is_valid(rule, len(rule)), rule
                    actual = lib.xcron_matches(rule, len(rule), moment)
                    assert actual == bool(match(local)), (name, moment, rule, local, actual)
                    checks += 1
            # 逐秒枚举最早本地整分钟，独立验证对齐及跨时差跳转。
            # 前三条最多隔数小时触发，限制两天避免测试自身无限等待。
            for rule, match in RULES[:3]:
                expected = None
                for moment in range(anchor+1, anchor+2*86400+1):
                    local = dt.datetime.fromtimestamp(moment, zone)
                    if local.second == 0 and match(local):
                        expected = moment
                        break
                assert expected is not None, (name, anchor, rule)
                actual = lib.xcron_next(rule, len(rule), anchor)
                assert actual == expected, (name, anchor, rule, expected, actual)
                assert actual > anchor and lib.xcron_matches(rule, len(rule), actual)
                next_checks += 1
    print(f"crontab Python timezone oracle: {checks} matches, {next_checks} next, {len(SCENARIOS)} zones: PASS")


if __name__ == "__main__":
    main()
