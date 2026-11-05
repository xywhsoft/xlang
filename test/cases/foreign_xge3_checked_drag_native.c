#include "foreign_xge3_checked_drag.h"
#include <assert.h>
static int lastStatus;
static uint32_t lastEffect;
int binding_drag_dispatch(xge_data_object data,uint32_t* output)
{
    xge_drag_event_t event={0};event.iSize=sizeof(event);
    event.iType=XGE_DRAG_EVENT_ENTER;event.iSequence=23;
    event.iAllowedEffects=XGE_DRAG_EFFECT_COPY|XGE_DRAG_EFFECT_MOVE;
    event.iSuggestedEffect=XGE_DRAG_EFFECT_COPY;event.pData=data;
    lastEffect=UINT32_C(0xa5);
    lastStatus=xgeDragEventDispatchChecked(&event,&lastEffect);
    if(lastStatus==XGE_OK)*output=lastEffect;
    return lastStatus;
}
int binding_drag_last_status(void){return lastStatus;}
uint32_t binding_drag_last_effect(void){return lastEffect;}
void binding_drag_no_alloc(void){assert(xrtMemDebugFailAfter(0));}
int binding_drag_reset_alloc(void)
{bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();return hit;}
