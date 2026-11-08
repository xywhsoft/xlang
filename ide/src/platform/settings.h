#ifndef XLANG_IDE_SETTINGS_H
#define XLANG_IDE_SETTINGS_H
#include "app/features.h"
#include "model/workspace.h"
typedef struct xide_settings_options {
    int tabColumns,indentColumns,expandTabs,lineNumbers,foldMargin,minimap,wordWrap;
    int documentLineGap,documentWrap,theme,automaticDiagnostics,diagnosticDelay,completion,signatureHelp,helpFollow;
} xide_settings_options;
typedef struct xide_settings { char *path,*compiler,*cServer,*editorFont,*uiFont,*documentFont; int editorFontSize,uiFontSize,documentFontSize,optionsReady; xide_settings_options options; xvalue* source; int existed; } xide_settings;
typedef struct xide_settings_draft {
    const char *compiler,*editorFont,*uiFont,*documentFont;
    int editorSize,uiSize,documentSize;
    xide_settings_options options;
} xide_settings_draft;
const xide_settings_options* xideSettingsOptions(const xide_settings* settings);
int xideSettingsSaveDraft(xide_settings*,const xide_settings_draft*,xide_model_error*);
char* xideSettingsDefaultPath(void);
int xideSettingsLoad(xide_settings* settings,const char* path,xide_model_error* error);
int xideSettingsSetCompiler(xide_settings* settings,const char* compiler,xide_model_error* error);
#if XIDE_ENABLE_C_LSP
int xideSettingsSetCServer(xide_settings* settings,const char* server,xide_model_error* error);
#endif
int xideSettingsSetEditorFont(xide_settings* settings,const char* path,int size,xide_model_error* error);
int xideSettingsSetUiFont(xide_settings* settings,const char* path,int size,xide_model_error* error);
int xideSettingsRememberOpen(xide_settings* settings,const char* path,xide_model_error* error);
unsigned xideSettingsAssociationMask(const xide_settings*);
int xideSettingsAssociationPrompt(const xide_settings*);
int xideSettingsSetAssociations(xide_settings*,unsigned,int,xide_model_error*);
/* Rebuild known preferences from live fonts and the loaded preferences, keeping
   unknown members of a previously valid settings object. CLI tools stay local. */
xvalue* xideSettingsCapture(const xide_settings* settings,const char* path,const char* editorFont,int editorSize,const char* uiFont,int uiSize);
void xideSettingsClear(xide_settings* settings);
#endif
