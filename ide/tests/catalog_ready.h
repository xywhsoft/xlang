#ifndef XIDE_TEST_CATALOG_READY_H
#define XIDE_TEST_CATALOG_READY_H
#include "ui/catalog_view.h"
#include <windows.h>
/* Component fixtures drive their own frame loop. Only startup-specific tests
 * exercise the first-painted-frame gate through the real native frame path. */
static inline int xideTestCatalogReady(xide_app* app)
{
    int gate=app->startupAsync;app->startupAsync=0;
    ULONGLONG until=GetTickCount64()+30000;
    while(!xideCatalogViewSettled(app) && GetTickCount64()<until){xideCatalogViewPoll(app);Sleep(1);}
    app->startupAsync=gate;return xideCatalogViewSettled(app) && !app->frameError;
}
#endif
