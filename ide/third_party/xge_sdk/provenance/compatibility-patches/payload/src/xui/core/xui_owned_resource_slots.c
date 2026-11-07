#include "xui_config.h"
#if XGE_ENABLE_XUI
#include "xui_internal.h"
#include "xui_proxy_resource.h"
#include <stdlib.h>
#include <string.h>

int xuiInternalCleanupPrefixBegin(xui_context context,xui_cleanup_prefix_t* prefix,int status,const xerror* error)
{
    if(!prefix || prefix->self || prefix->context || prefix->previous || prefix->thread || prefix->status || prefix->error)
        return XUI_ERROR_INVALID_ARGUMENT;
    if(!context)return XUI_OK;
    if(!xuiInternalContextIsValid(context) ||
       (context->iOperationDepth<=0 && context->iWidgetCallbackDepth<=0 && !context->bDestroying))return XUI_ERROR_INVALID_STATE;
    prefix->self=prefix;prefix->context=context;prefix->previous=context->pCleanupPrefixes;
    prefix->thread=xrtThreadCurrentId();prefix->status=status;prefix->error=error;
    if(prefix->status==XUI_OK && error)prefix->status=XUI_ERROR_CALLBACK_FAILED;
    if(prefix->previous && prefix->previous->status!=XUI_OK) {
        prefix->status=prefix->previous->status;prefix->error=prefix->previous->error;
    }
    context->pCleanupPrefixes=prefix;return XUI_OK;
}
int xuiInternalCleanupPrefixEnd(xui_cleanup_prefix_t* prefix)
{
    if(!prefix)return XUI_ERROR_INVALID_ARGUMENT;
    if(!prefix->self && !prefix->context && !prefix->previous && !prefix->thread && !prefix->status && !prefix->error)return XUI_OK;
    if(prefix->self!=prefix || !prefix->context || prefix->thread!=xrtThreadCurrentId() ||
       prefix->context->pCleanupPrefixes!=prefix)return XUI_ERROR_INVALID_STATE;
    prefix->context->pCleanupPrefixes=prefix->previous;memset(prefix,0,sizeof(*prefix));return XUI_OK;
}
void xuiInternalContextRecordOutcome(xui_context context,int status,const xerror* error)
{
    if(!context || (status==XUI_OK && !error))return;
    if(status==XUI_OK)status=XUI_ERROR_CALLBACK_FAILED;
    xui_call_scope_t* primaryScope=NULL;
    bool explicitPrimary=context->pCleanupPrefixes && context->pCleanupPrefixes->status!=XUI_OK;
    if(explicitPrimary){
        status=context->pCleanupPrefixes->status;
        error=context->pCleanupPrefixes->error;
    }else{
        /* A parent operation may already own a failure which a nested native
         * scope isolated from TLS. Consuming cleanup must preserve that exact
         * first receipt, including failed-with-NULL, rather than promote a
         * later Drop diagnostic. Ordinary returned errors stay local. */
        for(xui_call_scope_t* scope=context->pCallScopes;scope;scope=scope->pNext){
            if(scope->iResult!=XUI_OK && (scope->bEnding&XUI_CALL_SCOPE_PROPAGATED)){
                status=scope->iResult;error=scope->_pFirstError;primaryScope=scope;
            }
        }
    }
    for(xui_call_scope_t* scope=context->pCallScopes;scope;scope=scope->pNext){
        if(scope->iResult==XUI_OK){
            scope->bEnding|=XUI_CALL_SCOPE_PROPAGATED;
            if(explicitPrimary || (primaryScope && primaryScope!=scope))scope->bEnding|=XUI_CALL_SCOPE_INHERITED;
        }
        xuiInternalCallScopeRecordOutcome(scope,status,error);
    }
}
void xuiInternalContextRecordFailure(xui_context context,int status)
{xuiInternalContextRecordOutcome(context,status,xrtGetError());}
static void outcome(int status,int* result,xerror** first)
{
    xerror* error=xrtTakeError();
    if(status==XUI_OK && !error)return;
    if(*result==XUI_OK){*result=status!=XUI_OK ? status:XUI_ERROR_CALLBACK_FAILED;*first=error;}
    else xrtErrorFree(error);
}
int xuiInternalFontRelease(xui_context context,xui_font font)
{
    int result=xrtGetError() ? XUI_ERROR_CALLBACK_FAILED:XUI_OK;
    xerror* first=result!=XUI_OK ? xrtTakeError():NULL;
    xuiInternalContextRecordOutcome(context,result,first);
    if(font)xuiInternalContextCleanupOutcome(context,xuiFontReleaseChecked(font),&result,&first);
    xuiInternalContextRecordOutcome(context,result,first);xrtSetErrorTake(first);return result;
}
/* Pointer slots retain their actual C types: do not alias a Font** or
 * Surface** through void**. The kind lives in existing stack-token padding. */
