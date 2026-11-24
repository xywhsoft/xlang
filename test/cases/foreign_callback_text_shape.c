#include "foreign_callback_text_shape.h"

static int retirePayload(xui_proxy proxy,void* payload)
{
    (void)proxy;
    (void)payload;
    return XUI_OK;
}

int xl_test_proxy_text_shape(xui_proxy proxy)
{
    xui_font font=NULL;
    const xui_text_shape_t* shape=NULL;
    xui_draw_context draw=NULL;
    void* payload=NULL;
    int status=xuiFontCreateOwned(proxy,0,retirePayload,&font,&payload);
    if(status)return status;
    xui_text_item_t item={.iSize=sizeof(item),.pFont=font,.sText="ab",.iTextSize=2};
    status=xuiTextShapeCreateOwned(proxy,&item,&shape);
    if(!status)status=xuiDrawCreateOwned(proxy,NULL,0,retirePayload,&draw,&payload);
    const xui_rect_t rectangle={1,2,10,12};
    if(!status){
        xui_vec2_t size={0};
        status=proxy->textShapeRangeMeasure(proxy,shape,1,2,&size);
        if(!status && (size.fX!=5.0f || size.fY!=12.0f))status=XUI_ERROR_INVALID_ARGUMENT;
    }
    if(!status)
        status=proxy->drawTextShapeRange(proxy,draw,shape,0,2,rectangle,UINT32_C(0x11223344),7);
    if(!status){
        const xui_text_paint_span_t spans[]={
            {.iSize=sizeof(*spans),.iStart=0,.iEnd=2,.iColor=UINT32_C(0xAABBCCDD)}
        };
        status=proxy->drawTextShapeRangeSpans(proxy,draw,shape,0,2,rectangle,UINT32_C(0x11223344),8,0.25f,spans,1);
    }
    if(!status){
        item.pShape=shape;
        item.iFlags=XUI_TEXT_SHAPE_RANGE;
        item.iRangeEnd=2;
        status=proxy->drawText(proxy,draw,&item,rectangle,UINT32_C(0x11223344),9);
    }
    if(draw){
        const int ended=xuiDrawReleaseChecked(draw);
        if(!status)status=ended;
    }
    if(shape){
        const int released=xuiTextShapeReleaseChecked(shape);
        if(!status)status=released;
    }
    const int releasedFont=xuiFontReleaseChecked(font);
    return status ? status : releasedFont;
}
