#ifndef XLANG_IDE_SETTINGS_H
#define XLANG_IDE_SETTINGS_H
#include "model/workspace.h"
typedef struct xide_settings { char *path,*compiler,*cServer,*editorFont,*uiFont; int editorFontSize,uiFontSize; xvalue* source; int existed; } xide_settings;
char* xideSettingsDefaultPath(void);
int xideSettingsLoad(xide_settings* settings,const char* path,xide_model_error* error);
int xideSettingsSetCompiler(xide_settings* settings,const char* compiler,xide_model_error* error);
int xideSettingsSetCServer(xide_settings* settings,const char* server,xide_model_error* error);
int xideSettingsSetEditorFont(xide_settings* settings,const char* path,int size,xide_model_error* error);
int xideSettingsSetUiFont(xide_settings* settings,const char* path,int size,xide_model_error* error);
int xideSettingsRememberOpen(xide_settings* settings,const char* path,xide_model_error* error);
/* Rebuild known preferences from live fonts and the loaded preferences, keeping
   unknown members of a previously valid settings object. CLI tools stay local. */
xvalue* xideSettingsCapture(const xide_settings* settings,const char* path,const char* editorFont,int editorSize,const char* uiFont,int uiSize);
void xideSettingsClear(xide_settings* settings);
#endif
