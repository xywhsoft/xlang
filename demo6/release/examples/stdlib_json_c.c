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

    require_json(xrtStrComp(compact, "[1,2,3]", 0, FALSE) == 0);
    require_json(xrtStrIndexOf(pretty, 0, (str)"\n", 0, FALSE) >= 0);
    require_json(xrtStrIndexOf(objectText, 0, (str)"\"xlang\"", 0, FALSE) >= 0);
    require_json(written != 0);
    require_json(xrtStrIndexOf(loadedText, 0, (str)"\"stage\"", 0, FALSE) >= 0);
    require_json(deleted != 0);

    xvoUnref(values);
    xvoUnref(object);
    xvoUnref(loaded);
    xrtFree(compact);
    xrtFree(pretty);
    xrtFree(objectText);
    xrtFree(loadedText);

    printf("json=ok\n");
    xrtUnit();
    return 0;
}
