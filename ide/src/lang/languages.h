#ifndef XLANG_IDE_LANGUAGES_H
#define XLANG_IDE_LANGUAGES_H
#include "app_xrt.h"
#include "xui.h"
/* Unknown extensions use plain text; a per-document explicit override is allowed. */
const char* xideLanguageForPath(const char* path);
int xideLanguageSupported(const char* language);
int xideRegisterLanguages(xui_code_language_registry registry);
int xideConfigureEditorLanguage(xui_widget editor, const char* language);
int xideRebuildEditorFolds(xui_widget editor);
/* Reapply collapse flags only to matching start/end/level ranges. The caller
 * captures before replacing text;the mutable borrowed snapshot is sorted here. */
int xideRestoreEditorFolds(xui_widget editor,xui_code_fold_range_t* previous,int previousCount);
int xideRefreshEditorSyntax(xui_widget editor);
#endif