static void* slotRead(const xui_resource_slot_update_t* update)
{return update->kind==XUI_OWNED_FONT ? (void*)*(xui_font*)update->slot:(void*)*(xui_surface*)update->slot;}
static void slotWrite(xui_resource_slot_update_t* update,void* value)
{if(update->kind==XUI_OWNED_FONT)*(xui_font*)update->slot=value;else *(xui_surface*)update->slot=value;}
static int resourceValidate(xui_context context,unsigned kind,void* value)
{
    if(!context || (kind!=XUI_OWNED_FONT && kind!=XUI_OWNED_SURFACE))return XUI_ERROR_INVALID_ARGUMENT;
    if(!context->pOwnedProxy || !value)return XUI_OK;
    xui_proxy proxy=kind==XUI_OWNED_FONT ? xuiFontGetProxy(value):xuiSurfaceGetProxy(value);
    return proxy && proxy->_pCapability==context->pOwnedProxy->_pCapability &&
        xuiInternalProxyResourceReady(value,kind) ? XUI_OK:XUI_ERROR_INVALID_ARGUMENT;
}
static int resourceAddRef(unsigned kind,void* value)
{return kind==XUI_OWNED_FONT ? xuiFontAddRef(value):xuiSurfaceAddRef(value);}
static int resourceAdmit(xui_context context,unsigned kind,void* value)
{
    int status=resourceValidate(context,kind,value);
    if(status!=XUI_OK || !context->pOwnedProxy || !value)return status;
    return resourceAddRef(kind,value);
}
static int resourceReleaseChecked(unsigned kind,void* value)
{return kind==XUI_OWNED_FONT ? xuiFontReleaseChecked(value):xuiSurfaceReleaseChecked(value);}
int xuiInternalFontAdmit(xui_context context,xui_font font)
{return resourceAdmit(context,XUI_OWNED_FONT,font);}
int xuiInternalSurfaceAdmit(xui_context context,xui_surface surface)
{return resourceAdmit(context,XUI_OWNED_SURFACE,surface);}
int xuiInternalSurfaceRelease(xui_context context,xui_surface surface)
{
    int result=xrtGetError() ? XUI_ERROR_CALLBACK_FAILED:XUI_OK;
    xerror* first=result!=XUI_OK ? xrtTakeError():NULL;
    xuiInternalContextRecordOutcome(context,result,first);
    if(surface)xuiInternalContextCleanupOutcome(context,xuiSurfaceReleaseChecked(surface),&result,&first);
    xuiInternalContextRecordOutcome(context,result,first);xrtSetErrorTake(first);return result;
}
static int resourceSlotPrepare(xui_context context,void* slot,void* resource,unsigned kind,bool admitted,bool preserveOld,xui_resource_slot_update_t* update)
{
    if(!context || !slot || !update || update->prepared)return XUI_ERROR_INVALID_ARGUMENT;
    update->kind=(uint8_t)kind;
    if(!context->pOwnedProxy){
        update->slot=slot;update->current=resource;update->prepared=true;return XUI_OK;
    }
    /* Reserve this exact real field before any Hold or arbitrary user code.
     * Different fields/widgets can nest; the same field is a checked BUSY. */
    for(xui_resource_slot_update_t* active=context->pActiveResourceUpdates;active;active=active->next)
        if(active->slot==slot)return XUI_ERROR_BUSY;
    int status=xuiCallScopeBegin(context,&update->scope);if(status!=XUI_OK)return status;
    update->context=context;update->slot=slot;update->current=resource;update->previous=slotRead(update);
    update->preserveOld=preserveOld;update->ownsSlots=admitted;update->prepared=true;
    update->next=context->pActiveResourceUpdates;context->pActiveResourceUpdates=update;
    status=resourceValidate(context,kind,resource);
    if(status==XUI_OK && admitted && resource){status=resourceAddRef(kind,resource);update->newHeld=status==XUI_OK;}
    if(status==XUI_OK && admitted && preserveOld && slotRead(update)){
        status=resourceAdmit(context,kind,slotRead(update));
        if(status==XUI_OK){update->guarded=slotRead(update);update->guardHeld=true;}
    }
    return status==XUI_OK ? status:xuiInternalResourceSlotEnd(update,true,status);
}
int xuiInternalResourceSlotPublish(xui_resource_slot_update_t* update)
{
    if(!update || !update->prepared || update->published)return XUI_ERROR_INVALID_STATE;
    if(update->context){
        if(update->context->bCloseRequested || update->context->bDestroyPending || update->context->bDestroying ||
           (update->widget && !xuiInternalWidgetIsValid(update->widget)) ||
           (update->preserveOld && slotRead(update)!=update->previous))return XUI_ERROR_INVALID_STATE;
        xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))return XUI_ERROR_INVALID_STATE;
        update->previous=slotRead(update);slotWrite(update,update->current);update->published=true;
        if(!xrtOwnershipScopeEnd(&mutation))abort();
    }else{update->previous=slotRead(update);slotWrite(update,update->current);update->published=true;}
    return XUI_OK;
}
int xuiInternalFontSlotPrepare(xui_context context,xui_font* slot,xui_font font,bool admitted,bool preserveOld,xui_font_slot_update_t* update)
{return resourceSlotPrepare(context,slot,font,XUI_OWNED_FONT,admitted,preserveOld,update);}
int xuiInternalSurfaceSlotPrepare(xui_context context,xui_surface* slot,xui_surface surface,bool admitted,bool preserveOld,xui_surface_slot_update_t* update)
{return resourceSlotPrepare(context,slot,surface,XUI_OWNED_SURFACE,admitted,preserveOld,update);}
int xuiInternalFontSlotBegin(xui_context context,xui_font* slot,xui_font font,bool admitted,xui_font_slot_update_t* update)
{
    int status=xuiInternalFontSlotPrepare(context,slot,font,admitted,false,update);
    if(status==XUI_OK)status=xuiInternalResourceSlotPublish(update);
    return status==XUI_OK ? status:xuiInternalResourceSlotEnd(update,true,status);
}
int xuiInternalSurfaceSlotBegin(xui_context context,xui_surface* slot,xui_surface surface,bool admitted,xui_surface_slot_update_t* update)
{
    int status=xuiInternalSurfaceSlotPrepare(context,slot,surface,admitted,false,update);
    if(status==XUI_OK)status=xuiInternalResourceSlotPublish(update);
    return status==XUI_OK ? status:xuiInternalResourceSlotEnd(update,true,status);
}
int xuiInternalResourceSlotRestore(xui_resource_slot_update_t* update)
{
    if(!update || !update->prepared || !update->published || update->restored)return XUI_ERROR_INVALID_STATE;
    if(slotRead(update)!=update->current)return XUI_ERROR_INVALID_STATE;
    if(update->context){
        xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))return XUI_ERROR_INVALID_STATE;
        slotWrite(update,update->previous);update->restored=true;
        if(!xrtOwnershipScopeEnd(&mutation))abort();
    }else{slotWrite(update,update->previous);update->restored=true;}
    return XUI_OK;
}
int xuiInternalResourceSlotEnd(xui_resource_slot_update_t* update,bool commit,int status)
{
    if(!update || !update->slot)return status;
    if(!update->context){
        if(update->published && !commit && !update->restored)(void)xuiInternalResourceSlotRestore(update);
        memset(update,0,sizeof(*update));return status;
    }
    xui_context context=update->context;
    if(status==XUI_OK && xrtGetError())status=XUI_ERROR_CALLBACK_FAILED;
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    xuiInternalCallScopeRecordOutcome(&update->scope,status,first);
    if(update->published && !commit && !update->restored)outcome(xuiInternalResourceSlotRestore(update),&status,&first);
    void* retired=!update->published || update->restored ? update->current:update->previous;
    if(update->ownsSlots && retired && (update->published || update->newHeld))xuiInternalContextCleanupOutcome(context,resourceReleaseChecked(update->kind,retired),&status,&first);
    if(update->guardHeld)xuiInternalContextCleanupOutcome(context,resourceReleaseChecked(update->kind,update->guarded),&status,&first);
    xuiInternalCallScopeRecordOutcome(&update->scope,status,first);
    if(context->pActiveResourceUpdates!=update)abort();
    context->pActiveResourceUpdates=update->next;
    status=xuiInternalCallScopeEndOutcome(&update->scope,status,&first);
    memset(update,0,sizeof(*update));xrtSetErrorTake(first);return status;
}
/* Multi-field updates acquire every new owner before publication. Rollback
 * restores every field before releasing any new owner, so a finalizer never
 * observes a partially restored indicator or patch set. Tokens are caller
 * stack storage and close in the reverse order of their native scopes. */
