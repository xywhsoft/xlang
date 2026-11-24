#include "foreign_xui_callback_consumer_routes.h"
#include "foreign_xui_render_backend.c"

static xui_context observedContext;

typedef struct {
    const void* events[2];
    int count;
    int matched;
} event_owners;

static bool remember_event_owner(xrtownershipref owner, void* raw)
{
    event_owners* edges = raw;
    if (!owner.Data || edges->count == 2) return false;
    edges->events[edges->count++] = owner.Data;
    return true;
}

static bool match_context_owner(xrtownershipref owner, void* raw)
{
    event_owners* edges = raw;
    for (int i = 0; i < edges->count; ++i) {
        if (edges->events[i] == owner.Data) ++edges->matched;
    }
    return true;
}

void fixtureEventObserveContext(xui_context context)
{
    assert(!observedContext);
    observedContext = context;
}

int fixtureEventCountOwners(void)
{
    xui_context context = observedContext;
    assert(context);
    observedContext = NULL;
    event_owners edges = {0};
    assert(xuiContextTraceEventOwners(context, remember_event_owner, &edges));
    assert(xuiContextTraceOwnership(context, match_context_owner, &edges));
    assert(edges.matched == edges.count);
    return edges.count;
}
