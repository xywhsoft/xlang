#include "app/xide_app.h"
#include "ui/catalog_view.h"
#include "ui/editor_view.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
static int checks;
#define CHECK(e) do {++checks;if(!(e)){fprintf(stderr,"startup_catalog_view_test:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
static int loaded(xide_app* a)
{
    ULONGLONG until=GetTickCount64()+30000;
    while(!xideCatalogViewSettled(a) && GetTickCount64()<until){xideEditorPoll(a);Sleep(1);}
    return xideCatalogViewSettled(a);
}
int main(int argc,char** argv)
{
    xide_app a={0};int failed=0;xide_model_error error={0};char backup[32768];
    CHECK(argc==2);snprintf(backup,sizeof(backup),"%s.moved",argv[1]);a.hidden=1;a.startupAsync=1;a.catalogPath=argv[1];a.viewportWidth=1280;a.viewportHeight=800;
    CHECK(xuiCreate(&a.context)==XUI_OK && ideThemeInit(a.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(a.context,1280,800)==XUI_OK);
    CHECK(xideWorkbenchCreate(&a)==XUI_OK);
    CHECK(!a.catalog.count && !xideCatalogViewSettled(&a));
    const xui_tree_view_node_t* first=xuiTreeViewGetNode(a.commands,0);CHECK(first && !strcmp(first->sText,"【命令列表加载中 ...】"));
    /* The first frame is allowed to paint even if the worker is already done. */
    xideEditorPoll(&a);CHECK(!a.catalog.count);
    xide_document* d=xideDocumentNew(&a.documents,"xlang",&error);CHECK(d && xideEditorShow(&a,d)==XUI_OK);
    CHECK(xuiSetFocusWidget(a.context,d->editor)==XUI_OK && xuiInputTextEx(a.context,'x',NULL)==XUI_OK && !strcmp(xuiCodeEditGetText(d->editor),"x"));
    CHECK(xideCatalogViewSearch(&a,"math sqrt")==XUI_OK);a.initialCommand="standard:math.sqrt:8a5dc87fdb93b619";a.frames=1;
    CHECK(loaded(&a) && a.catalog.count>19000 && !a.frameError && !a.initialCommand);
    CHECK(xideCatalogVisibleCount(&a)>0 && xideCatalogVisibleCount(&a)<20 && !strcmp(a.catalog.entries[a.selectedCommand].name,"math.sqrt"));
    CHECK(xuiGetFocusWidget(a.context)==d->editor && !strcmp(xuiCodeEditGetText(d->editor),"x"));
    double load=0,publish=0;xideCatalogViewLoadStats(&a,&load,&publish);printf("startup catalog: load %.0f ms background, publish %.3f ms UI\n",load,publish);
    CHECK(rename(argv[1],backup)==0);CHECK(xideCatalogViewReload(&a) && loaded(&a));
    CHECK(a.catalog.count>19000 && !strcmp(a.catalog.entries[a.selectedCommand].name,"math.sqrt"));
    CHECK(rename(backup,argv[1])==0);CHECK(xideCatalogViewReload(&a) && xideCatalogViewReload(&a) && loaded(&a));
    CHECK(a.catalog.count>19000 && !strcmp(a.catalog.entries[a.selectedCommand].name,"math.sqrt"));
    CHECK(xideCatalogViewSearch(&a,"")==XUI_OK && xideCatalogViewReload(&a) && loaded(&a));
    CHECK(xideCatalogVisibleCount(&a)==a.catalog.count);
    xideCatalogViewLoadStats(&a,&load,&publish);printf("full catalog: load %.0f ms background, publish %.3f ms UI\n",load,publish);
    /* Destruction is safe with a replacement still running. */
    CHECK(xideCatalogViewReload(&a));
cleanup:
    xideAppDestroy(&a);
    if(argc==2 && GetFileAttributesA(backup)!=INVALID_FILE_ATTRIBUTES)rename(backup,argv[1]);
    if(!failed)printf("startup_catalog_view_test passed: %d checks, first-frame placeholder, editing, pending query, CLI selection, failed reload and close during load\n",checks);
    return failed;
}
