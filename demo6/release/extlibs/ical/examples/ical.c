#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_BUFFER
#define XRT_MODULE_UNICODE
#include "../include/ical_native.h"

/* 教学范例可单文件 xl run；正式包仍分别编译三个实现单元。 */
#include "../ical_native.c"
#include "../ical_parse.c"
#include "../ical_build.c"

#include <stdio.h>
#include <string.h>

static int put(xvalue *object, const char *key, const char *text) {
    return xrtValueObjectSetNew(object, (xstrview){key, strlen(key)},
        xrtValueString((xstrview){text, strlen(text)}));
}

static void print_field(xvalue *event, const char *key, const char *label) {
    xvalue *value = xrtValueObjectGet(event, (xstrview){key, strlen(key)});
    xstrview text = {"", 0};
    if (value != NULL) xrtValueGetString(value, &text);
    printf("%s %.*s\n", label, (int)text.Size, text.Data);
}

int main(int argc, char **argv) {
    xvalue *calendar = xrtValueObject();
    xvalue *events = xrtValueArray();
    xvalue *event = xrtValueObject();
    xvalue *back = NULL, *back_events, *parsed_event;
    char *text = NULL;
    int result = 1;
    if (calendar == NULL || events == NULL || event == NULL) goto done;
    if (!put(event, "uid", "standup@team") ||
        !put(event, "dtstamp", "20260801T000000Z") ||
        !put(event, "dtstart", "20260817T013000Z") ||
        !put(event, "summary", "团队例会") ||
        !put(event, "rrule", "FREQ=WEEKLY;BYDAY=MO,WE") ||
        !xrtValueArrayAppendTake(events, &event) ||
        !xrtValueObjectSetTake(calendar, (xstrview){"events", 6}, &events)) goto done;
    text = xi_build(calendar);
    if (text == NULL) goto done;
    if (argc > 1 && !xrtFileWriteAll(argv[1],
        (xbytesview){(const unsigned char *)text, strlen(text)})) goto done;
    back = xi_parse(text, strlen(text));
    if (back == NULL) goto done;
    back_events = xrtValueObjectGet(back, (xstrview){"events", 6});
    if (back_events == NULL || xrtValueCount(back_events) != 1) goto done;
    printf("events: %lld\n", (long long)xrtValueCount(back_events));
    parsed_event = xrtValueArrayAt(back_events, 0);
    if (parsed_event == NULL) goto done;
    print_field(parsed_event, "summary", "summary:");
    print_field(parsed_event, "rrule", "rrule:");
    puts("ical example: PASS");
    result = 0;
done:
    xrtValueRelease(back);
    xrtFree(text);
    xrtValueRelease(event);
    xrtValueRelease(events);
    xrtValueRelease(calendar);
    return result;
}
