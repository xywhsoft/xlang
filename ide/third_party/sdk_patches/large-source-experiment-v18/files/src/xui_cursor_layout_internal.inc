/* A cached provider is opt-in. This first integration resumes only a whole,
   tab-free long line; tab-separated and custom layouts keep their full path. */
static int __xuiCodeEditVisitLayoutShape(xui_proxy pProxy,xui_font pFont,xui_code_edit_data_t* pData,
    const char* sText,int iSize,xui_code_edit_layout_visit_t* pVisit,xui_text_shape_t* pShape)
{
    int iRet;
    if (pData->onShapeVisit) iRet=pData->onShapeVisit(pProxy,pFont,sText,iSize,XUI_TEXT_SHAPE_DEFAULT,
        __xuiCodeEditLayoutVisitCluster,pVisit,pShape,pData->pShapeVisitUser);
    else iRet=xuiInternalProxyXgeVisitTextShape(pProxy,pFont,sText,iSize,XUI_TEXT_SHAPE_DEFAULT,
        __xuiCodeEditLayoutVisitCluster,pVisit,pShape);
    if (iRet==XUI_ERROR_UNSUPPORTED) {
        iRet=__xuiCodeEditShapeText(pProxy,pFont,sText,iSize,pShape);
        if (iRet==XUI_OK) for (int i=0;i<pShape->iClusterCount;++i) {
            iRet=__xuiCodeEditLayoutVisitCluster(&pShape->pClusters[i],pVisit);
            if (iRet!=XUI_OK) break;
        }
    }
    return iRet;
}

static void __xuiCodeEditKeepCursorLayout(xui_code_edit_data_t* pData,xui_font pFont)
{
    int keep=-1;
    for (int i=0;i<pData->iLineLayoutCount;++i) {
        xui_code_edit_line_layout_t* p=&pData->pLineLayouts[i];
        if (p->pCursor && p->pFont==pFont && p->iTabColumns==pData->iTabColumns &&
            (keep<0 || p->iTextSize>pData->pLineLayouts[keep].iTextSize)) keep=i;
    }
    if (keep<0) {__xuiCodeEditLineLayoutsClear(pData);return;}
    for (int i=0;i<pData->iLineLayoutCount;++i) if (i!=keep) __xuiCodeEditLineLayoutClear(&pData->pLineLayouts[i]);
    if (keep>0) {pData->pLineLayouts[0]=pData->pLineLayouts[keep];memset(&pData->pLineLayouts[keep],0,sizeof(*pData->pLineLayouts));}
    pData->iLineLayoutCount=1;pData->bVisibleRangeCached=0;
}

static xui_code_edit_line_layout_t* __xuiCodeEditCursorSingleLayout(xui_proxy pProxy,xui_font pFont,
    xui_code_edit_data_t* pData,xui_code_edit_line_layout_t* pLayout,const char* sText,int iSize,uint32_t iVersion)
{
    char* sNew=xrtMalloc((size_t)iSize+1u);
    xui_text_shape_cursor pOld=NULL,pNew=NULL;
    xui_text_shape_resume_t tResume={0};
    xui_code_edit_layout_visit_t tVisit={0};
    xui_code_edit_text_segment_t* pSegment;
    int iCommon=0,iRet;
    if (!sNew) return NULL;
    memcpy(sNew,sText,(size_t)iSize);sNew[iSize]=0;
    if (pLayout->pCursor && pLayout->pFont==pFont && pLayout->iTabColumns==pData->iTabColumns &&
        pLayout->onCursorFree==pData->onCursorFree && pLayout->pCursorUser==pData->pCursorVisitUser) {
        int n=iSize<pLayout->iTextSize?iSize:pLayout->iTextSize;
        while (iCommon<n && pLayout->sText[iCommon]==sNew[iCommon]) ++iCommon;
        pOld=pLayout->pCursor;pLayout->pCursor=NULL;
    } else {
        __xuiCodeEditLineLayoutClear(pLayout);
    }
    for (int i=0;i<pLayout->iSegmentCount;++i) xuiTextShapeFree(&pLayout->pSegments[i].tShape);
    pLayout->iSegmentCount=0;xrtFree(pLayout->sText);pLayout->sText=sNew;
    pLayout->iDocumentVersion=iVersion;pLayout->pFont=pFont;pLayout->iTabColumns=pData->iTabColumns;pLayout->iTextSize=iSize;
    pLayout->iUseStamp=++pData->iLineLayoutUseStamp;pLayout->iWrapOffsetCount=0;pLayout->fWrapWidth=0;
    if (__xuiCodeEditLineLayoutReserveSegments(pLayout,1)!=XUI_OK) goto failed;
    pSegment=&pLayout->pSegments[0];memset(pSegment,0,sizeof(*pSegment));pSegment->iEnd=iSize;pLayout->iSegmentCount=1;
    if (pLayout->iStopCount<=0 && __xuiCodeEditLineLayoutAddStop(pLayout,0,0)!=XUI_OK) goto failed;
    tResume.iSize=sizeof(tResume);tVisit.pLayout=pLayout;tVisit.pResume=&tResume;tVisit.iOldStopCount=pLayout->iStopCount;
    iRet=pData->onCursorVisit(pProxy,pFont,sNew,iSize,XUI_TEXT_SHAPE_DEFAULT,
        __xuiCodeEditLayoutVisitCluster,&tVisit,&pSegment->tShape,pOld,iCommon,&pNew,&tResume,pData->pCursorVisitUser);
    if (pOld) {pData->onCursorFree(pOld,pData->pCursorVisitUser);pOld=NULL;}
    pLayout->pCursor=pNew;pLayout->onCursorFree=pData->onCursorFree;pLayout->pCursorUser=pData->pCursorVisitUser;
    if (iRet==XUI_ERROR_UNSUPPORTED) {
        if (pNew) {pData->onCursorFree(pNew,pData->pCursorVisitUser);pLayout->pCursor=NULL;}
        xuiTextShapeFree(&pSegment->tShape);pLayout->iStopCount=1;pLayout->pStops[0]=(xui_code_edit_caret_stop_t){0,0};
        tVisit.pResume=NULL;tVisit.fX=0;
        iRet=__xuiCodeEditVisitLayoutShape(pProxy,pFont,pData,sNew,iSize,&tVisit,&pSegment->tShape);
    }
    if (iRet!=XUI_OK) goto failed;
    pLayout->fWidth=tVisit.fX;
    if (__xuiCodeEditLineLayoutAddStop(pLayout,iSize,tVisit.fX)!=XUI_OK) goto failed;
    return pLayout;
failed:
    if (pOld) pData->onCursorFree(pOld,pData->pCursorVisitUser);
    __xuiCodeEditLineLayoutClear(pLayout);return NULL;
}
