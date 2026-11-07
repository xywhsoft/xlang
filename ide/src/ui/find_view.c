#include "ui/find_view.h"
#include "services/ui_font.h"

/* XGE owns the window, query, controls, results and replacement operations.
 * The IDE only maps its active document to the native shared find scope. */
void xideFindViewPoll(xide_app* app)
{
    if(!app || !app->editorFindScope) return;
    xui_widget focus=xuiGetFocusWidget(app->context);
    if(focus && (!app->activeDocument || app->activeDocument->editor!=focus) && xuiWidgetGetType(focus)==xuiCodeEditGetType(app->context)) {
        for(size_t i=0;i<app->documents.count;++i) {
            xide_document* d=app->documents.items[i];
            if(d->open && d->editor==focus && app->activeDocument!=d) {
                app->activeDocument=d;
                xideActionsRefresh(app);
                break;
            }
        }
    }
    xide_document* d=app->activeDocument;
    int result=xuiCodeFindScopeSetActiveEditor(app->editorFindScope,d && d->open && d->window>=0 ? d->editor:NULL);
    if(result!=XUI_OK) app->frameError=result;
}

void xideFindViewChanged(xide_app* app) { xideFindViewPoll(app); }

xui_widget xideFindViewWindow(xide_app* app)
{
    return app ? xuiCodeFindScopeGetFindWindow(app->editorFindScope):NULL;
}

int xideFindViewOpen(xide_app* app,int replace)
{
    xide_document* d=app ? app->activeDocument:NULL;
    if(!d || !d->open) return 0;
    xui_widget previous=xideFindViewWindow(app);
    int result=replace ? xuiCodeEditOpenReplace(d->editor):xuiCodeEditOpenFind(d->editor);
    if(result==XUI_OK && previous!=xideFindViewWindow(app))
        result=xideUiFontBindWidget(app,xideFindViewWindow(app));
    if(result!=XUI_OK) return 0;
    xgeRenderRequest();
    return 1;
}

int xideFindViewNavigate(xide_app* app,int previous)
{
    xui_widget window=xideFindViewWindow(app);
    if(!window || !xuiWindowIsOpen(window)) return 0;
    xideFindViewPoll(app);
    return xuiCodeFindScopeFindNext(app->editorFindScope,previous)==XUI_OK;
}
