#include "test/cases/foreign_xui_codeedit_input_setup.h"
#include <string.h>

int fixtureCodeEditDispatchText(xui_context context, xui_widget editor, uint32_t scalar)
{
    xui_event_t event={.iSize=sizeof(event),.iType=XUI_EVENT_TEXT,.pTarget=editor,.iCodepoint=scalar};
    return xuiDispatchEvent(context,&event);
}

int fixtureCodeEditDispatchIme(xui_context context, xui_widget editor)
{
    const char value[]="\xe4\xb8\xad";
    xui_event_t event={.iSize=sizeof(event),.iType=XUI_EVENT_IME_COMPOSITION,
        .pTarget=editor,.iTextSize=sizeof(value)-1,.bCompositionActive=0};
    memcpy(event.sText,value,sizeof(value));
    return xuiDispatchEvent(context,&event);
}
