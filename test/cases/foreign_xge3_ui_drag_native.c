#include "foreign_xge3_ui_drag.h"
int binding_ui_drag_dispatch(xui_context context,xui_widget widget,
    xui_data_object data,int type,int sizedPayload)
{
    xui_drag_event_data_t payload={0};
    payload.iSize=sizedPayload ? sizeof(payload):0;
    payload.pData=data;payload.pSource=widget;payload.pTarget=widget;
    payload.iAllowedEffects=XUI_DRAG_EFFECT_COPY|XUI_DRAG_EFFECT_MOVE;
    payload.iSuggestedEffect=XUI_DRAG_EFFECT_COPY;payload.iEffect=XUI_DRAG_EFFECT_MOVE;
    xui_event_t event={0};event.iSize=sizeof(event);event.iType=type;
    event.pTarget=widget;event.pData=&payload;
    return xuiDispatchEvent(context,&event);
}
