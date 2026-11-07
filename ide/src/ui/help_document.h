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
/* UI-thread admission only. Documents must belong to the supplied loaded
 * catalog. Parsing owns a lease and never accesses widgets; Poll publishes
 * only the newest generation on the UI thread. Retry a declined admission. */
int xideHelpSetDeferred(xide_help_view*,const xide_help_document* const*,size_t,const xide_command_catalog*);
int xideHelpPoll(xide_help_view*);
int xideHelpPending(xide_help_view*);
void xideHelpPreparationCounts(xide_help_view*,unsigned*,unsigned*);
int xideHelpLayout(xide_help_view* view,int width);
int xideHelpDocumentWidth(xide_help_view*,int);
int xideHelpUiFont(xide_help_view* view);
int xideHelpDocumentFont(xide_help_view* view);
int xideHelpCodeFont(xide_help_view* view);
int xideHelpPrepareRender(xide_help_view* view);
int xideHelpTheme(xide_help_view* view);
size_t xideHelpChapterCount(xide_help_view* view);
const char* xideHelpChapterTitle(xide_help_view* view,size_t chapter);
int xideHelpJump(xide_help_view* view,size_t chapter);
void xideHelpApplyJump(xide_help_view* view);
#endif
