#define XRT_MODULE_XSON
#define XRT_MODULE_JSON
#define XRT_MODULE_DIR
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static void require_xson(bool bValue)
{
    if ( !bValue ) {
        fprintf(stderr, "xson=fail\n");
        exit(1);
    }
}



typedef struct XsonVisitState {
    int Count;
    int StringIndex;
    bool SawSet;
    bool SawTime;
} XsonVisitState;



static xxsonvisitaction visit_xson(const xxsonevent* pEvent, ptr pUserData)
{
    XsonVisitState* pState = (XsonVisitState*)pUserData;

    if ( pEvent == NULL || pState == NULL ) {
        return XXSON_VISIT_FAIL;
    }
    pState->Count += 1;
    if (
        (pEvent->Type == XXSON_EVENT_STRING) &&
        (pEvent->Key.Type == XVALUE_KEY_INT) &&
        (pEvent->Value.String.Size == 3u) &&
        (memcmp(pEvent->Value.String.Data, "two", 3u) == 0)
    ) {
        pState->StringIndex = (int)pEvent->Key.Integer;
    }
    if (
        (pEvent->Type == XXSON_EVENT_SET_BEGIN) &&
        (pEvent->Key.Type == XVALUE_KEY_STRING) &&
        (pEvent->Key.String.Size == 4u) &&
        (memcmp(pEvent->Key.String.Data, "tags", 4u) == 0)
    ) {
        pState->SawSet = true;
    }
    if (
        (pEvent->Type == XXSON_EVENT_TIME) &&
        (pEvent->Key.Type == XVALUE_KEY_STRING) &&
        (pEvent->Key.String.Size == 4u) &&
        (memcmp(pEvent->Key.String.Data, "when", 4u) == 0)
    ) {
        pState->SawTime = true;
    }
    return XXSON_VISIT_NEXT;
}



static xxsonvisitaction stop_xson(const xxsonevent* pEvent, ptr pUserData)
{
    (void)pUserData;
    return pEvent != NULL && pEvent->Depth < 1u ?
        XXSON_VISIT_NEXT : XXSON_VISIT_STOP;
}



int main(void)
{
    cstr sFilePath = "examples_out/stdlib_xson_example.xson";
    xstrview VisitSource = XRT_STR_LITERAL(
        "{\"items\":intmap{2:\"two\"},\"tags\":set[1,2],"
        "\"when\":time(\"2026-06-18T00:00:00Z\")}"
    );
    xvalue* pConfig;
    xvalue* pCopy;
    xvalue* pFileCopy;
    str sCompact;
    str sPretty;
    str sJsonText;
    str sFileText;
    str sWriterText;
    xxsonreadconfig ReadConfig;
    xxsonwriteconfig WriteConfig;
    xxsonwriter* pWriter;
    xtime Created;
    XsonVisitState VisitState = { 0, -1, false, false };

    require_xson(xrtDirCreateAll("examples_out"));
    (void)xrtFileDelete(sFilePath);

    require_xson(xrtXsonValid(XRT_STR_LITERAL("{\"name\":\"xlang\",\"level\":6}")));
    require_xson(!xrtXsonValid(XRT_STR_LITERAL("{\"skip\":ptr(\"123\")}")));
    xrtXsonReadConfigInit(&ReadConfig);
    ReadConfig.Flags |= XXSON_READ_CUSTOM;
    require_xson(
        xrtXsonVisit(
            XRT_STR_LITERAL("{\"skip\":ptr(\"123\")}"),
            &ReadConfig,
            visit_xson,
            &VisitState
        ) == XXSON_VISIT_DONE
    );

    pConfig = xrtXsonParse(XRT_STR_LITERAL("{\"name\":\"xlang\",\"level\":6,\"enabled\":true}"));
    require_xson(pConfig != NULL);
    sCompact = xrtXsonStringify(pConfig, false, NULL);
    sPretty = xrtXsonStringify(pConfig, true, NULL);
    pCopy = xrtXsonParse((xstrview){ sCompact, strlen(sCompact) });
    sJsonText = xrtJsonStringify(pCopy, false, NULL);
    require_xson(sCompact != NULL && sPretty != NULL && pCopy != NULL && sJsonText != NULL);
    require_xson(xrtXsonStringifyFile(sFilePath, pConfig, true));
    pFileCopy = xrtXsonParseFile(sFilePath);
    require_xson(pFileCopy != NULL);
    sFileText = xrtXsonStringify(pFileCopy, false, NULL);
    require_xson(sFileText != NULL);

    xrtXsonWriteConfigInit(&WriteConfig);
    WriteConfig.Flags |= XXSON_WRITE_PRETTY;
    pWriter = xrtXsonWriterCreate(&WriteConfig);
    require_xson(pWriter != NULL);
    require_xson(xrtDateTime(2026, 6, 18, 0, 0, 0, 0, &Created));
    require_xson(xrtXsonWriterObject(pWriter));
    require_xson(xrtXsonWriterName(pWriter, XRT_STR_LITERAL("name")));
    require_xson(xrtXsonWriterString(pWriter, XRT_STR_LITERAL("xlang")));
    require_xson(xrtXsonWriterName(pWriter, XRT_STR_LITERAL("created")));
    require_xson(xrtXsonWriterTime(pWriter, Created));
    require_xson(xrtXsonWriterEnd(pWriter));
    require_xson(xrtXsonWriterFinish(pWriter));
    sWriterText = xrtXsonWriterTake(pWriter, NULL);
    require_xson(sWriterText != NULL);

    VisitState.Count = 0;
    VisitState.StringIndex = -1;
    VisitState.SawSet = false;
    VisitState.SawTime = false;
	require_xson(xrtXsonVisit(VisitSource, &ReadConfig, visit_xson, &VisitState) == XXSON_VISIT_DONE);
    require_xson(VisitState.Count >= 9);
    require_xson(VisitState.StringIndex == 2);
    require_xson(VisitState.SawSet && VisitState.SawTime);
	require_xson(xrtXsonVisit(VisitSource, &ReadConfig, stop_xson, NULL) == XXSON_VISIT_STOPPED);
	require_xson(xrtXsonVisit(XRT_STR_LITERAL("{bad}"), &ReadConfig, visit_xson, &VisitState) == XXSON_VISIT_ERROR);

    require_xson(strstr(sCompact, "xlang") != NULL);
    require_xson(strchr(sPretty, '\n') != NULL);
    require_xson(strstr(sJsonText, "\"level\"") != NULL);
    require_xson(strstr(sFileText, "enabled") != NULL);
    require_xson(strstr(sWriterText, "time(") != NULL);

    require_xson(xrtFileDelete(sFilePath));
    xrtXsonWriterFree(pWriter);
    xrtValueRelease(pConfig);
    xrtValueRelease(pCopy);
    xrtValueRelease(pFileCopy);
    xrtFree(sCompact);
    xrtFree(sPretty);
    xrtFree(sJsonText);
    xrtFree(sFileText);
    xrtFree(sWriterText);

    printf("xson=ok\n");
    return 0;
}
