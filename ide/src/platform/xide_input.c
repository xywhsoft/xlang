#include "app/xide_app.h"

/* Chords and labels come from the same action definitions. Editor chords
   only route while the target is an open document; ordinary fields stay local. */
static int shortcut(xui_widget widget,const xui_event_t* event,void* user)
{
    (void)widget; xide_app* app=user;
    for(size_t i=0;i<xideActionCount();++i) {
        const xide_action_info* action=xideActionAt(i);
        if(!action->key || event->iKey!=action->key || event->iModifiers!=action->modifiers) continue;
        if(action->shortcutScope==XIDE_SHORTCUT_WORKSPACE && xuiGetFocusWidget(app->context)!=app->workspace) continue;
        if(action->shortcutScope==XIDE_SHORTCUT_EDITOR) {
            xui_widget focus=xuiGetFocusWidget(app->context); xide_document* target=NULL;
            for(size_t j=0;j<app->documents.count;++j) if(app->documents.items[j]->open && focus==app->documents.items[j]->editor) { target=app->documents.items[j]; break; }
            if(!target) continue;
            app->activeDocument=target;
        }
        xideDispatchAction(app,action->id);
        return XUI_EVENT_DISPATCH_STOP;
    }
    return XUI_OK;
}
static int dockFocus(xui_widget widget,const xui_event_t* event,void* user)
{
    xide_app* app=user;
    xui_widget_event_proc original=event->iType==XUI_EVENT_FOCUS ? app->dockFocusHandler : app->dockBlurHandler;
    void* originalUser=event->iType==XUI_EVENT_FOCUS ? app->dockFocusUser : app->dockBlurUser;
    int ret=original ? original(widget,event,originalUser) : XUI_OK;
    /* The pinned Dock consumes child focus/blur in CAPTURE, preventing
       CodeEdit from cancelling composition. Preserve its invalidation but
       allow the child's native handler to receive its own notification. */
    if(ret>=0 && event->iPhase==XUI_EVENT_PHASE_CAPTURE && event->pTarget!=widget)
        ret &= ~XUI_EVENT_DISPATCH_STOP;
    return ret;
}
static int installDockFocus(xide_app* app,int type,xui_widget_event_proc* original,void** user)
{
    xui_widget_event_proc handler=NULL; void* data=NULL;
    int ret=xuiWidgetGetEventHandler(app->dock,type,&handler,&data);
    if(ret!=XUI_OK || handler==dockFocus) return ret;
    *original=handler; *user=data;
    return xuiWidgetSetEventHandler(app->dock,type,dockFocus,app);
}
int xidePlatformRegisterShortcuts(xide_app* app)
{
    if(!app || !app->context || !app->root || !app->dock) return XUI_ERROR_INVALID_ARGUMENT;
    int ret=installDockFocus(app,XUI_EVENT_FOCUS,&app->dockFocusHandler,&app->dockFocusUser);
    if(ret==XUI_OK) ret=installDockFocus(app,XUI_EVENT_BLUR,&app->dockBlurHandler,&app->dockBlurUser);
    if(ret!=XUI_OK) return ret;
    for(size_t i=0;i<xideActionCount();++i) {
        const xide_action_info* action=xideActionAt(i);
        if(!action->key) continue;
        int duplicate=0; for(size_t j=0;j<i;++j) { const xide_action_info* earlier=xideActionAt(j); if(earlier->key==action->key && earlier->modifiers==action->modifiers) { duplicate=1; break; } } if(duplicate) continue;
        int ret=xuiHotKeyRegister(app->context,app->root,action->key,action->modifiers,shortcut,app);
        if(ret!=XUI_OK) { xuiHotKeyClearWidget(app->root); return ret; }
    }
    return XUI_OK;
}
int xidePlatformHandleInput(xide_app* app)
{
    if(!app || !app->context) return XUI_ERROR_INVALID_ARGUMENT;
    /* This bridge drains arrival-ordered events and dispatches each before
       reading the next. It forwards event-time modifiers, repeats, pointers,
       composition and commit, and owns TSF snapshots/candidate positioning.
       Polling legacy key/text/mouse views here would deliver input twice. */
    return xuiProxyXgePumpInput(app->context);
}
