#define XRT_MODULE_XLON
#define XRT_MODULE_JSON
#define XRT_MODULE_DIR
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static void require_xlon(bool bValue)
{
    if ( !bValue ) {
        fprintf(stderr, "xlon=fail\n");
        exit(1);
    }
}



typedef struct XlonVisitState {
    int Count;
    int StringIndex;
    bool SawSet;
    bool SawTime;
} XlonVisitState;



static xxlonvisitaction visit_xlon(const xxlonevent* pEvent, ptr pUserData)
{
    XlonVisitState* pState = (XlonVisitState*)pUserData;

    if ( pEvent == NULL || pState == NULL ) {
        return XXLON_VISIT_FAIL;
    }
    pState->Count += 1;
    if (
        (pEvent->Type == XXLON_EVENT_STRING) &&
        (pEvent->Key.Type == XVALUE_KEY_INT) &&
        (pEvent->Value.String.Size == 3u) &&
        (memcmp(pEvent->Value.String.Data, "two", 3u) == 0)
    ) {
        pState->StringIndex = (int)pEvent->Key.Integer;
    }
    if (
        (pEvent->Type == XXLON_EVENT_SET_BEGIN) &&
        (pEvent->Key.Type == XVALUE_KEY_STRING) &&
        (pEvent->Key.String.Size == 4u) &&
        (memcmp(pEvent->Key.String.Data, "tags", 4u) == 0)
    ) {
        pState->SawSet = true;
    }
    if (
        (pEvent->Type == XXLON_EVENT_TIME) &&
        (pEvent->Key.Type == XVALUE_KEY_STRING) &&
        (pEvent->Key.String.Size == 4u) &&
        (memcmp(pEvent->Key.String.Data, "when", 4u) == 0)
    ) {
        pState->SawTime = true;
    }
    return XXLON_VISIT_NEXT;
}



static xxlonvisitaction stop_xlon(const xxlonevent* pEvent, ptr pUserData)
{
    (void)pUserData;
    return pEvent != NULL && pEvent->Depth < 1u ?
        XXLON_VISIT_NEXT : XXLON_VISIT_STOP;
}



int main(void)
{
    cstr sFilePath = "examples_out/stdlib_xlon_example.xlon";
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
    xxlonreadconfig ReadConfig;
    xxlonwriteconfig WriteConfig;
    xxlonwriter* pWriter;
    xtime Created;
    XlonVisitState VisitState = { 0, -1, false, false };

    require_xlon(xrtDirCreateAll("examples_out"));
    (void)xrtFileDelete(sFilePath);

    require_xlon(xrtXlonValid(XRT_STR_LITERAL("{\"name\":\"xlang\",\"level\":6}")));
    require_xlon(!xrtXlonValid(XRT_STR_LITERAL("{\"skip\":ptr(\"123\")}")));
    xrtXlonReadConfigInit(&ReadConfig);
    ReadConfig.Flags |= XXLON_READ_CUSTOM;
    require_xlon(
        xrtXlonVisit(
            XRT_STR_LITERAL("{\"skip\":ptr(\"123\")}"),
            &ReadConfig,
            visit_xlon,
            &VisitState
        ) == XXLON_VISIT_DONE
    );

    pConfig = xrtXlonParse(XRT_STR_LITERAL("{\"name\":\"xlang\",\"level\":6,\"enabled\":true}"));
    require_xlon(pConfig != NULL);
    sCompact = xrtXlonStringify(pConfig, false, NULL);
    sPretty = xrtXlonStringify(pConfig, true, NULL);
    pCopy = xrtXlonParse((xstrview){ sCompact, strlen(sCompact) });
    sJsonText = xrtJsonStringify(pCopy, false, NULL);
    require_xlon(sCompact != NULL && sPretty != NULL && pCopy != NULL && sJsonText != NULL);
    require_xlon(xrtXlonStringifyFile(sFilePath, pConfig, true));
    pFileCopy = xrtXlonParseFile(sFilePath);
    require_xlon(pFileCopy != NULL);
    sFileText = xrtXlonStringify(pFileCopy, false, NULL);
    require_xlon(sFileText != NULL);

    xrtXlonWriteConfigInit(&WriteConfig);
    WriteConfig.Flags |= XXLON_WRITE_PRETTY;
    pWriter = xrtXlonWriterCreate(&WriteConfig);
    require_xlon(pWriter != NULL);
    require_xlon(xrtDateTime(2026, 6, 18, 0, 0, 0, 0, &Created));
    require_xlon(xrtXlonWriterObject(pWriter));
    require_xlon(xrtXlonWriterName(pWriter, XRT_STR_LITERAL("name")));
    require_xlon(xrtXlonWriterString(pWriter, XRT_STR_LITERAL("xlang")));
    require_xlon(xrtXlonWriterName(pWriter, XRT_STR_LITERAL("created")));
    require_xlon(xrtXlonWriterTime(pWriter, Created));
    require_xlon(xrtXlonWriterEnd(pWriter));
    require_xlon(xrtXlonWriterFinish(pWriter));
    sWriterText = xrtXlonWriterTake(pWriter, NULL);
    require_xlon(sWriterText != NULL);

    VisitState.Count = 0;
    VisitState.StringIndex = -1;
    VisitState.SawSet = false;
    VisitState.SawTime = false;
	require_xlon(xrtXlonVisit(VisitSource, &ReadConfig, visit_xlon, &VisitState) == XXLON_VISIT_DONE);
    require_xlon(VisitState.Count >= 9);
    require_xlon(VisitState.StringIndex == 2);
    require_xlon(VisitState.SawSet && VisitState.SawTime);
	require_xlon(xrtXlonVisit(VisitSource, &ReadConfig, stop_xlon, NULL) == XXLON_VISIT_STOPPED);
	require_xlon(xrtXlonVisit(XRT_STR_LITERAL("{bad}"), &ReadConfig, visit_xlon, &VisitState) == XXLON_VISIT_ERROR);

    require_xlon(strstr(sCompact, "xlang") != NULL);
    require_xlon(strchr(sPretty, '\n') != NULL);
    require_xlon(strstr(sJsonText, "\"level\"") != NULL);
    require_xlon(strstr(sFileText, "enabled") != NULL);
    require_xlon(strstr(sWriterText, "time(") != NULL);

    require_xlon(xrtFileDelete(sFilePath));
    xrtXlonWriterFree(pWriter);
    xrtValueRelease(pConfig);
    xrtValueRelease(pCopy);
    xrtValueRelease(pFileCopy);
    xrtFree(sCompact);
    xrtFree(sPretty);
    xrtFree(sJsonText);
    xrtFree(sFileText);
    xrtFree(sWriterText);

    printf("xlon=ok\n");
    return 0;
}
