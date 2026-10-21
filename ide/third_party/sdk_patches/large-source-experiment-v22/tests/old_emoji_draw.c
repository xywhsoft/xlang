static int diagOldEmojiDraw(xge_emoji_pack pPack, uint32_t iEmojiId, xge_rect_t tDst,
	int bScreenSpace)
{
	xge_svg pSvg;
	int iEntry;
	int iRet;

	if ( (pPack == NULL) || (iEmojiId == 0) || (iEmojiId > (uint32_t)pPack->iEntryCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iEntry = (int)iEmojiId - 1;
	pSvg = NULL;
	xrtMutexLock(pPack->pCacheMutex);
	iRet = __xgeEmojiEntryLoadLocked(pPack, iEntry);
	if ( iRet == XGE_OK ) {
		pSvg = pPack->pEntries[iEntry].pSvg;
		iRet = xgeSvgAddRef(pSvg);
	}
	xrtMutexUnlock(pPack->pCacheMutex);
	if ( iRet != XGE_OK ) return iRet;
    if ( bScreenSpace ) {
        xge_rect_i_t tPixelClip = xgeClipGetPixels();
        if ( __xgeTextOutsidePixelClip(tDst, tPixelClip) ) {
            xge_rect_t tPaintBounds;
            /* A custom SVG may paint outside its nominal viewport. */
            if ( xgeSvgGetDrawBounds(pSvg, tDst, 0.35f, &tPaintBounds) == XGE_OK &&
                 __xgeTextOutsidePixelClip(tPaintBounds, tPixelClip) ) {
                xgeSvgDestroy(pSvg);
                return XGE_OK;
            }
        }
    }
	iRet = bScreenSpace ? xgeSvgDrawPx(pSvg, tDst, 0.35f) : xgeSvgDraw(pSvg, tDst, 0.35f);
	xgeSvgDestroy(pSvg);
	return iRet;
}
