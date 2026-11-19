#include "test/cases/foreign_xui_codeedit_command_dispatch.h"

int fixtureCodeEditCommandKey(xui_context context, xui_widget editor, int key, uint32_t modifiers)
{
    xui_event_t event = {
        .iSize = sizeof(event), .iType = XUI_EVENT_KEY_DOWN, .pTarget = editor,
        .iKey = key, .iModifiers = modifiers,
    };
    return xuiDispatchEvent(context, &event);
}
