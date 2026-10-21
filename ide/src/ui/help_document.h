#ifndef XLANG_IDE_HELP_DOCUMENT_H
#define XLANG_IDE_HELP_DOCUMENT_H
#include "app/xide_app.h"
typedef struct xide_help_view xide_help_view;
int xideHelpCreate(xide_app* app,xide_help_view** out);
xvalue* xideHelpCaptureReading(xide_help_view* view,const xvalue* previous);
void xideHelpResetReading(xide_help_view* view);
void xideHelpRestoreReading(xide_help_view* view,const xvalue* position);
void xideHelpDestroy(xide_help_view* view);
xui_widget xideHelpNavigator(xide_help_view* view);
xui_widget xideHelpPages(xide_help_view* view);
xui_widget xideHelpContent(xide_help_view* view);
const xide_help_document* xideHelpSourceDocument(xide_help_view* view);
size_t xideHelpCodeCount(xide_help_view* view);
xui_widget xideHelpCode(xide_help_view* view,size_t index);
int xideHelpSet(xide_help_view* view,const xide_help_document* const* documents,size_t count);
int xideHelpLayout(xide_help_view* view,int width);
int xideHelpUiFont(xide_help_view* view);
int xideHelpCodeFont(xide_help_view* view);
int xideHelpPrepareRender(xide_help_view* view);
int xideHelpTheme(xide_help_view* view);
size_t xideHelpChapterCount(xide_help_view* view);
const char* xideHelpChapterTitle(xide_help_view* view,size_t chapter);
int xideHelpJump(xide_help_view* view,size_t chapter);
void xideHelpApplyJump(xide_help_view* view);
#endif
