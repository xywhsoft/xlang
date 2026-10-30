/* 与 XLang 同一配置和结果；C 事件是借用视图，不构造动态值对象。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_xml.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char source[] = "<catalog><book id='xlang'>语言</book><book id='xrt'>运行库</book></catalog>";
    const xrtxmlevent* event;
    xrtxmlerror error;
    xrtxmloptions options;
    unsigned books = 0;
    char names[64] = {0};
    size_t used = 0;
    bool ok = true;
    int status;
    xrtxmlsax* reader = xrtXmlSaxOpenMemory(source, sizeof(source) - 1, NULL, &error);
    if (!reader) return 1;
    while ((status = xrtXmlSaxNext(reader, &event, &error)) == XRT_XML_NEXT_EVENT) {
        if (event->iType == XRT_XML_EVENT_START && !strcmp(event->sName, "book")) ++books;
        if (event->iType == XRT_XML_EVENT_TEXT) {
            size_t size = strlen(event->sText);
            if (size >= sizeof(names) - used) { ok = false; break; }
            memcpy(names + used, event->sText, size);
            used += size;
            names[used] = 0;
        }
    }
    ok = ok && status == XRT_XML_NEXT_END && books == 2 && !strcmp(names, "语言运行库") &&
        xrtXmlSaxNext(reader, &event, &error) == XRT_XML_NEXT_END && xrtXmlSaxClose(reader, &error);
    xrtXmlSaxDestroy(reader);
    xrtXmlOptionsInit(&options);
    options.tLimits.iMaxInputBytes = 4;
    reader = xrtXmlSaxOpenMemory(source, sizeof(source) - 1, &options, &error);
    ok = ok && !reader && error.iCode == XRT_XML_ERROR_LIMIT;
    xrtXmlSaxDestroy(reader);
    reader = xrtXmlSaxOpenMemory("<ok/>", 5, NULL, &error);
    ok = ok && reader && xrtXmlSaxNext(reader, &event, &error) == XRT_XML_NEXT_EVENT &&
        event && !strcmp(event->sName, "ok") && xrtXmlSaxClose(reader, &error);
    xrtXmlSaxDestroy(reader);
    xrtClearError();
    if (ok) puts("books=2\nnames=语言运行库\nlimit rejected=true; recovery=true\nXML catalog stream: OK");
    return ok ? 0 : 1;
}

#include "../xrt_xml_common.c"
#include "../xrt_xml_sax.c"
#include "../../_expat/vendor/expat/lib/xmlparse.c"
#include "../../_expat/vendor/expat/lib/xmlrole.c"
#include "../../_expat/vendor/expat/lib/xmltok.c"
#include "../../_expat/expat_random.c"
