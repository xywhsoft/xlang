#include "foreign_callback_elements.h"

static int finishDraw(xui_proxy proxy,void* payload)
{
    (void)proxy;
    (void)payload;
    return XUI_OK;
}

int xl_test_proxy_mesh(xui_proxy proxy,int mode)
{
    static const xui_mesh_vertex_t vertices[]={{1,2,UINT32_C(0x11223344)},{3,4,UINT32_C(0xAABBCCDD)}};
    static const uint32_t indices[]={1,0,1};
    xui_draw_context draw=NULL;
    void* payload=NULL;
    int status=xuiDrawCreateOwned(proxy,NULL,0,finishDraw,&draw,&payload);
    if(status)return status;
    switch(mode){
        case 1:status=proxy->drawMeshTriangles(proxy,draw,NULL,0,NULL,0,0);break;
        case 2:status=proxy->drawMeshTriangles(proxy,draw,vertices,-1,indices,3,0);break;
        case 3:status=proxy->drawMeshTriangles(proxy,draw,NULL,2,indices,3,0);break;
        default:status=proxy->drawMeshTriangles(proxy,draw,vertices,2,indices,3,7);break;
    }
    const int released=xuiDrawReleaseChecked(draw);
    return status ? status : released;
}

int xl_test_proxy_clipboard_items(xui_proxy proxy)
{
    static const unsigned char bytes[]={'x',0,'y'};
    const xui_clipboard_item_t items[]={
        {.sFormat="text/plain",.pData=bytes,.iDataSize=sizeof(bytes)},
        {.sFormat="application/x-empty",.pData=NULL,.iDataSize=0},
    };
    return proxy->clipboardSetItems(proxy,items,2);
}

int xl_test_counted_elements(probe_counted_elements callback,void* user,int mode)
{
    static const xui_mesh_vertex_t vertices[]={{8,9,0}};
    if(mode==1)return callback(vertices,SIZE_MAX,user);
    if(mode==2)return callback(NULL,1,user);
    if(mode==3){
        const uintptr_t misaligned=(uintptr_t)vertices+1;
        return callback((const xui_mesh_vertex_t*)misaligned,1,user);
    }
    return callback(vertices,1,user);
}
