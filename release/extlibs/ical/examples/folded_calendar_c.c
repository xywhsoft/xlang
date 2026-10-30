#define XRT_MODULE_UNICODE
#define XRT_MODULE_FILE_WHOLE
#include "../include/ical_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../ical_native.c"
#include "../ical_parse.c"
#include "../ical_build.c"
#endif

static xvalue* field(xvalue* object, const char* name)
{
    return xrtValueObjectGet(object, (xstrview){name, strlen(name)});
}

/* 与 XLang 相同：读取物理折行、恢复逻辑 TEXT，并规范化重新输出。 */
int main(int argc, char** argv)
{
    const char source[] = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:test\r\n"
        "BEGIN:VEVENT\r\nUID:one\r\nDTSTAMP:20261030T000000Z\r\n"
        "DTSTART:20261030T120000Z\r\nSUMMARY:团\r\n 队例会\r\n"
        "DESCRIPTION:第一行\\n第二\r\n\t行\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    xvalue* calendar = xi_parse(source, sizeof(source) - 1);
    xvalue* event = calendar ? xrtValueArrayAt(field(calendar, "events"), 0) : NULL;
    char* canonical = event ? xi_build(calendar) : NULL;
    xstrview summary = {0}, description = {0};
    bool ok = canonical && xrtUtf8Valid((xstrview){canonical, strlen(canonical)}, NULL) &&
        xrtValueGetString(field(event, "summary"), &summary) &&
        xrtValueGetString(field(event, "description"), &description);
    if (ok && argc == 2) ok = xrtFileWriteAll(argv[1], (xbytesview){(cbytes)canonical, strlen(canonical)});
    if (ok) {
        bool restored = description.Size == strlen("第一行\n第二行") &&
            !memcmp(description.Data, "第一行\n第二行", description.Size);
        bool normalized = strstr(canonical, "SUMMARY:团队例会\r\n") != NULL;
        printf("summary: %.*s\ndescription restored: %s\ncanonical summary: %s\n",
            (int)summary.Size, summary.Data, restored ? "true" : "false", normalized ? "true" : "false");
        ok = restored && normalized;
    }
    xrtFree(canonical);
    xrtValueRelease(calendar);
    return ok ? 0 : 1;
}
