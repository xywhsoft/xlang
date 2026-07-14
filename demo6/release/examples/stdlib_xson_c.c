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
    xson_options decodeOptions;
    xson_options prettyOptions;

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
