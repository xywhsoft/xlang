#ifndef XL6_XGE3_CALLBACK_TEXT_GEOMETRY_H
#define XL6_XGE3_CALLBACK_TEXT_GEOMETRY_H

#include "xge3_runtime.h"

/* xuiTextShape initializes the flags and input byte count before the callback.
 * The result takes ownership of both arrays after the assignment succeeds. */
static inline int xl6Xui3TextShapeSetGeometry(xui_text_shape_t* shape,const xui_text_item_t* item,
    float width,float height,float ascent,float descent,float lineHeight,
    const xui_text_cluster_t* clusters,int clusterCount,const xui_text_caret_t* carets,int caretCount)
{
    if(!shape || shape->iSize<sizeof(*shape) || !item || item->iSize<sizeof(*item) ||
        !item->sText || item->iTextSize<0 || item->iTextSize!=shape->iTextSize || item->iFlags!=shape->iFlags ||
        clusterCount<0 || (clusterCount && !clusters) || caretCount<0 || (caretCount && !carets))
        return XUI_ERROR_INVALID_ARGUMENT;
    if(shape->pClusters || shape->iClusterCount || shape->pCarets || shape->iCaretCount)
        return XUI_ERROR_INVALID_STATE;

    int textSize=item->iTextSize;
    if(item->iFlags & XUI_TEXT_SHAPE_RANGE){
        if(item->iRangeStart<0 || item->iRangeEnd<item->iRangeStart || item->iRangeEnd>textSize)
            return XUI_ERROR_INVALID_ARGUMENT;
        textSize=item->iRangeEnd-item->iRangeStart;
    }
    if((size_t)clusterCount>SIZE_MAX/sizeof(*clusters) || (size_t)caretCount>SIZE_MAX/sizeof(*carets))
        return XUI_ERROR_OUT_OF_MEMORY;

    xui_text_cluster_t* newClusters=clusterCount ? (xui_text_cluster_t*)xrtMalloc((size_t)clusterCount*sizeof(*newClusters)) : NULL;
    if(clusterCount && !newClusters)return XUI_ERROR_OUT_OF_MEMORY;
    xui_text_caret_t* newCarets=caretCount ? (xui_text_caret_t*)xrtMalloc((size_t)caretCount*sizeof(*newCarets)) : NULL;
    if(caretCount && !newCarets){
        xrtFree(newClusters);
        return XUI_ERROR_OUT_OF_MEMORY;
    }

    int lastOffset=0;
    for(int i=0;i<clusterCount;++i){
        const xui_text_cluster_t* row=&clusters[i];
        if(row->iTextStart<0 || row->iTextEnd<row->iTextStart || row->iTextEnd>textSize)
            goto invalidGeometry;
        newClusters[i]=*row;
        newClusters[i].iSize=sizeof(*newClusters);
    }
    for(int i=0;i<caretCount;++i){
        const xui_text_caret_t* row=&carets[i];
        if(row->iTextOffset<=lastOffset || row->iTextOffset>=textSize)
            goto invalidGeometry;
        lastOffset=row->iTextOffset;
        newCarets[i]=*row;
        newCarets[i].iSize=sizeof(*newCarets);
    }

    shape->iTextSize=textSize;
    shape->iClusterCount=clusterCount;
    shape->pClusters=newClusters;
    shape->fWidth=width;
    shape->fHeight=height;
    shape->fAscent=ascent;
    shape->fDescent=descent;
    shape->fLineHeight=lineHeight;
    shape->iCaretCount=caretCount;
    shape->pCarets=newCarets;
    return XUI_OK;

invalidGeometry:
    xrtFree(newCarets);
    xrtFree(newClusters);
    return XUI_ERROR_INVALID_ARGUMENT;
}

#endif