int xuiInternalSurfaceSlotsPrepare(xui_widget widget,size_t count,xui_surface* const* slots,const xui_surface* values,xui_surface_slot_update_t* updates)
{
    if(!xuiInternalWidgetIsValid(widget) || !count || !slots || !values || !updates)return XUI_ERROR_INVALID_ARGUMENT;
    for(size_t i=0;i<count;++i){
        updates[i].widget=widget;
        int status=xuiInternalSurfaceSlotPrepare(widget->pContext,slots[i],values[i],widget->bTypeInitialized!=0,false,&updates[i]);
        if(status!=XUI_OK)return xuiInternalSurfaceSlotsEnd(updates,i,false,status);
    }
    return XUI_OK;
}
int xuiInternalSurfaceSlotsPublish(xui_surface_slot_update_t* updates,size_t count)
{
    if(!updates || !count)return XUI_ERROR_INVALID_ARGUMENT;
    for(size_t i=0;i<count;++i){
        int status=xuiInternalSurfaceSlotPublish(&updates[i]);
        if(status!=XUI_OK)return status;
    }
    return XUI_OK;
}
int xuiInternalSurfaceSlotsEnd(xui_surface_slot_update_t* updates,size_t count,bool commit,int status)
{
    if(!updates)return status;
    if(status==XUI_OK && xrtGetError())status=XUI_ERROR_CALLBACK_FAILED;
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    if(!commit)for(size_t i=count;i>0;--i){
        xui_surface_slot_update_t* update=&updates[i-1];
        if(update->prepared && update->published && !update->restored)
            outcome(xuiInternalSurfaceSlotRestore(update),&status,&first);
    }
    for(size_t i=count;i>0;--i){
        xrtSetErrorTake(first);first=NULL;
        status=xuiInternalSurfaceSlotEnd(&updates[i-1],true,status);
        first=xrtTakeError();
    }
    xrtSetErrorTake(first);return status;
}
bool xuiInternalContextTraceResources(xui_context context,xrtownershipvisitor visit,void* user)
{
    if(!context->pOwnedProxy)return true;
    if(!visit(xuiProxyOwnership(context->pOwnedProxy),user))return false;
    if(context->pTransferData){xrtownershipref data=xuiDataObjectOwnership(context->pTransferData);
        if(!data.Data || !visit(data,user))return false;}
    if(context->pDefaultFont && !visit(xuiFontOwnership(context->pDefaultFont),user))return false;
    if(context->pSystemFont && !visit(xuiFontOwnership(context->pSystemFont),user))return false;
    for(int i=0;i<context->iFontCount;++i)
        if(context->pFonts[i].pFont && !visit(xuiFontOwnership(context->pFonts[i].pFont),user))return false;
    return xuiInternalContextTraceRegistry(context,visit,user) && xuiInternalTextMeasureCacheTrace(context,visit,user) &&
        xuiInternalContextTraceIcons(context,visit,user);
}
#endif
