#include "foreign_callback_text_geometry.h"
#include <string.h>

static int finishFont(xui_proxy proxy,void* payload)
{
    (void)proxy;
    (void)payload;
    return XUI_OK;
}

int xl_test_proxy_text_geometry(xui_proxy proxy,int mode)
{
    xui_font font=NULL;
    const xui_text_shape_t* shape=NULL;
    void* payload=NULL;
    int status=xuiFontCreateOwned(proxy,0,finishFont,&font,&payload);
    if(status)return status;
    xui_text_item_t input={.iSize=sizeof(input),.pFont=font,.sText="abcd",.iTextSize=-1};
    if(mode==1){
        input.iFlags=XUI_TEXT_SHAPE_RANGE;
        input.iRangeStart=1;
        input.iRangeEnd=3;
    }
    status=xuiTextShapeCreateOwned(proxy,&input,&shape);
    if(mode==2){
        status=(status==XUI_ERROR_UNSUPPORTED && shape==NULL) ? XUI_OK : XUI_ERROR_INVALID_STATE;
    }else if(!status){
        const int length=mode==1 ? 2 : 4;
        const int count=length/2;
        if(!shape || shape->iTextSize!=length || shape->iFlags!=input.iFlags ||
            shape->iClusterCount!=count || shape->iCaretCount!=count || shape->fWidth!=(float)length*3.0f ||
            shape->fHeight!=12.0f || shape->fAscent!=9.0f || shape->fDescent!=3.0f || shape->fLineHeight!=12.0f)
            status=XUI_ERROR_INVALID_STATE;
        for(int i=0;!status && i<count;++i){
            const xui_text_cluster_t* cluster=&shape->pClusters[i];
            const xui_text_caret_t* caret=&shape->pCarets[i];
            if(cluster->iSize!=sizeof(*cluster) || cluster->iTextStart!=i*2 || cluster->iTextEnd!=i*2+2 ||
                caret->iSize!=sizeof(*caret) || caret->iTextOffset!=i*2+1 || caret->fAdvance!=3.0f)
                status=XUI_ERROR_INVALID_STATE;
        }
    }
    if(shape){
        const int disposed=xuiTextShapeReleaseChecked(shape);
        if(!status)status=disposed;
    }
    const int released=xuiFontReleaseChecked(font);
    return status ? status : released;
}

int xl_test_text_geometry_allocations(void)
{
    const bool wasEnabled=xrtMemDebugEnabled();
    if(!wasEnabled && !xrtMemDebugEnable(true))return XUI_ERROR_INVALID_STATE;
    const xui_text_item_t input={.iSize=sizeof(input),.sText="ab",.iTextSize=2};
    const xui_text_cluster_t cluster={.iTextStart=0,.iTextEnd=2,.fAdvance=6.0f};
    const xui_text_caret_t caret={.iTextOffset=1,.fAdvance=3.0f};
    xui_text_shape_t shape={.iSize=sizeof(shape),.iTextSize=2};
    unsigned char beforeShape[sizeof(shape)];
    memcpy(beforeShape,&shape,sizeof(shape));
    int status=XUI_OK;
    for(unsigned i=0;i<2;++i){
        xmemdebugsnapshot before,after;
        xrtMemDebugSnapshot(&before);
        if(!xrtMemDebugFailAfter(i)){
            status=XUI_ERROR_INVALID_STATE;
            break;
        }
        const int result=xl6Xui3TextShapeSetGeometry(&shape,&input,6,12,9,3,12,&cluster,1,&caret,1);
        const bool triggered=xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        if(!triggered || result!=XUI_ERROR_OUT_OF_MEMORY || memcmp(beforeShape,&shape,sizeof(shape)) ||
            before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes ||
            before.InvalidFreeCount!=after.InvalidFreeCount || before.DoubleFreeCount!=after.DoubleFreeCount){
            (void)xuiTextShapeFreeChecked(&shape);
            status=XUI_ERROR_INVALID_STATE;
            break;
        }
    }
    if(!wasEnabled)(void)xrtMemDebugEnable(false);
    return status;
}
