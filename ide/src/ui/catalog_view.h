#ifndef XLANG_IDE_CATALOG_VIEW_H
#define XLANG_IDE_CATALOG_VIEW_H
#include "app/xide_app.h"
#include "ui/help_document.h"
int xideCatalogResetReading(xide_app* app);
char* xideCatalogTreeKey(xide_app* app,int node);
xui_widget xideCatalogReloadWidget(xide_app* app);
xui_widget xideCatalogStatusWidget(xide_app* app);
int xideCatalogViewReload(xide_app* app);
xide_help_view* xideCatalogHelpDocument(xide_app* app);
void xideCatalogViewAfterLayout(xide_app* app);
int xideCatalogViewPrepareRender(xide_app* app);
int xideCatalogViewUiFont(xide_app* app);
int xideCatalogViewCodeFont(xide_app* app);
int xideCatalogViewTheme(xide_app* app);
int xideCatalogViewCreate(xide_app* app);
void xideCatalogViewDestroy(xide_app* app);
void xideCatalogViewLayout(xide_app* app);
void xideCatalogViewPoll(xide_app* app);
int xideCatalogViewSearch(xide_app* app,const char* query);
int xideCatalogViewSelect(xide_app* app,size_t index);
int xideCatalogViewInsertExample(xide_app* app);
int xideCatalogBindEditor(xide_app* app,xui_widget editor);
xui_widget xideCatalogSearchWidget(xide_app* app);
xui_widget xideCatalogInsertWidget(xide_app* app);
xui_widget xideCatalogReturnReferencesWidget(xide_app* app);
size_t xideCatalogVisibleCount(xide_app* app);
enum { XIDE_CATALOG_SOURCE_DECLARATION, XIDE_CATALOG_SOURCE_DOCUMENT };
int xideCatalogSourceAvailable(xide_app* app,int document);
int xideCatalogSourceOpen(xide_app* app,int document);
int xideCatalogSourceIsOpen(xide_app* app);
void xideCatalogSourceClose(xide_app* app);
xui_widget xideCatalogSourceButton(xide_app* app,int document);
xui_widget xideCatalogSourceWindow(xide_app* app);
xui_widget xideCatalogSourceEditor(xide_app* app);
xui_widget xideCatalogSourceCloseWidget(xide_app* app);
const char* xideCatalogSourceMessage(xide_app* app);
#endif
