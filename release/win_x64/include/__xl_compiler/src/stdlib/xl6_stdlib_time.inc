/* Immutable fields are a native leaf; the language still owns its lifetime. */
const char* xl6StdlibDateTimeSource =
    "class DateTime\n"
    "    private native point handle\n"
    "    extern \"xl6DateTimeSplitV1\" create(time value, int offset = 0)\n"
    "    extern \"xl6DateTimeFieldsV1\" create(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int millisecond = 0, int offset = 0)\n"
    "    extern \"xl6DateTimeLocalV1\" create(time value, bool local)\n"
    "    extern \"xl6DateTimeParseV1\" create(string text, string pattern, int size = -1, int patternSize = -1)\n"
    "    extern \"xl6DateTimeDestroyV1\" destroy()\n"
    "    extern \"xl6DateTimeYearV1\" @pure @no_raise @nonblocking get year() as int\n"
    "    extern \"xl6DateTimeMonthV1\" @pure @no_raise @nonblocking get month() as int\n"
    "    extern \"xl6DateTimeDayV1\" @pure @no_raise @nonblocking get day() as int\n"
    "    extern \"xl6DateTimeHourV1\" @pure @no_raise @nonblocking get hour() as int\n"
    "    extern \"xl6DateTimeMinuteV1\" @pure @no_raise @nonblocking get minute() as int\n"
    "    extern \"xl6DateTimeSecondV1\" @pure @no_raise @nonblocking get second() as int\n"
    "    extern \"xl6DateTimeMillisecondV1\" @pure @no_raise @nonblocking get millisecond() as int\n"
    "    extern \"xl6DateTimeOffsetV1\" @pure @no_raise @nonblocking get offset() as int\n"
    "    extern \"xl6DateTimeWeekdayV1\" @pure @no_raise @nonblocking get weekday() as int\n"
    "    extern \"xl6DateTimeDayOfYearV1\" @pure @no_raise @nonblocking get dayOfYear() as int\n"
    "    extern \"xl6DateTimeIsDSTV1\" @pure @no_raise @nonblocking get isDST() as int\n"
    "    extern \"xl6DateTimeToTimeV1\" func toTime() as time\n"
    "    extern \"xl6DateTimeFormatV1\" func format(string pattern = \"%F %T\", int patternSize = -1) as string\n"
    "end\n";
const char* xl6StdlibTimeFoldSource =
    "enum TimeFold as int\n    reject = 0\n    earlier = 1\n    later = 2\nend\n";
static const char xl6StdTimeSplitBody[] =
    "func __xl_std_time_split(time value, int offset = 0) as DateTime\n"
    "    return DateTime(value, offset)\n"
    "end\n";
static const char xl6StdTimeLocalBody[] =
    "func __xl_std_time_local(time value) as DateTime\n"
    "    return DateTime(value, true)\n"
    "end\n";
static const char xl6StdTimeFromLocalBody[] =
    "extern \"xl6TimeFromLocalV1\" func __xl_time_from_local_native(DateTime value, int fold) as time\n"
    "func __xl_std_time_from_local(DateTime value, TimeFold fold = TimeFold.reject) as time\n"
    "    return __xl_time_from_local_native(value, int(fold))\n"
    "end\n";
static const char xl6StdTimeMonthRangeBody[] =
    "extern \"xl6TimeMonthRangeV1\" func __xl_std_time_month_range(time value) as (time, time)\n";
static const char xl6StdTimeYearRangeBody[] =
    "extern \"xl6TimeYearRangeV1\" func __xl_std_time_year_range(time value) as (time, time)\n";
static const char xl6StdTimeWeekRangeBody[] =
    "extern \"xl6TimeWeekRangeV1\" func __xl_std_time_week_range(time value, int firstWeekday = 1) as (time, time)\n";
static const char xl6StdTimeISOWeekBody[] =
    "extern \"xl6TimeISOWeekV1\" func __xl_std_time_iso_week(time value) as (int, int, int)\n";
static const char xl6StdTimeParseFieldsBody[] =
    "func __xl_std_time_parse_fields(string text, string pattern, int size = -1, int patternSize = -1) as DateTime\n"
    "    return DateTime(text, pattern, size, patternSize)\nend\n";
