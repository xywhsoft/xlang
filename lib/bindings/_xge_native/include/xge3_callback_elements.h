#ifndef XL6_XGE3_CALLBACK_ELEMENTS_H
#define XL6_XGE3_CALLBACK_ELEMENTS_H

#include "xge3_runtime.h"
#include "__xl_runtime/xl6_foreign_callback_elements.h"

XL6_FOREIGN_CONST_ELEMENTS(xl6_xui_surface_vertex_span,xui_surface_vertex_t)
XL6_FOREIGN_CONST_ELEMENTS_COPY(xl6_xui_surface_vertex_span,xui_surface_vertex_t)
XL6_FOREIGN_CONST_ELEMENTS(xl6_xui_mesh_vertex_span,xui_mesh_vertex_t)
XL6_FOREIGN_CONST_ELEMENTS_COPY(xl6_xui_mesh_vertex_span,xui_mesh_vertex_t)
XL6_FOREIGN_CONST_ELEMENTS(xl6_xui_mesh_index_span,uint32_t)
XL6_FOREIGN_CONST_ELEMENTS_COPY(xl6_xui_mesh_index_span,uint32_t)
XL6_FOREIGN_CONST_ELEMENTS(xl6_xui_path_command_span,xui_path_command_t)
XL6_FOREIGN_CONST_ELEMENTS_COPY(xl6_xui_path_command_span,xui_path_command_t)
XL6_FOREIGN_CONST_ELEMENTS(xl6_xui_paint_span,xui_text_paint_span_t)
XL6_FOREIGN_CONST_ELEMENTS_COPY(xl6_xui_paint_span,xui_text_paint_span_t)
XL6_FOREIGN_CONST_ELEMENTS(xl6_xui_clipboard_item_span,xui_clipboard_item_t)

static inline int xl6Xui3ForwardClipboardItems(const xn_transfer_services_v1* transfer,xui_context destination,
    xui_proxy origin,const xl6_xui_clipboard_item_span* items)
{
    if(!transfer || !origin || !items || !items->Data || !items->Count || items->Count>64)
        return XUI_ERROR_INVALID_ARGUMENT;
    xui_proxy target=xuiGetProxyOwned(destination);
    if(!target)return XUI_ERROR_INVALID_STATE;
    const xrtownershipref source=xuiProxyOwnership(origin);
    if(!source.Data)return XUI_ERROR_INVALID_STATE;
    if(source.Data==xuiProxyOwnership(target).Data)return XUI_ERROR_BUSY;
    return xuiClipboardSetItemsWithTransfer(destination,items->Data,(int)items->Count,transfer);
}

static inline int xl6Xui3PathStyleReadDash(const xui_path_style_t* style,size_t index,float* output)
{
    if(!style || style->iSize<sizeof(*style) || style->iDashCount<0){
        xrtSetErrorKind(XERR_ARGUMENT);
        return -1;
    }
    if(!style->pDashPattern || index>=(size_t)style->iDashCount){
        xrtSetErrorKind(XERR_RANGE);
        return -1;
    }
    *output=style->pDashPattern[index];
    return 0;
}

static inline int xl6Xui3TextItemReadPaintSpan(const xui_text_item_t* item,size_t index,xui_text_paint_span_t* output)
{
    if(!item || item->iSize<sizeof(*item) || item->iPaintSpanCount<0){
        xrtSetErrorKind(XERR_ARGUMENT);
        return -1;
    }
    if(!item->pPaintSpans || index>=(size_t)item->iPaintSpanCount){
        xrtSetErrorKind(XERR_RANGE);
        return -1;
    }
    *output=item->pPaintSpans[index];
    return 0;
}

#endif
