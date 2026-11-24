#include "foreign_callback_draw_records.h"

static int finish(xui_proxy proxy,void* payload)
{
    (void)proxy;
    (void)payload;
    return XUI_OK;
}

int xl_test_proxy_draw_records(xui_proxy proxy,int mode)
{
    static const xui_path_command_t commands[]={
        {.iCommand=XUI_PATH_CMD_MOVE,.arrPoints={{1,2}}},
        {.iCommand=XUI_PATH_CMD_LINE,.arrPoints={{3,4}}},
    };
    static const float dashes[]={1.5f,2.5f};
    static const xui_text_paint_span_t paint[]={
        {.iSize=sizeof(xui_text_paint_span_t),.iStart=0,.iEnd=2,.iColor=UINT32_C(0x10203040)},
        {.iSize=sizeof(xui_text_paint_span_t),.iStart=2,.iEnd=4,.iColor=UINT32_C(0x50607080)},
    };
    xui_path_style_t style={
        .iSize=sizeof(style),.iFillColor=UINT32_C(0x11223344),.iStrokeColor=UINT32_C(0xAABBCCDD),
        .fStrokeWidth=2.25f,.iFillRule=XUI_PATH_FILL_EVEN_ODD,.iLineJoin=XUI_PATH_JOIN_BEVEL,
        .iLineCap=XUI_PATH_CAP_ROUND,.pDashPattern=dashes,.iDashCount=2,.fDashOffset=0.5f,
    };
    xui_text_item_t item={
        .iSize=sizeof(item),.sText="abcd",.iTextSize=-1,.iFlags=XUI_TEXT_SHAPE_RANGE,
        .iRangeStart=0,.iRangeEnd=4,.pPaintSpans=paint,.iPaintSpanCount=2,
    };
    const xui_rect_t rectangle={1,2,8,9};
    xui_draw_context draw=NULL;
    void* payload=NULL;
    int status=xuiDrawCreateOwned(proxy,NULL,0,finish,&draw,&payload);
    if(status)return status;
    switch(mode){
        case 0:
            status=proxy->drawPath(proxy,draw,commands,2,&style,0.25f);
            break;
        case 1:
            status=proxy->drawSvgPath(proxy,draw,"M 1 2 L 3 4",rectangle,rectangle,&style,0.5f);
            break;
        case 2:
            status=proxy->drawText(proxy,draw,&item,rectangle,UINT32_C(0xFFEEDDCC),7);
            break;
        case 3:
            style.iDashCount=-1;
            status=proxy->drawPath(proxy,draw,commands,2,&style,0.75f);
            break;
        case 4:
            item.iPaintSpanCount=-1;
            status=proxy->drawText(proxy,draw,&item,rectangle,UINT32_C(0xFFEEDDCC),8);
            break;
        case 5:
            style.pDashPattern=NULL;
            status=proxy->drawPath(proxy,draw,commands,2,&style,0.75f);
            break;
        case 6:
            item.pPaintSpans=NULL;
            status=proxy->drawText(proxy,draw,&item,rectangle,UINT32_C(0xFFEEDDCC),8);
            break;
        default:
            status=XUI_ERROR_INVALID_ARGUMENT;
            break;
    }
    const int finished=xuiDrawReleaseChecked(draw);
    return status ? status : finished;
}
