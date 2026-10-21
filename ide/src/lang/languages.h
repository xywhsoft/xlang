#ifndef XLANG_IDE_LANGUAGES_H
#define XLANG_IDE_LANGUAGES_H
#include "app_xrt.h"
#include "xui.h"
/* Unknown extensions use plain text; a per-document explicit override is allowed. */
const char* xideLanguageForPath(const char* path);
int xideRegisterLanguages(xui_code_language_registry registry);
int xideConfigureEditorLanguage(xui_widget editor, const char* language);
int xideRebuildEditorFolds(xui_widget editor);
int xideRefreshEditorSyntax(xui_widget editor);
#endif