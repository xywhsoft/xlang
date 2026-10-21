#include "ui/import_view.h"
#include "platform/dpi_proxy.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "app/xide_app.h"
#include "services/ui_font.h"
#include <math.h>
#include "lang/languages.h"
#include "theme/theme.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/configuration_view.h"
#include "ui/references_view.h"
#include "ui/resource_view.h"
#include "ui/toolchain_view.h"
#include "ui/language_view.h"
#include "ui/c_language_view.h"
#include "ui/catalog_view.h"
#include "ui/completion_view.h"
#include "ui/c_completion_view.h"
#include "ui/font_view.h"
#include "ui/goto_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include <stdio.h>
#include <string.h>
#define TRY(call) do { int result_ = (call); if (result_ != XUI_OK) { fprintf(stderr, "UI call failed at line %d: %s -> %d\n", __LINE__, #call, result_); return result_; } } while (0)

static const char* xlangSample =
    "#doc\n描述: xlang 语法演示，只读\n#end\n\n"
    "func add(int a, int b) as int\n    return a + b\nend\n\n"
    "func main(...) as int\n    int 总数 = add(1, 2)\n"
    "    if 总数 > 0 then\n        print(f\"总数 = {总数}\")\n    end\n    return 0\nend\n";
static const char* cSample =
    "/* C 语法演示，只读 */\n#include <stdio.h>\n\n"
    "static int add(int a, int b)\n{\n    return a + b;\n}\n\n"
    "int main(void)\n{\n    printf(\"sum = %d\\n\", add(1, 2));\n    return 0;\n}\n";

