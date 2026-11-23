#include "foreign_xui_edit_event_owned.h"

#undef NDEBUG
#include <assert.h>
#include <string.h>

static int calls;

static int legacyEditEvent(xui_widget widget, const xui_edit_event_t* event, void* user)
{
    assert(widget && event->iType == XUI_EDIT_EVENT_VALIDATION_CHANGED);
    assert(event->sText && (size_t)event->iTextSize == strlen(event->sText));
    ++*(int*)user;
    return XUI_OK;
}

int fixtureEditUseLegacy(xui_widget input)
{
    return xuiEditSetEvent(input, legacyEditEvent, &calls);
}

int fixtureEditLegacyCalls(void)
{
    return calls;
}
