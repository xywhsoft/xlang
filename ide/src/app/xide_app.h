#ifndef XLANG_IDE_APP_H
#define XLANG_IDE_APP_H
#include "app_xrt.h"
#include "app/actions.h"
#include "xge.h"
#include "xui.h"
#include "ui/dialog.h"
#include "ui/message_box.h"
#include "ui/file_dialog.h"
#include "model/workspace.h"
#include "editor/documents.h"
#include "services/tasks.h"
#include "services/command_catalog.h"
#include "platform/settings.h"
#include "platform/dpi_proxy.h"
#include "platform/runtime_failure.h"
typedef struct xide_workspace_view xide_workspace_view;
typedef struct xide_task_view xide_task_view;
typedef struct xide_main_toolbar xide_main_toolbar;
typedef struct xide_configuration_view xide_configuration_view;
typedef struct xide_references_view xide_references_view;
typedef struct xide_resource_view xide_resource_view;
typedef struct xide_toolchain_view xide_toolchain_view;
typedef struct xide_language_view xide_language_view;
typedef struct xide_completion_view xide_completion_view;
typedef struct xide_c_language_view xide_c_language_view;
typedef struct xide_c_completion_view xide_c_completion_view;
typedef struct xide_template_view xide_template_view;
typedef struct xide_import_view xide_import_view;
typedef struct xide_rename_view xide_rename_view;
typedef struct xide_catalog_view xide_catalog_view;
typedef struct xide_session xide_session;
typedef struct xide_font_view xide_font_view;
typedef struct xide_goto_view xide_goto_view;
typedef struct xide_recovery_view xide_recovery_view;
typedef struct xide_file_sync xide_file_sync;
typedef struct xide_file_rebind xide_file_rebind;
typedef struct xide_file_changes_view xide_file_changes_view;

typedef struct xide_editor_position {
    xide_document* owner;
    xui_code_document document;
    uint64_t documentId;
    uint32_t version;
    int offset,line,column,valid;
} xide_editor_position;

enum xide_window_id {
    XIDE_COMMANDS, XIDE_WORKSPACE, XIDE_PROPERTIES, XIDE_XLANG_DEMO, XIDE_C_DEMO,
    XIDE_HELP, XIDE_PROBLEMS, XIDE_OUTPUT, XIDE_TERMINAL, XIDE_TASKS, XIDE_OUTLINE, XIDE_USAGES, XIDE_WINDOW_COUNT
};
typedef struct xide_app {
    xide_dialog* dialogs;
    xide_messages* messages;
    xide_file_dialog* fileDialog;
    xide_actions* actions;
    xide_dpi_proxy* dpiProxy; float dpiScale,dpiOverride; int highDpi;
    xide_file_sync* fileSync; xide_file_changes_view* fileChangesView;
    xide_font_view* fontView; xide_goto_view* gotoView; xide_recovery_view* recoveryView; int initialRecovery;
    xide_settings preferences; int preferencesInitialized;
    char *codeFontPath,*uiFontPath; int codeFontSize,uiFontSize,initialFontSettings,initialUiFontSettings;
    xide_session* session;
    const char* sessionRoot; char* projectSessionPath;
    int noSession,themeExplicit,sessionSwitchConfirmed,windowSizeExplicit;
    int disableAutomaticDiagnostics; /* Explicit manual/compatibility profile. */
    xide_command_catalog catalog; xide_catalog_view* catalogView;
    const char *catalogPath,*initialCommand,*initialCommandSearch;
    xide_workspace workspaceModel;
    xide_workspace_view* workspaceView;
    xide_configuration_view* configurationView;
    xide_references_view* referencesView;
    xide_resource_view* resourceView;
    xide_toolchain_view* toolchainView; xide_language_view* languageView; xide_completion_view* completionView; xide_c_completion_view* cCompletionView; xide_c_language_view* cLanguageView;
    xide_template_view* templateView;
    xide_import_view* importView;
    xide_rename_view* renameView;
    xide_tasks* tasks; xide_task_view* taskView;
    xide_main_toolbar* mainToolbar;
    xvalue* executionTarget; uint64_t executionRevision;
    const char *compilerPath,*configurationId,*settingsPath,*cServerPath;
    int initialChapter,initialChapterRequested;
    int initialTask,waitTask,taskExit,initialConfigurations; uint64_t taskStartMs;
    char* selectedProject;
    const char *workspacePath,*projectPath,*initialSelectedProject,*initialFile;
    xui_widget fileMenu,projectMenu,languageMenu,editMenu;
    xide_file_rebind* fileRebind; xvalue* fileRebindReading;
    xide_document_store documents;
    xui_code_find_scope editorFindScope;
    xide_document *activeDocument,*reopenDocument;
    xide_editor_position editorPosition;
    char editorStatus[256];
    xui_widget_event_proc dockFocusHandler,dockBlurHandler;
    void *dockFocusUser,*dockBlurUser;
    xui_context context;
    xui_proxy_t proxy;
    xui_surface target;
    xui_font uiFont, codeFont;
    xui_widget root, menuBar, toolbar, statusBar, dock;
    xui_widget commands, workspace, properties, editors[2], help, helpContent;
    xui_widget helpTitle, helpDescription, helpSignature, helpParameters, helpReturn, helpExample;
    xui_widget panels[XIDE_WINDOW_COUNT];
    int windows[XIDE_WINDOW_COUNT], documentPane, helpPane, workspacePane, propertyPane;
    int viewportWidth, viewportHeight, theme, selectedCommand, frames, maxFrames, hidden;
    int frameError, initialEditor;
    xide_runtime_failure failure;
    const char* diagnosticRoot;
    const char* screenshot;
} xide_app;
int xideWorkbenchCreate(xide_app* app);
void xideWorkbenchLayout(xide_app* app);
int xideWorkbenchReset(xide_app* app);
int xideWorkbenchApplyTheme(xide_app* app, int theme);
void xideWorkbenchShowHelp(xide_app* app, int command);
void xideDispatchAction(xide_app* app, int action);
int xideWorkbenchAction(xide_app* app,int action);
int xidePlatformRegisterShortcuts(xide_app* app);
int xidePlatformHandleInput(xide_app* app);
void xideAppLoadSettings(xide_app* app);
int xideAppCreate(xide_app* app);
void xideAppDestroy(xide_app* app);
int xideAppFrame(void* user);
void xideAppFailureRecovery(xide_app* app);
int xideAppCapture(xide_app* app,const char* path);
#endif
