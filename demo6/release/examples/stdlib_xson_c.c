#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_xson(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "xson=fail\n");
        exit(1);
    }
}

typedef struct {
    int count;
    int stringIndex;
    int sawSet;
    int sawTime;
} XsonVisitState;

static bool visit_xson(const xrt_xson_event* event, void* userdata)
{
    XsonVisitState* state = (XsonVisitState*)userdata;

    if ( event == NULL || state == NULL ) {
        return false;
    }
    ++state->count;
    if ( event->type == XRT_XSON_EVENT_STRING &&
         event->text_size == 3 &&
         memcmp(event->text, "two", 3) == 0 &&
         event->has_index ) {
        state->stringIndex = (int)event->index;
    }
    if ( event->type == XRT_XSON_EVENT_SET_BEGIN &&
         event->key_size == 4 &&
         memcmp(event->key, "tags", 4) == 0 ) {
        state->sawSet = 1;
    }
    if ( event->type == XRT_XSON_EVENT_TIME &&
         event->key_size == 4 &&
         memcmp(event->key, "when", 4) == 0 &&
         event->text != NULL &&
         strstr(event->text, "2026-06-18") != NULL ) {
        state->sawTime = 1;
    }
    return true;
}

static bool stop_xson(const xrt_xson_event* event, void* userdata)
{
    (void)userdata;
    return event != NULL && event->depth < 1u;
}

int main(void)
{
    const char* filePath = "examples_out/stdlib_xson_example.xson";
    const char* visitSource = "{\"items\":list[2:\"two\"],\"tags\":set{1,2},\"when\":time(2026-06-18 00:00:00)}";
    xvalue config;
    xvalue copy;
    xvalue fileCopy;
    str compact;
    str pretty;
    str jsonText;
    str fileText;
    xson_options decodeOptions;
    xson_options prettyOptions;
    xxsonwriter writer;
    str writerText;
    XsonVisitState visitState = { 0, -1, 0, 0 };
    XsonVisitState valueVisitState = { 0, -1, 0, 0 };

    xrtInit();
    xrtDirCreateAll((str)"examples_out");
    xrtFileDelete((str)filePath);

    decodeOptions = xrtXsonOptionsDefault();
    decodeOptions.ignore_unsupported_decode = TRUE;
    prettyOptions = xrtXsonOptionsDefault();
    prettyOptions.pretty = TRUE;

    require_xson(xrtXsonValidWithOptions("{\"name\":\"xlang\",\"level\":6,\"enabled\":true}", 0, NULL));
    require_xson(!xrtXsonValidWithOptions("{\"keep\":1,\"skip\":ptr(123),\"after\":2}", 0, NULL));
    require_xson(xrtXsonValidWithOptions("{\"keep\":1,\"skip\":ptr(123),\"after\":2}", 0, &decodeOptions));

    config = xrtParseXSONWithOptions("{\"name\":\"xlang\",\"level\":6,\"enabled\":true}", 0, NULL);
    compact = (str)xrtStringifyXSONWithOptions(config, NULL, NULL);
    pretty = (str)xrtStringifyXSONWithOptions(config, &prettyOptions, NULL);
    copy = xrtParseXSONWithOptions((const char*)compact, 0, NULL);
    jsonText = xrtStringifyJSON(copy, FALSE, NULL);
    require_xson(xrtStringifyXSONFileWithOptions(filePath, config, &prettyOptions) != 0);
    fileCopy = xrtParseXSONFileWithOptions(filePath, &prettyOptions);
    fileText = (str)xrtStringifyXSONWithOptions(fileCopy, NULL, NULL);
    xrtFileDelete((str)filePath);

    require_xson(xrtStrIndexOf(compact, 0, (str)"xlang", 0, FALSE) >= 0);
    require_xson(xrtStrIndexOf(pretty, 0, (str)"\n", 0, FALSE) >= 0);
    require_xson(xrtStrIndexOf(jsonText, 0, (str)"\"level\"", 0, FALSE) >= 0);
    require_xson(xrtStrIndexOf(fileText, 0, (str)"enabled", 0, FALSE) >= 0);

    writer = xrtXsonWriterCreateWithOptions(&prettyOptions);
    require_xson(writer != NULL);
    require_xson(xrtXsonWriterBeginDict(writer));
    require_xson(xrtXsonWriterStringKey(writer, "name", "xlang"));
    require_xson(xrtXsonWriterTimeKey(writer, "created", xrtDateTimeSerial(2026, 6, 18, 0, 0, 0)));
    require_xson(xrtXsonWriterEndDict(writer));
    writerText = xrtXsonWriterFinish(writer, NULL);
    require_xson(writerText != NULL);
    require_xson(xrtStrIndexOf(writerText, 0, (str)"time(", 0, FALSE) >= 0);
    require_xson(xrtStrIndexOf(writerText, 0, (str)"\n", 0, FALSE) >= 0);
    xrtFree(writerText);

    require_xson(xrtXsonVisitWithOptions(visitSource, 0, &prettyOptions, visit_xson, &visitState) == 0);
    require_xson(visitState.count >= 9);
    require_xson(visitState.stringIndex == 2);
    require_xson(visitState.sawSet);
    require_xson(visitState.sawTime);
    require_xson(xrtXsonVisitWithOptions(visitSource, 0, &prettyOptions, stop_xson, NULL) == 1);
    require_xson(xrtXsonVisitWithOptions("{bad}", 0, &prettyOptions, visit_xson, &visitState) == -1);
    require_xson(xrtXsonVisitValue(config, visit_xson, &valueVisitState) == 0);
    require_xson(valueVisitState.count >= 4);

    xvoUnref(config);
    xvoUnref(copy);
    xvoUnref(fileCopy);
    xrtFree(compact);
    xrtFree(pretty);
    xrtFree(jsonText);
    xrtFree(fileText);

    printf("xson=ok\n");
    xrtUnit();
    return 0;
}
