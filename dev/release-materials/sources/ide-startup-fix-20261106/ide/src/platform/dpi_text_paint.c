#include "app_xrt.h"
#include "platform/dpi_text_paint.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct dpi_text_paint {
    void* logicalPaint;
    void (*logicalFree)(void*);
    int (*logicalFreeChecked)(void*,bool*);
    const xui_text_paint_ownership_v1_t* logicalOwnership;
    xui_text_shape_t physical;
    float scale;
    int hasPhysical;
} dpi_text_paint;

static bool protocolValid(const xui_text_paint_ownership_v1_t* protocol)
{
    return protocol && protocol->size>=sizeof(*protocol) &&
        protocol->AbiTag==XUI_TEXT_PAINT_OWNERSHIP_V1_TAG &&
        protocol->DropReady && protocol->TraceRaw && protocol->DropChecked;
}
static void logicalOwner(const dpi_text_paint* paint,xui_text_shape_t* output)
{
    memset(output,0,sizeof(*output));output->iSize=sizeof(*output);
    output->pPaint=paint->logicalPaint;output->paintFree=paint->logicalFree;
    output->paintFreeChecked=paint->logicalFreeChecked;
    output->pPaintOwnership=paint->logicalOwnership;
}
static bool paintReady(const void* value)
{
    const dpi_text_paint* paint=value;
    return paint && paint->logicalOwnership->DropReady(paint->logicalPaint) &&
        (!paint->physical.pPaint || paint->physical.pPaintOwnership->DropReady(paint->physical.pPaint));
}
static bool paintTrace(const void* value,xrtownershipvisitor visit,void* user)
{
    const dpi_text_paint* paint=value;
    return paint && visit && paint->logicalOwnership->TraceRaw(paint->logicalPaint,visit,user) &&
        (!paint->physical.pPaint || paint->physical.pPaintOwnership->TraceRaw(paint->physical.pPaint,visit,user));
}
static int paintDrop(void* value)
{
    dpi_text_paint* paint=value;
    xui_text_shape_t logical;logicalOwner(paint,&logical);
    xerror* previous=xrtTakeError();
    int status=xuiTextShapeFreeChecked(&logical);xerror* first=xrtTakeError();
    if(status==XUI_OK && first)status=XUI_ERROR_CALLBACK_FAILED;
    int next=xuiTextShapeFreeChecked(&paint->physical);xerror* secondary=xrtTakeError();
    if(status==XUI_OK && (next!=XUI_OK || secondary)) {
        status=next!=XUI_OK ? next:XUI_ERROR_CALLBACK_FAILED;first=secondary;
    } else xrtErrorFree(secondary);
    /* Both payloads come from xuiProxyXge. Their admission is creator-thread
     * affinity, which stays true during this consuming cleanup. Semantic
     * failures consume their owners; physical arrays use the native allocator.
     * The outer shape alone owns the logical cluster/caret arrays. */
    free(paint);
    if(previous) { xrtErrorFree(first);first=previous; }
    xrtSetErrorTake(first);return status;
}
static int paintFreeChecked(void* value,bool* consumed)
{
    if(!consumed)return XUI_ERROR_INVALID_ARGUMENT;
    *consumed=false;
    if(!value)return XUI_ERROR_INVALID_ARGUMENT;
    if(!paintReady(value))return XUI_ERROR_BUSY;
    int status=paintDrop(value);*consumed=true;return status;
}
static void paintFree(void* value)
{
    bool consumed=false;int status=paintFreeChecked(value,&consumed);
    if(status!=XUI_OK && !xrtGetError())xrtSetErrorKind(XERR_STATE);
}
static const xui_text_paint_ownership_v1_t paintOwnership={
    sizeof(paintOwnership),XUI_TEXT_PAINT_OWNERSHIP_V1_TAG,paintReady,paintTrace,paintDrop
};
static dpi_text_paint* carrier(const xui_text_shape_t* shape)
{
    return shape && shape->pPaintOwnership==&paintOwnership ? shape->pPaint:NULL;
}
int xideDpiTextPaintWrap(xui_text_shape_t* logical,xui_text_shape_t* physical,float scale)
{
    if(!logical || !logical->pPaint || logical==physical || !isfinite(scale) || scale<=0)
        return XUI_ERROR_INVALID_ARGUMENT;
    if(!protocolValid(logical->pPaintOwnership) ||
       (physical && physical->pPaint && !protocolValid(physical->pPaintOwnership)))
        return XUI_ERROR_UNSUPPORTED;
    dpi_text_paint* paint=calloc(1,sizeof(*paint));if(!paint)return XUI_ERROR_OUT_OF_MEMORY;
    paint->logicalPaint=logical->pPaint;paint->logicalFree=logical->paintFree;
    paint->logicalFreeChecked=logical->paintFreeChecked;paint->logicalOwnership=logical->pPaintOwnership;
    paint->scale=scale;
    if(physical) { paint->physical=*physical;paint->hasPhysical=1;memset(physical,0,sizeof(*physical)); }
    logical->pPaint=paint;logical->paintFree=paintFree;
    logical->paintFreeChecked=paintFreeChecked;logical->pPaintOwnership=&paintOwnership;
    logical->iPaintBytes+=sizeof(*paint)+paint->physical.iPaintBytes+
        (size_t)paint->physical.iClusterCount*sizeof(xui_text_cluster_t)+
        (size_t)paint->physical.iCaretCount*sizeof(xui_text_caret_t);
    return XUI_OK;
}
void xideDpiTextPaintLogical(const xui_text_shape_t* shape,xui_text_shape_t* output)
{
    *output=*shape;dpi_text_paint* paint=carrier(shape);
    if(paint) {
        output->pPaint=paint->logicalPaint;output->paintFree=paint->logicalFree;
        output->paintFreeChecked=paint->logicalFreeChecked;output->pPaintOwnership=paint->logicalOwnership;
    }
}
int xideDpiTextPaintPhysical(const xui_text_shape_t* shape,float scale,xui_text_shape_t* output)
{
    dpi_text_paint* paint=carrier(shape);
    if(!paint || paint->scale!=scale)return XUI_ERROR_UNSUPPORTED;
    if(scale==1)xideDpiTextPaintLogical(shape,output);
    else if(paint->hasPhysical)*output=paint->physical;
    else return XUI_ERROR_UNSUPPORTED;
    return XUI_OK;
}
