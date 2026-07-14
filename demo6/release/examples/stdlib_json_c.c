#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

static void require_json(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "json=fail\n");
        exit(1);
    }
}

typedef struct {
    int count;
    int sawName;
} JsonVisitState;

static bool visit_json(const xrt_json_event* event, void* userdata)
{
    JsonVisitState* state = (JsonVisitState*)userdata;
    ++state->count;
    if ( event->type == XRT_JSON_EVENT_STRING && event->key_size == 4 &&
         memcmp(event->key, "name", 4) == 0 && event->text_size == 5 &&
         memcmp(event->text, "xlang", 5) == 0 ) {
        state->sawName = 1;
    }
    return true;
}

int main(void)
{
    const char* filePath = "examples_out/example_stdlib_json.json";
    xvalue values;
    xvalue object;
    xvalue loaded;
    str compact;
    str pretty;
    str objectText;
    str loadedText;
    int written;
    int deleted;
    JsonVisitState visitState = { 0 };
    xjsonwriter writer;
    char* streamed;

    xrtInit();
    xrtDirCreateAll((str)"examples_out");
    xrtFileDelete((str)filePath);

    require_json(xrtJsonValid((str)"{\"name\":\"xlang\",\"stage\":6}", 0));
    require_json(!xrtJsonValid((str)"{\"name\":", 0));

    values = xrtParseJSON((str)"[1,2,3]", 0);
    compact = xrtStringifyJSON(values, FALSE, NULL);
    pretty = xrtStringifyJSON(values, TRUE, NULL);
    object = xrtParseJSON((str)"{\"name\":\"xlang\",\"stage\":6}", 0);
    objectText = xrtStringifyJSON(object, FALSE, NULL);
    written = xrtStringifyJSON_File((str)filePath, object, TRUE);
    loaded = xrtParseJSON_File((str)filePath);
    loadedText = xrtStringifyJSON(loaded, FALSE, NULL);
    deleted = xrtFileDelete((str)filePath);

    require_json(xrtJsonVisit("{\"name\":\"xlang\",\"items\":[1,true,null]}", 0,
        visit_json, &visitState) == 0);
    writer = xrtJsonWriterCreate(FALSE);
    require_json(writer != NULL);
    require_json(xrtJsonWriterBeginObject(writer));
    require_json(xrtJsonWriterStringKey(writer, "name", "xlang"));
    require_json(xrtJsonWriterBeginArrayKey(writer, "items"));
    require_json(xrtJsonWriterInt(writer, 1));
    require_json(xrtJsonWriterBool(writer, TRUE));
    require_json(xrtJsonWriterNull(writer));
    require_json(xrtJsonWriterEndArray(writer));
    require_json(xrtJsonWriterEndObject(writer));
    streamed = xrtJsonWriterFinish(writer, NULL);

    require_json(xrtStrComp(compact, "[1,2,3]", 0, FALSE) == 0);
    require_json(xrtStrIndexOf(pretty, 0, (str)"\n", 0, FALSE) >= 0);
    require_json(xrtStrIndexOf(objectText, 0, (str)"\"xlang\"", 0, FALSE) >= 0);
    require_json(written != 0);
    require_json(xrtStrIndexOf(loadedText, 0, (str)"\"stage\"", 0, FALSE) >= 0);
    require_json(deleted != 0);
    require_json(visitState.count == 8 && visitState.sawName);
    require_json(streamed != NULL && strcmp(streamed, "{\"name\":\"xlang\",\"items\":[1,true,null]}") == 0);

    xvoUnref(values);
    xvoUnref(object);
    xvoUnref(loaded);
    xrtFree(compact);
    xrtFree(pretty);
    xrtFree(objectText);
    xrtFree(loadedText);
    xrtFree(streamed);
    xrtJsonWriterDestroy(writer);

    printf("json=ok\n");
    xrtUnit();
    return 0;
}
