static unsigned long long collectAssertions,collectCases,collectFastCases,collectFallbackCases,collectErrorCases;
#define PCC_CHECK(e) do {++collectAssertions;if(!(e)){fprintf(stderr,"paint collect check %d: %s\n",__LINE__,#e);fflush(stderr);exit(1);}} while(0)
static void pccCompare(xge_render_target target,const xge_text_shape_desc_t* desc,float x,float y,xge_rect_i_t clip,int expected,int overflow) {
 unsigned long long start=paintCollectStarts,fast=paintCollectFast,fallback=paintCollectFallback;
 pstCompare(target,desc,x,y,clip,overflow);++collectCases;
 PCC_CHECK(paintCollectStarts==start+1);
 if(expected==1){PCC_CHECK(paintCollectFast==fast+1 && paintCollectFallback==fallback);++collectFastCases;}
 else if(expected==0){PCC_CHECK(paintCollectFast==fast && paintCollectFallback==fallback+1);++collectFallbackCases;}
 else {PCC_CHECK(paintCollectFast==fast && paintCollectFallback==fallback);++collectErrorCases;}
 PCC_CHECK(paintCollectMaxCandidates<=4096);
}
static void pccReplaySvgFailure(xge_render_target target,xge_font font,xge_emoji_pack pack,const char* svg) {
 xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=font;d.pEmojiPack=pack;d.sText="A\xf0\x9f\x98\x80\xe4\xb8\xad";d.iTextSize=8;d.iFlags=XGE_TEXT_SHAPE_DEFAULT;d.fEmojiScale=1.25f;
 xge_rect_i_t clip={8,8,496,112};void* caches[2]={0};unsigned long long fast=paintCollectFast,fallback=paintCollectFallback;
 xge_emoji_metrics_t metrics=pack->pEntries[0].tMetrics;int fontRefs=font->iRefCount;
 for(int mode=0;mode<2;++mode){xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,0xffefefefu);PCC_CHECK(xgePassBegin(&pass)==XGE_OK);xgeClipSetPixels(clip);
  if(mode==0){xge_glyph_run_t run={0};PCC_CHECK(xgeTextShape(&d,&run)==XGE_OK);caches[0]=__xgeGlyphRunPaintCacheCaptureDraw(&run,-25.75f,9.5f,0xff102030u,NULL,0);xgeGlyphRunFree(&run);}
  else PCC_CHECK(xgeInternalTextPaintStream(&d,-25.75f,9.5f,0xff102030u,NULL,0,caches+1)==XGE_OK);
  PCC_CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();PCC_CHECK(xgeRenderTargetReadPixels(target,pstPixels[mode],512*4)==XGE_OK);
 }
 PCC_CHECK(!memcmp(pstPixels[0],pstPixels[1],sizeof(pstPixels[0])));pstCacheEqual(caches[0],caches[1]);PCC_CHECK(caches[0]);
 xge_glyph_paint_cache_t* cache=caches[0];int retained=0;for(int i=0;i<cache->tRun.iGlyphCount;++i)if(cache->tRun.pGlyphs[i].iItemKind==XGE_TEXT_ITEM_EMOJI && cache->tRun.pGlyphs[i].iCluster==1)retained=1;PCC_CHECK(retained);
 /* Between draws replace the SVG without changing its shaping metrics/ID. */
 const char* broken="not SVG";uint32_t id=0;PCC_CHECK(xgeEmojiPackAddSvgMemory(pack,"\xf0\x9f\x98\x80",broken,(int)strlen(broken),&metrics,&id)==XGE_OK && id==1);
 for(int mode=0;mode<2;++mode){xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,0xffefefefu);PCC_CHECK(xgePassBegin(&pass)==XGE_OK);xgeClipSetPixels(clip);__xgeGlyphRunPaintCacheDraw(caches[mode],0xff102030u,NULL,0);PCC_CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();PCC_CHECK(xgeRenderTargetReadPixels(target,pstPixels[mode],512*4)==XGE_OK);}
 PCC_CHECK(!memcmp(pstPixels[0],pstPixels[1],sizeof(pstPixels[0])));
 PCC_CHECK(xgeEmojiPackAddSvgMemory(pack,"\xf0\x9f\x98\x80",svg,(int)strlen(svg),&metrics,&id)==XGE_OK && id==1);
 __xgeGlyphRunPaintCacheFree(caches[0]);__xgeGlyphRunPaintCacheFree(caches[1]);PCC_CHECK(font->iRefCount==fontRefs);
 PCC_CHECK(paintCollectFast==fast+1 && paintCollectFallback==fallback);++collectCases;++collectFastCases;
}
static void verifyPaintCollect(char** args) {
 unsigned long long previous=paintStreamAssertions;
 PCC_CHECK(paintCollectFast>=20 && paintCollectFallback>=20);
 xge_render_target_t target={0};PCC_CHECK(xgeRenderTargetCreate(&target,512,128)==XGE_OK);
 xge_emoji_pack pack=NULL,tiny=NULL;PCC_CHECK(xgeEmojiPackCreate(&pack)==XGE_OK && xgeEmojiPackCreate(&tiny)==XGE_OK);
 const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><rect x='1' y='1' width='30' height='30' fill='#ff8800'/></svg>";
 xge_emoji_metrics_t em={0};em.iSize=sizeof(em);em.fAdvanceEm=em.fWidthEm=1;em.fHeightEm=2;em.fBaselineRatio=.9f;uint32_t id=0;
 PCC_CHECK(xgeEmojiPackAddSvgMemory(pack,"\xf0\x9f\x98\x80",svg,(int)strlen(svg),&em,&id)==XGE_OK);
 em.fAdvanceEm=.000001f;PCC_CHECK(xgeEmojiPackAddSvgMemory(tiny,"\xf0\x9f\x98\x80",svg,(int)strlen(svg),&em,&id)==XGE_OK);
 char* text=malloc(20001);PCC_CHECK(text);memset(text,'V',8192);memcpy(text+8192,"\xe4\xb8\xad\xf0\x9f\x98\x80",7);text[8199]=0;
 xge_rect_i_t clip={8,8,496,112};float sizes[]={8.5f,16,24,39.75f};float origins[]={12.25f,-20.75f,-9999.25f,510.5f};
 for(int sz=0;sz<4;++sz){xge_font_t font={0},fallback={0};PCC_CHECK(xgeFontLoad(&font,args[2],sizes[sz])==XGE_OK && xgeFontLoad(&fallback,args[3],sizes[sz]*1.8f)==XGE_OK);font.pFallback=&fallback;
  xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=&font;d.pEmojiPack=pack;d.sText=text;d.iTextSize=8199;d.iFlags=XGE_TEXT_SHAPE_DEFAULT;d.iEmojiLinePolicy=XGE_EMOJI_LINE_EXPAND;d.fEmojiScale=1;
  for(int origin=0;origin<4;++origin)pccCompare(&target,&d,origins[origin],9.5f,clip,1,0);
  if(sz==1){
   xge_glyph_run_t run={0};PCC_CHECK(xgeTextShape(&d,&run)==XGE_OK);float pen=__xgeTextSnapPixel(12.25f);int found=0;
   for(int i=0;i<run.iGlyphCount;++i){if(run.pGlyphs[i].iItemKind==XGE_TEXT_ITEM_EMOJI){found=1;break;}pen+=run.pGlyphs[i].fAdvanceX;}PCC_CHECK(found && pen>5000);xgeGlyphRunFree(&run);
   xrtMutexLock(pack->pCacheMutex);PCC_CHECK(__xgeEmojiEntryLoadLocked(pack,0)==XGE_OK);xge_svg shape=pack->pEntries[0].pSvg;PCC_CHECK(xgeSvgAddRef(shape)==XGE_OK);xrtMutexUnlock(pack->pCacheMutex);
   xge_svg_paint picture=NULL;PCC_CHECK(xgeSvgPaintGetPicture(shape,&picture)==XGE_OK);
   /* Runtime shearing mixes final Y into X: use the original second visitor. */
   xge_shape_ex_matrix_t transform={1,0,.25f,1,-pen+80,0};PCC_CHECK(xgeSvgPaintTransformSet(picture,&transform)==XGE_OK);
   pccCompare(&target,&d,12.25f,9.5f,clip,0,0);
   /* Translation can move a far-off nominal emoji back into the visible area. */
   transform.fC=0;PCC_CHECK(xgeSvgPaintTransformSet(picture,&transform)==XGE_OK);pccCompare(&target,&d,12.25f,9.5f,clip,1,0);
   PCC_CHECK(xgeSvgPaintTransformIdentity(picture)==XGE_OK);pccCompare(&target,&d,12.25f,9.5f,clip,1,0);xgeSvgDestroy(shape);
   pccCompare(&target,&d,12.25f,9.5f,(xge_rect_i_t){0},0,0);
   d.sText="AV\r\nTA\xe4\xb8\xad\xf0\x9f\x98\x80";d.iTextSize=-1;pccCompare(&target,&d,12.25f,9.5f,clip,0,0);
   d.sText="A\xff";d.iTextSize=2;pccCompare(&target,&d,12.25f,9.5f,clip,-1,0);
   for(int i=0;i<5000;++i){memcpy(text+i*4,"\xf0\x9f\x98\x80",4);}d.sText=text;d.iTextSize=20000;d.pEmojiPack=tiny;d.iEmojiLinePolicy=XGE_EMOJI_LINE_STABLE;
   pccCompare(&target,&d,12.25f,9.5f,clip,0,1);
   memset(text,'V',8192);memcpy(text+8192,"\xe4\xb8\xad\xf0\x9f\x98\x80",7);text[8199]=0;
  }
  if(sz==2)pccReplaySvgFailure(&target,&font,pack,svg);
  font.pFallback=NULL;xgeFontFree(&font);xgeFontFree(&fallback);
 }
 free(text);xgeEmojiPackFree(pack);xgeEmojiPackFree(tiny);xgeRenderTargetFree(&target);
 PCC_CHECK(collectCases==24 && collectFastCases==19 && collectFallbackCases==4 && collectErrorCases==1);
 printf("paint collect comparison passed: %llu assertions; %llu cases; %llu fast cases; %llu fallback cases; %llu error cases; %d maximum candidates; 1 SVG failure replay case\n",collectAssertions+paintStreamAssertions-previous,collectCases,collectFastCases,collectFallbackCases,collectErrorCases,paintCollectMaxCandidates);fflush(stdout);
}
