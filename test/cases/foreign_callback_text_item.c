#include "foreign_callback_text_item.h"

int xl_test_callback_text_item(xui_proxy proxy,int mode)
{
    static const char itemBytes[]={'a',0,'b',0};
    xui_text_item_t item={
        .iSize=sizeof(item), .sText=itemBytes, .iTextSize=3, .iFlags=7,
        .iScript=UINT32_C(0x4c61746e), .fDrawOffsetX=0.25f,
    };
    switch(mode){
        case 1:
            item.iTextSize=-1;
            item.sContext="ab";
            item.iContextSize=-1;
            item.sLanguage="en";
            break;
        case 2:
            item.iTextSize=-2;
            break;
        case 3:
            item.iContextSize=-1;
            break;
        case 4:
            item.sContext="";
            break;
        case 5:
            item.sText=NULL;
            item.iTextSize=0;
            break;
    }
    xui_vec2_t measured={0};
    const int status=proxy->textMeasure(proxy,&item,&measured);
    if(status)return status;
    return (measured.fX==3.5f && measured.fY==2.0f) ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;
}

int xl_test_inline_text(xl_test_inline_text_proc callback,void* user,int mode)
{
    xl_test_inline_text_t text={{'a','b','c','d'},0,-1};
    if(mode==0)text.data[3]=0;
    if(mode==2)text.size=4;
    if(mode==3)text.size=5;
    return callback(&text,user);
}
