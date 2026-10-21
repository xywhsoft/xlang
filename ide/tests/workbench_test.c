#include "app/xide_app.h"
#include "theme/theme.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(expression) do { if (!(expression)) { fprintf(stderr,"workbench_test:%d: %s\n",__LINE__,#expression); failed=1; goto cleanup; } } while(0)
static int near(float a,float b) { return fabsf(a-b)<2; }
static int inspect(xide_app* app)
{
    xui_dock_window_info_t windows[XIDE_WINDOW_COUNT]={0};
    for(int i=0;i<XIDE_WINDOW_COUNT;++i) {
        windows[i].iSize=sizeof(windows[i]);
        if(xuiDockPanelGetWindowInfo(app->dock,app->windows[i],&windows[i])!=XUI_OK) return 0;
    }
    xui_dock_pane_info_t editor={0},help={0},left={0},workspace={0},properties={0};
    xui_dock_pane_info_t* panes[]={&editor,&help,&left,&workspace,&properties};
    int ids[]={windows[XIDE_XLANG_DEMO].iPane,windows[XIDE_HELP].iPane,windows[XIDE_COMMANDS].iPane,windows[XIDE_WORKSPACE].iPane,windows[XIDE_PROPERTIES].iPane};
    for(int i=0;i<5;++i) { panes[i]->iSize=sizeof(*panes[i]); if(xuiDockPanelGetPaneInfo(app->dock,ids[i],panes[i])!=XUI_OK) return 0; }
    if(!near(editor.tRect.fX,help.tRect.fX) || !near(editor.tRect.fW,help.tRect.fW)) return 0;
    if(help.tRect.fY<editor.tRect.fY+editor.tRect.fH || editor.tRect.fW<200) return 0;
    if(!near(left.tRect.fY,editor.tRect.fY) || !near(left.tRect.fY+left.tRect.fH,help.tRect.fY+help.tRect.fH)) return 0;
    if(!near(workspace.tRect.fX,properties.tRect.fX) || !near(workspace.tRect.fW,properties.tRect.fW)) return 0;
    if(workspace.iPane==properties.iPane || properties.tRect.fY<workspace.tRect.fY+workspace.tRect.fH) return 0;
    if(!near(properties.tRect.fY+properties.tRect.fH,help.tRect.fY+help.tRect.fH)) return 0;
    if(windows[XIDE_HELP].iRegion!=XUI_DOCK_PANEL_REGION_DOCUMENT || windows[XIDE_PROPERTIES].iRegion!=XUI_DOCK_PANEL_REGION_RIGHT) return 0;
    if(xuiDockPanelGetPaneActiveWindow(app->dock,help.iPane)!=app->windows[XIDE_HELP]) return 0;
    if(windows[XIDE_OUTLINE].iState==XUI_DOCK_PANEL_WINDOW_DOCKED && (windows[XIDE_OUTLINE].iRegion!=XUI_DOCK_PANEL_REGION_DOCUMENT || windows[XIDE_OUTLINE].iPane!=help.iPane)) return 0;
    return xuiDockPanelGetPaneCount(app->dock)==5;
}
static int defaultOutline(xide_app* app)
{ xui_dock_window_info_t info={.iSize=sizeof(info)}; return xuiDockPanelGetWindowInfo(app->dock,app->windows[XIDE_OUTLINE],&info)==XUI_OK && info.iState==XUI_DOCK_PANEL_WINDOW_DOCKED && info.iRegion==XUI_DOCK_PANEL_REGION_DOCUMENT && info.iPane==app->helpPane; }
int main(void)
{
    xide_app app={0}; int failed=0;
    app.viewportWidth=1280; app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK);
    CHECK(ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK);
    CHECK(xuiInputViewport(app.context,1280,800)==XUI_OK);
    CHECK(xideWorkbenchCreate(&app)==XUI_OK && defaultOutline(&app));
    for(int i=0;i<3;++i) {
        app.viewportWidth=i==0?1280:i==1?1600:900; app.viewportHeight=i==0?800:i==1?1000:600;
        CHECK(xuiInputViewport(app.context,(float)app.viewportWidth,(float)app.viewportHeight)==XUI_OK);
        xideWorkbenchLayout(&app); CHECK(xuiLayout(app.context)==XUI_OK); CHECK(inspect(&app));
    }
    for(int i=0;i<20;++i) {
        CHECK(xuiDockPanelHideWindow(app.dock,app.windows[XIDE_WORKSPACE])==XUI_OK);
        CHECK(xuiDockPanelFloatWindow(app.dock,app.windows[XIDE_C_DEMO],(xui_rect_t){200,100,400,280})==XUI_OK);
        CHECK(xideWorkbenchReset(&app)==XUI_OK && defaultOutline(&app)); CHECK(xuiLayout(app.context)==XUI_OK); CHECK(inspect(&app));
    }
    uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(app.editors[0]));
    int layoutChanges=xuiDockPanelGetLayoutChangeCount(app.dock);
    for(int i=0;i<IDE_THEME_COUNT;++i) {
        CHECK(xideWorkbenchApplyTheme(&app,i)==XUI_OK);
        CHECK(xuiLayout(app.context)==XUI_OK && inspect(&app));
        CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(app.editors[0]))==version);
        CHECK(xuiDockPanelGetLayoutChangeCount(app.dock)==layoutChanges);
    }
    xideWorkbenchShowHelp(&app,1); xideWorkbenchLayout(&app); CHECK(xuiLayout(app.context)==XUI_OK);
    CHECK(app.selectedCommand==1 && strstr(xuiCodeEditGetText(app.helpExample),"class Counter")!=NULL);
    CHECK(xuiWidgetGetLayoutType(app.helpContent)==XUI_LAYOUT_COLUMN);
    CHECK(xuiWidgetGetParent(app.helpExample)==app.helpContent);
    CHECK(xuiWidgetGetParent(app.helpDescription)==app.helpContent);
    CHECK(xuiWidgetGetRect(app.helpExample).fY>xuiWidgetGetRect(app.helpReturn).fY);
    xideDispatchAction(&app,XIDE_ACTION_C);
    CHECK(xuiDockPanelGetPaneActiveWindow(app.dock,app.documentPane)==app.windows[XIDE_C_DEMO]);
    xideDispatchAction(&app,XIDE_ACTION_XLANG);
    CHECK(xuiDockPanelGetPaneActiveWindow(app.dock,app.documentPane)==app.windows[XIDE_XLANG_DEMO]);
    CHECK(xuiDockPanelHideWindow(app.dock,app.windows[XIDE_HELP])==XUI_OK);
    xideWorkbenchShowHelp(&app,2);
    CHECK(xuiLayout(app.context)==XUI_OK && inspect(&app));
    for(int i=XIDE_HELP;i<=XIDE_OUTLINE;++i) CHECK(xuiDockPanelHideWindow(app.dock,app.windows[i])==XUI_OK);
    xideWorkbenchShowHelp(&app,0);
    CHECK(xuiLayout(app.context)==XUI_OK && inspect(&app));
    CHECK(xuiDockPanelHideWindow(app.dock,app.windows[XIDE_XLANG_DEMO])==XUI_OK);
    CHECK(xuiDockPanelHideWindow(app.dock,app.windows[XIDE_C_DEMO])==XUI_OK);
    xideDispatchAction(&app,XIDE_ACTION_C);
    CHECK(xuiLayout(app.context)==XUI_OK);
    xui_dock_window_info_t restored={0}; restored.iSize=sizeof(restored);
    CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_C_DEMO],&restored)==XUI_OK && restored.iState==XUI_DOCK_PANEL_WINDOW_DOCKED);
    CHECK(xideWorkbenchReset(&app)==XUI_OK && xuiLayout(app.context)==XUI_OK && inspect(&app));
    puts("workbench_test passed: real XUI controls at 3 sizes, center-only bottom, right vertical split, 20 hide/float/reset cycles, 4 themes and single-column command help");
cleanup:
    xideAppDestroy(&app);
    return failed;
}