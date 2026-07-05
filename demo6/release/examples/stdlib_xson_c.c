#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

static void require_xson(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "xson=fail\n");
        exit(1);
    }
}

int main(void)
{
    const char* filePath = "examples_out/stdlib_xson_example.xson";
    xvalue config;
    xvalue copy;
    xvalue fileCopy;
    str compact;
    str pretty;
    str jsonText;
    str fileText;

    xrtInit();
    xrtDirCreateAll((str)"examples_out");
    xrtFileDelete((str)filePath);

    require_xson(xrtXsonValid((str)"{\"name\":\"xlang\",\"level\":6,\"enabled\":true}", 0, 0));
    require_xson(!xrtXsonValid((str)"{\"keep\":1,\"skip\":ptr(123),\"after\":2}", 0, 0));
    require_xson(xrtXsonValid((str)"{\"keep\":1,\"skip\":ptr(123),\"after\":2}", 0, XSON_F_IGNORE_UNSUPPORTED_DECODE));

    config = xrtParseXSON((str)"{\"name\":\"xlang\",\"level\":6,\"enabled\":true}", 0);
    compact = xrtStringifyXSON(config, FALSE, 0, NULL);
    pretty = xrtStringifyXSON(config, TRUE, 0, NULL);
    copy = xrtParseXSON(compact, 0);
    jsonText = xrtStringifyJSON(copy, FALSE, NULL);
    require_xson(xrtStringifyXSON_File((str)filePath, config, FALSE, 0) != 0);
    fileCopy = xrtParseXSON_File((str)filePath);
    fileText = xrtStringifyXSON(fileCopy, FALSE, 0, NULL);
    xrtFileDelete((str)filePath);

    require_xson(xrtStrIndexOf(compact, 0, (str)"xlang", 0, FALSE) >= 0);
    require_xson(xrtStrIndexOf(pretty, 0, (str)"\n", 0, FALSE) >= 0);
    require_xson(xrtStrIndexOf(jsonText, 0, (str)"\"level\"", 0, FALSE) >= 0);
    require_xson(xrtStrIndexOf(fileText, 0, (str)"enabled", 0, FALSE) >= 0);

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
