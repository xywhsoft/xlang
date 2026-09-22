#define XRT_MODULE_JSON
#define XRT_MODULE_DIR
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static void require_json(bool bValue)
{
    if ( !bValue ) {
        fprintf(stderr, "json=fail\n");
        exit(1);
    }
}



typedef struct JsonVisitState {
    int Count;
    bool SawName;
} JsonVisitState;



static xjsonvisitaction visit_json(const xjsonevent* pEvent, ptr pUserData)
{
    JsonVisitState* pState = (JsonVisitState*)pUserData;

    if ( pEvent == NULL || pState == NULL ) {
        return XJSON_VISIT_FAIL;
    }
    pState->Count += 1;
    if (
        (pEvent->Type == XJSON_EVENT_STRING) &&
        pEvent->HasName &&
        (pEvent->Name.Size == 4u) &&
        (memcmp(pEvent->Name.Data, "name", 4u) == 0) &&
        (pEvent->Value.String.Size == 5u) &&
        (memcmp(pEvent->Value.String.Data, "xlang", 5u) == 0)
    ) {
        pState->SawName = true;
    }
    return XJSON_VISIT_NEXT;
}



int main(void)
{
    cstr sFilePath = "examples_out/example_stdlib_json.json";
    xvalue* pValues;
    xvalue* pObject;
    xvalue* pLoaded;
    str sCompact;
    str sPretty;
    str sObjectText;
    str sLoadedText;
	str sStreamed;
	xjsonreadconfig ReadConfig;
	xjsonwriteconfig Config;
    xjsonwriter* pWriter;
    JsonVisitState VisitState = { 0 };

    require_json(xrtDirCreateAll("examples_out"));
    (void)xrtFileDelete(sFilePath);

    require_json(xrtJsonValid(XRT_STR_LITERAL("{\"name\":\"xlang\",\"stage\":6}")));
    require_json(!xrtJsonValid(XRT_STR_LITERAL("{\"name\":")));

    pValues = xrtJsonParse(XRT_STR_LITERAL("[1,2,3]"));
    pObject = xrtJsonParse(XRT_STR_LITERAL("{\"name\":\"xlang\",\"stage\":6}"));
    require_json(pValues != NULL && pObject != NULL);
    sCompact = xrtJsonStringify(pValues, false, NULL);
    sPretty = xrtJsonStringify(pValues, true, NULL);
    sObjectText = xrtJsonStringify(pObject, false, NULL);
    require_json(sCompact != NULL && sPretty != NULL && sObjectText != NULL);
    require_json(xrtJsonStringifyFile(sFilePath, pObject, true));
    pLoaded = xrtJsonParseFile(sFilePath);
    require_json(pLoaded != NULL);
    sLoadedText = xrtJsonStringify(pLoaded, false, NULL);
    require_json(sLoadedText != NULL);

	xrtJsonReadConfigInit(&ReadConfig);
	require_json(
		xrtJsonVisit(
			XRT_STR_LITERAL("{\"name\":\"xlang\",\"items\":[1,true,null]}"),
			&ReadConfig,
            visit_json,
            &VisitState
        ) == XJSON_VISIT_DONE
    );

    xrtJsonWriteConfigInit(&Config);
    pWriter = xrtJsonWriterCreate(&Config);
    require_json(pWriter != NULL);
    require_json(xrtJsonWriterObject(pWriter));
    require_json(xrtJsonWriterName(pWriter, XRT_STR_LITERAL("name")));
    require_json(xrtJsonWriterString(pWriter, XRT_STR_LITERAL("xlang")));
    require_json(xrtJsonWriterName(pWriter, XRT_STR_LITERAL("items")));
    require_json(xrtJsonWriterArray(pWriter));
    require_json(xrtJsonWriterInt(pWriter, 1));
    require_json(xrtJsonWriterBool(pWriter, true));
    require_json(xrtJsonWriterNull(pWriter));
    require_json(xrtJsonWriterEnd(pWriter));
    require_json(xrtJsonWriterEnd(pWriter));
    require_json(xrtJsonWriterFinish(pWriter));
    sStreamed = xrtJsonWriterTake(pWriter, NULL);
    require_json(sStreamed != NULL);

    require_json(strcmp(sCompact, "[1,2,3]") == 0);
    require_json(strchr(sPretty, '\n') != NULL);
    require_json(strstr(sObjectText, "\"xlang\"") != NULL);
    require_json(strstr(sLoadedText, "\"stage\"") != NULL);
    require_json(VisitState.Count == 8 && VisitState.SawName);
    require_json(strcmp(sStreamed, "{\"name\":\"xlang\",\"items\":[1,true,null]}") == 0);

    require_json(xrtFileDelete(sFilePath));
    xrtJsonWriterFree(pWriter);
    xrtValueRelease(pValues);
    xrtValueRelease(pObject);
    xrtValueRelease(pLoaded);
    xrtFree(sCompact);
    xrtFree(sPretty);
    xrtFree(sObjectText);
    xrtFree(sLoadedText);
    xrtFree(sStreamed);

    printf("json=ok\n");
    return 0;
}
