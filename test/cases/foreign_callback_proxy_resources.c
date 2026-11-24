#include "foreign_callback_proxy_resources.h"

static int finalized;
static int finalize(xui_proxy proxy,void* payload)
{
    (void)proxy;
    (void)payload;
    ++finalized;
    return XUI_OK;
}

int xl_test_proxy_resources(xui_proxy proxy)
{
    xui_surface surface=NULL;
    xui_font font=NULL;
    void* payload=NULL;
    int status=xuiSurfaceCreateOwned(proxy,0,finalize,&surface,&payload);
    if(status)return status;
    status=xuiFontCreateOwned(proxy,0,finalize,&font,&payload);
    xui_surface_desc_t descriptor={0};
    xui_font_metrics_t metrics={0};
    if(!status)status=proxy->surfaceGetDesc(proxy,surface,&descriptor);
    if(!status)status=proxy->fontGetMetrics(proxy,font,&metrics);
    if(!status){
        xui_text_item_t item={.iSize=sizeof(item),.pFont=font,.sText="probe",.iTextSize=5};
        xui_vec2_t size={0};
        status=proxy->textMeasure(proxy,&item,&size);
        if(!status && size.fX!=5.0f)status=XUI_ERROR_INVALID_ARGUMENT;
    }
    if(!status && (descriptor.iWidth!=17 || metrics.fAscent!=11.0f))status=XUI_ERROR_INVALID_ARGUMENT;
    int releaseFont=font ? xuiFontReleaseChecked(font) : XUI_OK;
    int releaseSurface=xuiSurfaceReleaseChecked(surface);
    return status ? status : releaseFont ? releaseFont : releaseSurface;
}

int xl_test_proxy_resource_finalizers(void)
{
    return finalized;
}
