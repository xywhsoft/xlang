#include "foreign_xui_resource_reservation.h"
#include "foreign_xui_render_backend.c"

static int sampleReservation(void* user)
{
    assert(xuiResourceGetRefCount(user) == 2);
    return XUI_OK;
}

void fixtureResourceNativeSample(xui_resource resource)
{
    assert(xuiResourceWithReservation(resource, sampleReservation, resource) == XUI_OK);
    fixtureRenderHeapBegin();
    for (int i = 0; i < 128; ++i) {
        assert(xuiResourceWithReservation(resource, sampleReservation, resource) == XUI_OK);
    }
    fixtureRenderHeapResult(1);
}
