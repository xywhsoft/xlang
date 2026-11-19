#include "test/cases/foreign_xui_codeedit_provider_dispatch.h"

int fixtureProviderEnter(xui_context context, xui_widget editor)
{
    xui_event_t event = {
        .iSize = sizeof(event), .iType = XUI_EVENT_KEY_DOWN,
        .pTarget = editor, .iKey = XUI_KEY_ENTER,
    };
    return xuiDispatchEvent(context, &event);
}