static int codeEditor(xide_app* app, xui_widget* widget, const char* text, const char* language)
{
    xui_code_edit_desc_t desc = {0}; desc.iSize = sizeof(desc);
    desc.pFont = app->codeFont; desc.sText = text; desc.bReadonly = 1;
    desc.bShowLineNumbers = 1; desc.bShowFoldMargin = 1; desc.iTabColumns = 4; desc.iIndentColumns = 4;
    TRY(xideDpiCodeEditCreate(app->context, widget, &desc));
    TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(*widget)));
    return xideConfigureEditorLanguage(*widget, language);
}
static int dockedPane(xide_app* app, int id)
{
    xui_dock_window_info_t info = {0}; info.iSize = sizeof(info);
    if (xuiDockPanelGetWindowInfo(app->dock, app->windows[id], &info) != XUI_OK ||
        info.iState != XUI_DOCK_PANEL_WINDOW_DOCKED) return -1;
    return info.iPane;
}
static int showEditor(xide_app* app, int id)
{
    int pane = dockedPane(app, id);
    if (pane < 0) {
        int other = id == XIDE_C_DEMO ? XIDE_XLANG_DEMO : XIDE_C_DEMO;
        pane = dockedPane(app, other);
        if (pane >= 0) TRY(xuiDockPanelDockWindowToPane(app->dock, app->windows[id], pane));
        else {
            int bottom = dockedPane(app, XIDE_HELP);
            if (bottom >= 0) TRY(xuiDockPanelDockWindowToPaneSide(app->dock, app->windows[id], bottom,
                                    XUI_DOCK_PANEL_SIDE_TOP, 0.72f, &pane));
            else TRY(xuiDockPanelDockWindow(app->dock, app->windows[id], XUI_DOCK_PANEL_REGION_DOCUMENT,
                                            XUI_DOCK_PANEL_SIDE_FILL, 0, &pane));
        }
    }
    app->documentPane = pane;
    return xuiDockPanelSetPaneActiveWindow(app->dock, pane, app->windows[id]);
}
static int showHelp(xide_app* app)
{
    int pane = dockedPane(app, XIDE_HELP);
    if (pane < 0) {
        for (int i = XIDE_PROBLEMS; i <= XIDE_OUTLINE && pane < 0; ++i) pane = dockedPane(app, i);
        if (pane >= 0) TRY(xuiDockPanelDockWindowToPane(app->dock, app->windows[XIDE_HELP], pane));
        else {
            int editor = dockedPane(app, XIDE_XLANG_DEMO);
            if (editor < 0) editor = dockedPane(app, XIDE_C_DEMO);
            if (editor < 0) { TRY(showEditor(app, XIDE_XLANG_DEMO)); editor = app->documentPane; }
            TRY(xuiDockPanelDockWindowToPaneSide(app->dock, app->windows[XIDE_HELP], editor,
                                                XUI_DOCK_PANEL_SIDE_BOTTOM, 0.28f, &pane));
        }
    }
    app->helpPane = pane;
    return xuiDockPanelSetPaneActiveWindow(app->dock, pane, app->windows[XIDE_HELP]);
}
void xideWorkbenchShowHelp(xide_app* app,int command)
{
    if(!app || command<0 || !xideCatalogViewSelect(app,(size_t)command)) return;
    showHelp(app);
}
static int addMenu(xide_app* app, const char* name, const int* items, int count)
{
    xui_widget menu = NULL;
    xui_menu_desc_t desc = {0}; desc.iSize = sizeof(desc); desc.pFont = app->uiFont; desc.pOwner = app->menuBar;
    TRY(xuiMenuCreate(app->context, &menu, &desc));
    TRY(xideActionsMenu(app,menu,items,(size_t)count));
    if(!strcmp(name,"文件")) app->fileMenu=menu;
    if(!strcmp(name,"项目")) app->projectMenu=menu;
    if(!strcmp(name,"编辑器")) app->languageMenu=menu;
    if(!strcmp(name,"编辑")) app->editMenu=menu;
    int added = xuiMenuBarAddItem(app->menuBar, name, menu, 0);
    return added < 0 ? added : XUI_OK;
}
static int createChrome(xide_app* app)
{
    xui_menubar_desc_t menu = {0}; menu.iSize = sizeof(menu); menu.pFont = app->uiFont;
    TRY(xuiMenuBarCreate(app->context, &app->menuBar, &menu)); TRY(xuiWidgetAddChild(app->root, app->menuBar));
    const int file[]={XIDE_ACTION_NEW_WORKSPACE,XIDE_ACTION_OPEN_WORKSPACE,XIDE_ACTION_OPEN_PROJECT,XIDE_ACTION_SAVE_WORKSPACE,XIDE_ACTION_SAVE_WORKSPACE_AS,0,XIDE_ACTION_BUILD,XIDE_ACTION_RUN,XIDE_ACTION_TEST,0,XIDE_ACTION_NEW_FILE,XIDE_ACTION_OPEN_FILE,XIDE_ACTION_SAVE_FILE,XIDE_ACTION_SAVE_FILE_AS,XIDE_ACTION_RENAME_FILE,XIDE_ACTION_SAVE_ALL_FILES,XIDE_ACTION_CLOSE_FILE,XIDE_ACTION_RELOAD_FILE,XIDE_ACTION_COMPILER,XIDE_ACTION_C_SERVER,XIDE_ACTION_TOOLCHAIN_INFO,XIDE_ACTION_CHECK_CURRENT,XIDE_ACTION_DEFINITION,XIDE_ACTION_HOVER,XIDE_ACTION_CHECK_SAVED,XIDE_ACTION_CANCEL_CHECK,XIDE_ACTION_STOP,XIDE_ACTION_CLOSE_INPUT};
    const int project[]={XIDE_ACTION_IMPORT_LEGACY,XIDE_ACTION_ADD_PROJECT,XIDE_ACTION_NEW_PROJECT,XIDE_ACTION_REMOVE_PROJECT,XIDE_ACTION_SET_STARTUP,XIDE_ACTION_ADD_FILE,XIDE_ACTION_REMOVE_FILE,XIDE_ACTION_RENAME_SELECTED_FILE,XIDE_ACTION_MIGRATE_PROJECT_MANIFEST,XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY,XIDE_ACTION_CONFIGURATIONS,XIDE_ACTION_REFERENCES,XIDE_ACTION_RESOURCES,XIDE_ACTION_NEW_RESOURCE,0,XIDE_ACTION_BUILD_FILE,XIDE_ACTION_RUN_FILE};
    const int view[]={XIDE_ACTION_OUTLINE,XIDE_ACTION_EDITOR_FONT,XIDE_ACTION_UI_FONT,XIDE_ACTION_RECOVERY,XIDE_ACTION_FILE_CHANGES,XIDE_ACTION_RESET,0,XIDE_ACTION_THEME_CLASSIC,XIDE_ACTION_THEME_GRAPHITE,XIDE_ACTION_THEME_PAPER,XIDE_ACTION_THEME_OCEAN};
    const int language[]={XIDE_ACTION_XLANG,XIDE_ACTION_C,0,XIDE_ACTION_LANGUAGE_C,XIDE_ACTION_LANGUAGE_XLANG,XIDE_ACTION_LANGUAGE_TEXT};
    TRY(addMenu(app, "文件", file, sizeof(file)/sizeof(*file)));
    TRY(addMenu(app, "项目", project, sizeof(project)/sizeof(*project)));
    const int edit[]={XIDE_ACTION_EDITOR_UNDO,XIDE_ACTION_EDITOR_REDO,0,XIDE_ACTION_EDITOR_CUT,XIDE_ACTION_EDITOR_COPY,XIDE_ACTION_EDITOR_PASTE,XIDE_ACTION_EDITOR_SELECT_ALL,0,XIDE_ACTION_EDITOR_INDENT,XIDE_ACTION_EDITOR_OUTDENT,XIDE_ACTION_EDITOR_COMMENT,0,XIDE_ACTION_EDITOR_FIND,XIDE_ACTION_EDITOR_REPLACE,XIDE_ACTION_EDITOR_GOTO,XIDE_ACTION_EDITOR_COMPLETE};
    TRY(addMenu(app,"编辑",edit,sizeof(edit)/sizeof(*edit)));
    TRY(addMenu(app, "视图", view, sizeof(view)/sizeof(*view)));
    TRY(addMenu(app, "编辑器", language, sizeof(language)/sizeof(*language)));
    xui_toolbar_desc_t toolbar = {0}; toolbar.iSize = sizeof(toolbar); toolbar.pFont = app->uiFont;
    TRY(xuiToolbarCreate(app->context, &app->toolbar, &toolbar)); TRY(xuiWidgetAddChild(app->root, app->toolbar));
    const int buttons[]={XIDE_ACTION_OPEN_FILE,XIDE_ACTION_SAVE_FILE,XIDE_ACTION_BUILD,XIDE_ACTION_RUN,XIDE_ACTION_XLANG,XIDE_ACTION_C,XIDE_ACTION_RESET,XIDE_ACTION_STOP};
    TRY(xideActionsToolbar(app,app->toolbar,buttons,sizeof(buttons)/sizeof(*buttons)));
    xui_statusbar_desc_t status = {0}; status.iSize = sizeof(status); status.pFont = app->uiFont;
    TRY(xuiStatusBarCreate(app->context, &app->statusBar, &status)); TRY(xuiWidgetAddChild(app->root, app->statusBar));
    if (xuiStatusBarAddText(app->statusBar, XUI_STATUSBAR_SECTION_LEFT, "工作台开发版本 · 语法演示为只读", 310, 0, 0) < 0) return XUI_ERROR;
    if (xuiStatusBarAddFlexibleSpacer(app->statusBar, XUI_STATUSBAR_SECTION_CENTER, 1) < 0) return XUI_ERROR;
    int added = xuiStatusBarAddText(app->statusBar, XUI_STATUSBAR_SECTION_RIGHT, "C / xlang  ·  UTF-8", 165, 0, 0);
    return added < 0 ? added : XUI_OK;
}
int xideWorkbenchReset(xide_app* app)
{
    for (int i = 0; i < XIDE_WINDOW_COUNT; ++i) TRY(xuiDockPanelHideWindow(app->dock, app->windows[i]));
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->window>=0) TRY(xuiDockPanelHideWindow(app->dock,app->documents.items[i]->window));
    TRY(xuiDockPanelSetRegionSize(app->dock, XUI_DOCK_PANEL_REGION_LEFT, XUI_DOCK_PANEL_SIZE_PIXEL, 190));
    TRY(xuiDockPanelSetRegionSize(app->dock, XUI_DOCK_PANEL_REGION_RIGHT, XUI_DOCK_PANEL_SIZE_PIXEL, 242));
    TRY(xuiDockPanelDockWindow(app->dock, app->windows[XIDE_XLANG_DEMO], XUI_DOCK_PANEL_REGION_DOCUMENT, XUI_DOCK_PANEL_SIDE_FILL, 0, &app->documentPane));
    TRY(xuiDockPanelDockWindowToPane(app->dock, app->windows[XIDE_C_DEMO], app->documentPane));
    /* The help pane splits DOCUMENT; using REGION_BOTTOM would span beneath both side docks. */
    float ratio = 180.0f / (app->viewportHeight > 300 ? app->viewportHeight - 92.0f : 400.0f);
    if (ratio > 0.45f) ratio = 0.45f;
    TRY(xuiDockPanelDockWindowToPaneSide(app->dock, app->windows[XIDE_HELP], app->documentPane, XUI_DOCK_PANEL_SIDE_BOTTOM, ratio, &app->helpPane));
    for (int i = XIDE_PROBLEMS; i <= XIDE_OUTLINE; ++i) TRY(xuiDockPanelDockWindowToPane(app->dock, app->windows[i], app->helpPane));
    TRY(xuiDockPanelDockWindow(app->dock, app->windows[XIDE_COMMANDS], XUI_DOCK_PANEL_REGION_LEFT, XUI_DOCK_PANEL_SIDE_FILL, 0, NULL));
    TRY(xuiDockPanelDockWindow(app->dock, app->windows[XIDE_WORKSPACE], XUI_DOCK_PANEL_REGION_RIGHT, XUI_DOCK_PANEL_SIDE_FILL, 0, &app->workspacePane));
    TRY(xuiDockPanelDockWindowToPaneSide(app->dock, app->windows[XIDE_PROPERTIES], app->workspacePane, XUI_DOCK_PANEL_SIDE_BOTTOM, 0.4f, &app->propertyPane));
    TRY(xuiDockPanelSetPaneActiveWindow(app->dock, app->documentPane, app->windows[XIDE_XLANG_DEMO]));
    TRY(showHelp(app));
    return xideEditorReset(app);
}
static int createWorkbench(xide_app* app)
{
    TRY(xuiWidgetCreate(app->context, &app->root)); TRY(xuiWidgetSetLayoutType(app->root, XUI_LAYOUT_MANUAL));
    TRY(xuiSetRootWidget(app->context, app->root)); TRY(xideActionsInit(app)); TRY(createChrome(app));
    xui_dock_panel_desc_t dock = {0}; dock.iSize = sizeof(dock); dock.pFont = app->uiFont;
    TRY(xuiDockPanelCreate(app->context, &app->dock, &dock)); TRY(xuiWidgetAddChild(app->root, app->dock));
    TRY(xideCatalogViewCreate(app));
    TRY(xideWorkspaceViewCreate(app));
    TRY(codeEditor(app, &app->editors[0], xlangSample, "xlang")); TRY(codeEditor(app, &app->editors[1], cSample, "c"));

    app->panels[XIDE_WORKSPACE] = app->workspace;
    app->panels[XIDE_PROPERTIES] = app->properties; app->panels[XIDE_XLANG_DEMO] = app->editors[0];
    app->panels[XIDE_C_DEMO] = app->editors[1];
    const char* titles[] = {"命令列表", "工作空间", "项目属性", "xlang 语法演示", "C 语法演示", "命令集成帮助", "问题", "构建输出", "终端", "任务", "文档大纲"};
    TRY(xideTaskViewCreate(app)); TRY(xideConfigurationViewCreate(app)); TRY(xideReferencesViewCreate(app)); TRY(xideResourceViewCreate(app)); TRY(xideToolchainViewCreate(app)); TRY(xideLanguageViewCreate(app)); TRY(xideCompletionViewCreate(app)); TRY(xideCCompletionViewCreate(app)); TRY(xideCLanguageViewCreate(app)); TRY(xideTemplateViewCreate(app)); TRY(xideImportViewCreate(app)); TRY(xideRenameViewCreate(app)); TRY(xideFontViewCreate(app)); TRY(xideRecoveryViewCreate(app)); TRY(xideFileChangesViewCreate(app)); TRY(xideGotoViewCreate(app));
    for (int i = 0; i < XIDE_WINDOW_COUNT; ++i) {
        TRY(xuiDockPanelAddWindow(app->dock, titles[i], app->panels[i], &app->windows[i]));
        TRY(xuiDockPanelSetWindowFlags(app->dock, app->windows[i], 1, 1));
    }
    TRY(xideWorkbenchReset(app)); TRY(xideEditorViewInit(app)); xideWorkbenchShowHelp(app, 0);
    if(app->catalog.count) xuiTreeViewSetSelected(app->commands, 100);
    xideWorkbenchLayout(app);
    return xidePlatformRegisterShortcuts(app);
}
int xideWorkbenchCreate(xide_app* app)
{
    /* Window and other native types register style properties during creation.
       Resolve those changes once, after the complete workbench exists. Keep
       EndUpdate paired even when an individual view fails to initialize. */
    int result=xuiStyleBeginUpdate(app->context);
    if(result!=XUI_OK) return result;
    result=createWorkbench(app);
    int end=xuiStyleEndUpdate(app->context);
    return result==XUI_OK ? end : result;
}
void xideWorkbenchLayout(xide_app* app)
{
    xideActionsRefresh(app);
    float width = (float)app->viewportWidth, height = (float)app->viewportHeight;
    xuiWidgetSetRect(app->root, (xui_rect_t){0,0,width,height});
    int menu=xideUiHeight(app,28),toolbar=xideUiHeight(app,36),status=xideUiHeight(app,28);
    xuiWidgetSetRect(app->menuBar, (xui_rect_t){0,0,width,menu});
    xuiWidgetSetRect(app->toolbar, (xui_rect_t){0,menu,width,toolbar});
    xuiWidgetSetRect(app->statusBar, (xui_rect_t){0,height-status,width,status});
    xuiStatusBarSetItemWidth(app->statusBar,0,fminf((float)xideUiHeight(app,310),width*0.58f));
    xuiStatusBarSetItemWidth(app->statusBar,2,fminf((float)xideUiHeight(app,165),width*0.42f));
    xuiWidgetSetRect(app->dock, (xui_rect_t){0,menu+toolbar,width,height-menu-toolbar-status});
    xideCatalogViewLayout(app);
    xideWorkspaceViewLayout(app); xideConfigurationViewLayout(app); xideReferencesViewLayout(app); xideResourceViewLayout(app); xideToolchainViewLayout(app); xideTemplateViewLayout(app); xideImportViewLayout(app); xideRenameViewLayout(app); xideFontViewLayout(app); xideRecoveryViewLayout(app); xideFileChangesViewLayout(app); xideGotoViewLayout(app); xideDialogLayoutAll(app); xideFileDialogLayout(app);
}
int xideWorkbenchApplyTheme(xide_app* app, int theme)
{
    TRY(ideThemeApply(app->context, theme)); app->theme = theme;
    for (int i = 0; i < 3; ++i) {
        xui_widget editor = i < 2 ? app->editors[i] : app->helpExample;
        int styles[] = {XUI_CODE_STYLE_DEFAULT, XUI_CODE_STYLE_KEYWORD, XUI_CODE_STYLE_TYPE, XUI_CODE_STYLE_NUMBER, XUI_CODE_STYLE_STRING, XUI_CODE_STYLE_CHAR, XUI_CODE_STYLE_COMMENT};
        ide_color_role roles[] = {IDE_TEXT, IDE_KEYWORD, IDE_TYPE, IDE_NUMBER, IDE_STRING, IDE_STRING, IDE_COMMENT};
        for (size_t j = 0; j < sizeof(styles)/sizeof(*styles); ++j) {
            xui_code_style_t style = {sizeof(style), ideThemeColor(theme, roles[j]), ideThemeColor(theme, IDE_FIELD), 0};
            TRY(xuiCodeEditSetStyle(editor, styles[j], &style));
        }
    }
    TRY(xideRecoveryViewTheme(app)); TRY(xideFileChangesViewTheme(app)); TRY(xideTaskViewTheme(app)); TRY(xideCatalogViewTheme(app));
    return xideEditorApplyTheme(app);
}
int xideWorkbenchAction(xide_app* app,int action)
{
    if (action == XIDE_ACTION_RESET) xideWorkbenchReset(app);
    else if (action == XIDE_ACTION_XLANG || action == XIDE_ACTION_C)
        showEditor(app, action == XIDE_ACTION_XLANG ? XIDE_XLANG_DEMO : XIDE_C_DEMO);
    else if (action >= XIDE_ACTION_THEME_CLASSIC && action <= XIDE_ACTION_THEME_OCEAN)
        xideWorkbenchApplyTheme(app, action-XIDE_ACTION_THEME_CLASSIC);
    else return 0;
    return 1;
}
