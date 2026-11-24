#include "foreign_callback_fixed_elements.h"

static int finishResource(xui_proxy proxy,void* payload)
{
    (void)proxy;
    (void)payload;
    return XUI_OK;
}

int xl_test_proxy_quads(xui_proxy proxy,int mode)
{
    static const xui_surface_vertex_t vertices[4]={
        {.fX=11,.fY=21,.fU=0,.fV=0,.iColor=UINT32_C(0x11223344)},
        {.fX=12,.fY=22,.fU=1,.fV=0,.iColor=UINT32_C(0x20304050)},
        {.fX=13,.fY=23,.fU=1,.fV=1,.iColor=UINT32_C(0x50607080)},
        {.fX=14,.fY=24,.fU=0,.fV=1,.iColor=UINT32_C(0xAABBCCDD)},
    };
    xui_surface surface=NULL;
    void* payload=NULL;
    int status=xuiSurfaceCreateOwned(proxy,0,finishResource,&surface,&payload);
    if(status)return status;
    switch(mode){
        case 0:
            status=proxy->surfaceDrawQuad(proxy,surface,vertices,71);
            break;
        case 1:
            status=proxy->surfaceDrawQuadTo(proxy,NULL,surface,vertices,72);
            break;
        case 2: {
            xui_draw_context draw=NULL;
            status=xuiDrawCreateOwned(proxy,surface,0,finishResource,&draw,&payload);
            if(status)break;
            status=proxy->drawSurfaceQuad(proxy,draw,surface,vertices,73);
            const int endStatus=xuiDrawReleaseChecked(draw);
            if(!status)status=endStatus;
            break;
        }
        case 3:
            status=proxy->surfaceDrawQuad(proxy,surface,NULL,74);
            break;
        default:
            status=XUI_ERROR_INVALID_ARGUMENT;
            break;
    }
    const int surfaceStatus=xuiSurfaceReleaseChecked(surface);
    return status ? status : surfaceStatus;
}
