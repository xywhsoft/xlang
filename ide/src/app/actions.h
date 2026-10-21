#ifndef XLANG_IDE_ACTIONS_H
#define XLANG_IDE_ACTIONS_H
#include "xui.h"
#include <stddef.h>
typedef struct xide_app xide_app;
typedef struct xide_actions xide_actions;
enum xide_action_id {
    XIDE_ACTION_RESET = 1, XIDE_ACTION_XLANG, XIDE_ACTION_C,
    XIDE_ACTION_THEME_CLASSIC = 10, XIDE_ACTION_THEME_GRAPHITE, XIDE_ACTION_THEME_PAPER, XIDE_ACTION_THEME_OCEAN,
    XIDE_ACTION_NEW_WORKSPACE=20, XIDE_ACTION_OPEN_WORKSPACE, XIDE_ACTION_SAVE_WORKSPACE, XIDE_ACTION_SAVE_WORKSPACE_AS,
    XIDE_ACTION_OPEN_PROJECT, XIDE_ACTION_ADD_PROJECT, XIDE_ACTION_REMOVE_PROJECT, XIDE_ACTION_SET_STARTUP,
    XIDE_ACTION_ADD_FILE, XIDE_ACTION_REMOVE_FILE, XIDE_ACTION_NEW_PROJECT, XIDE_ACTION_CONFIGURATIONS, XIDE_ACTION_APPLY_PROPERTIES, XIDE_ACTION_REFERENCES, XIDE_ACTION_RENAME_SELECTED_FILE, XIDE_ACTION_MIGRATE_PROJECT_MANIFEST, XIDE_ACTION_MIGRATE_PROJECT_DIRECTORY, XIDE_ACTION_IMPORT_LEGACY, XIDE_ACTION_RESOURCES, XIDE_ACTION_NEW_RESOURCE,
    XIDE_ACTION_NEW_FILE=40, XIDE_ACTION_OPEN_FILE, XIDE_ACTION_SAVE_FILE, XIDE_ACTION_SAVE_FILE_AS, XIDE_ACTION_SAVE_ALL_FILES, XIDE_ACTION_CLOSE_FILE, XIDE_ACTION_RELOAD_FILE,
    XIDE_ACTION_LANGUAGE_C, XIDE_ACTION_LANGUAGE_XLANG, XIDE_ACTION_LANGUAGE_TEXT, XIDE_ACTION_EDITOR_UNDO, XIDE_ACTION_EDITOR_REDO, XIDE_ACTION_EDITOR_FIND, XIDE_ACTION_EDITOR_REPLACE, XIDE_ACTION_EDITOR_COMPLETE, XIDE_ACTION_RENAME_FILE,
    XIDE_ACTION_BUILD=60, XIDE_ACTION_RUN, XIDE_ACTION_STOP, XIDE_ACTION_COMPILER, XIDE_ACTION_CLOSE_INPUT, XIDE_ACTION_TOOLCHAIN_INFO, XIDE_ACTION_CHECK_SAVED, XIDE_ACTION_CANCEL_CHECK, XIDE_ACTION_CHECK_CURRENT, XIDE_ACTION_DEFINITION,
    XIDE_ACTION_EDITOR_FONT=70, XIDE_ACTION_UI_FONT, XIDE_ACTION_RECOVERY, XIDE_ACTION_FILE_CHANGES, XIDE_ACTION_OUTLINE, XIDE_ACTION_HOVER, XIDE_ACTION_C_SERVER, XIDE_ACTION_EDITOR_GOTO, XIDE_ACTION_TEST,
    XIDE_ACTION_EDITOR_CUT=80, XIDE_ACTION_EDITOR_COPY, XIDE_ACTION_EDITOR_PASTE, XIDE_ACTION_EDITOR_SELECT_ALL,
    XIDE_ACTION_EDITOR_INDENT, XIDE_ACTION_EDITOR_OUTDENT, XIDE_ACTION_EDITOR_COMMENT,
    XIDE_ACTION_SOURCE_DECLARATION=90, XIDE_ACTION_SOURCE_DOCUMENT,
    XIDE_ACTION_BUILD_FILE, XIDE_ACTION_RUN_FILE
};
enum xide_action_result { XIDE_ACTION_UNKNOWN=-1, XIDE_ACTION_DISABLED=0, XIDE_ACTION_DISPATCHED=1 };
enum xide_shortcut_scope { XIDE_SHORTCUT_GLOBAL, XIDE_SHORTCUT_EDITOR, XIDE_SHORTCUT_WORKSPACE };
typedef struct xide_action_info {
    int id;
    const char *name,*title,*toolbarTitle,*shortcut;
    int key;
    uint32_t modifiers;
    enum xide_shortcut_scope shortcutScope;
    int codeCommand;
} xide_action_info;
typedef struct xide_action_state { int enabled,checked; const char* reason; } xide_action_state;
/* Immutable built-ins; stable names are independent of UI item numbers. */
size_t xideActionCount(void);
const xide_action_info* xideActionAt(size_t index);
const xide_action_info* xideActionFind(int id);
const xide_action_info* xideActionNamed(const char* name);
int xideActionsHasModal(xide_app* app);
xide_action_state xideActionQuery(xide_app* app,int action);
int xideActionExecute(xide_app* app,int action);
/* Init after the root exists; destroy after XUI destroys callbacks/widgets. */
int xideActionsInit(xide_app* app);
void xideActionsDestroy(xide_app* app);
void xideActionsRefresh(xide_app* app);
int xideActionsMenu(xide_app* app,xui_widget menu,const int* actions,size_t count);
int xideActionsToolbar(xide_app* app,xui_widget toolbar,const int* actions,size_t count);
int xideActionsButton(xide_app* app,xui_widget button,int action);
int xideActionsBindEditor(xide_app* app,xui_widget editor);
#endif
