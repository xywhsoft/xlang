#include <stdio.h>
#include <stdlib.h>
static unsigned long long paintStreamAssertions,paintStreamCases,paintStreamPixels,paintStreamReuses;
static int paintStreamOverflow,paintStreamPinned;
#define PST_CHECK(e) do {++paintStreamAssertions;if(!(e)){fprintf(stderr,"paint stream check %d: %s\n",__LINE__,#e);fflush(stderr);exit(1);}} while(0)
static unsigned char pstPixels[2][512*128*4];
static void pstCacheEqual(void* a,void* b) {
 PST_CHECK((a==NULL)==(b==NULL));if(!a)return;
 xge_glyph_paint_cache_t* x=a; xge_glyph_paint_cache_t* y=b;
 PST_CHECK(x->bValid && y->bValid && x->iCapacity<=4096 && y->iCapacity<=4096);
 const xge_glyph_run_t* u=&x->tRun;const xge_glyph_run_t* v=&y->tRun;
 PST_CHECK(u->iSize==v->iSize && u->iFlags==v->iFlags && u->iTextSize==v->iTextSize && u->iGlyphCount==v->iGlyphCount);
 PST_CHECK(u->fWidth==v->fWidth && u->fHeight==v->fHeight && u->fAscent==v->fAscent && u->fDescent==v->fDescent && u->fLineHeight==v->fLineHeight);
 PST_CHECK(!memcmp(&x->tClip,&y->tClip,sizeof(x->tClip)));
 for(int i=0;i<u->iGlyphCount;++i){PST_CHECK(!memcmp(u->pGlyphs+i,v->pGlyphs+i,sizeof(*u->pGlyphs)));PST_CHECK(!memcmp(x->pPens+i,y->pPens+i,sizeof(*x->pPens)));}
 xge_glyph_run_backend_t* p=u->pBackend; xge_glyph_run_backend_t* q=v->pBackend;
 PST_CHECK(p && q && p->iFontCount==q->iFontCount && p->pEmojiPack==q->pEmojiPack);
 for(int i=0;i<p->iFontCount;++i)PST_CHECK(p->ppFonts[i]==q->ppFonts[i]);
}
static void pstCompare(xge_render_target target,const xge_text_shape_desc_t* desc,float x,float y,xge_rect_i_t clip,int expectOverflow) {
 xge_text_paint_span_t spans[5]={{sizeof(xge_text_paint_span_t),-3,4,0xff0088ffu},{sizeof(xge_text_paint_span_t),2,17,0xff990077u},{sizeof(xge_text_paint_span_t),5,5,0xff001100u},{sizeof(xge_text_paint_span_t),20,20000000,0xff994400u},{sizeof(xge_text_paint_span_t),3,7,0xff33aa55u}};
 void* caches[2]={0};int codes[2]={0};int fontRefs=desc->pFont->iRefCount;
 int fallbackRefs=desc->pFont->pFallback?desc->pFont->pFallback->iRefCount:0;
 int packRefs=desc->pEmojiPack?desc->pEmojiPack->iRefCount:0;
 /* Alternate which implementation sees a cold glyph/SVG atlas first. */
 for(int step=0;step<2;++step){int mode=(int)((paintStreamCases+step)%2);
  xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,0xffefefefu);PST_CHECK(xgePassBegin(&pass)==XGE_OK);
  if(clip.iW>0 && clip.iH>0)xgeClipSetPixels(clip);else xgeClipClear();
  if(mode==0){xge_glyph_run_t run={0};codes[mode]=xgeTextShape(desc,&run);if(codes[mode]==XGE_OK)caches[mode]=__xgeGlyphRunPaintCacheCaptureDraw(&run,x,y,0xff102030u,spans,5);xgeGlyphRunFree(&run);}
  else codes[mode]=xgeInternalTextPaintStream(desc,x,y,0xff102030u,spans,5,caches+mode);
  PST_CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();PST_CHECK(xgeRenderTargetReadPixels(target,pstPixels[mode],512*4)==XGE_OK);
 }
 PST_CHECK(codes[0]==codes[1]);PST_CHECK(!memcmp(pstPixels[0],pstPixels[1],sizeof(pstPixels[0])));++paintStreamPixels;++paintStreamCases;
 pstCacheEqual(caches[0],caches[1]);
 if(expectOverflow){PST_CHECK(codes[0]==XGE_OK && !caches[0] && !caches[1]);++paintStreamOverflow;}
 if(caches[0]){
  for(int mode=0;mode<2;++mode){xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,0xffefefefu);PST_CHECK(xgePassBegin(&pass)==XGE_OK);xgeClipSetPixels(clip);
   __xgeGlyphRunPaintCacheDraw(caches[mode],0xff779911u,spans+1,4);
   PST_CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();PST_CHECK(xgeRenderTargetReadPixels(target,pstPixels[mode],512*4)==XGE_OK);}
  PST_CHECK(!memcmp(pstPixels[0],pstPixels[1],sizeof(pstPixels[0])));++paintStreamReuses;
 }
 __xgeGlyphRunPaintCacheFree(caches[0]);__xgeGlyphRunPaintCacheFree(caches[1]);
 PST_CHECK(desc->pFont->iRefCount==fontRefs);
 if(desc->pFont->pFallback)PST_CHECK(desc->pFont->pFallback->iRefCount==fallbackRefs);
 if(desc->pEmojiPack)PST_CHECK(desc->pEmojiPack->iRefCount==packRefs);
}
typedef struct pst_positions_t {xge_glyph_position_t positions[16];int count;} pst_positions_t;
static int pstCollect(const xge_glyph_position_t* p,void* user){pst_positions_t* out=user;PST_CHECK(out->count<16);out->positions[out->count++]=*p;return XGE_OK;}
static void pstPinnedDefault(xge_font font,xge_emoji_pack a,xge_emoji_pack b) {
 xge_text_shape_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=font;desc.sText="A\xf0\x9f\x98\x80" "A";desc.iTextSize=6;desc.iFlags=XGE_TEXT_SHAPE_DEFAULT;desc.fEmojiScale=1;
 PST_CHECK(xgeEmojiPackSetDefault(a)==XGE_OK);xge_glyph_run_t gold={0},visited={0},changed={0};PST_CHECK(xgeTextShape(&desc,&gold)==XGE_OK);
 PST_CHECK(gold.iGlyphCount==3 && gold.pGlyphs[0].iItemKind==XGE_TEXT_ITEM_GLYPH && gold.pGlyphs[2].iItemKind==XGE_TEXT_ITEM_EMOJI);
 PST_CHECK(xgeEmojiPackSetDefault(b)==XGE_OK);pst_positions_t positions={0};
 PST_CHECK(__xgeTextShape(&desc,&visited,pstCollect,&positions,NULL,NULL,NULL,a,1)==XGE_OK);
 PST_CHECK(positions.count==gold.iGlyphCount && !visited.pGlyphs);
 PST_CHECK(visited.fWidth==gold.fWidth && visited.fAscent==gold.fAscent && visited.fHeight==gold.fHeight);
 for(int i=0;i<positions.count;++i)PST_CHECK(!memcmp(positions.positions+i,gold.pGlyphs+i,sizeof(*gold.pGlyphs)));
 PST_CHECK(xgeTextShape(&desc,&changed)==XGE_OK && changed.fWidth!=gold.fWidth);
 xgeGlyphRunFree(&gold);xgeGlyphRunFree(&visited);xgeGlyphRunFree(&changed);++paintStreamPinned;
}
static void verifyPaintStream(char** args) {
 xge_render_target_t target={0};PST_CHECK(xgeRenderTargetCreate(&target,512,128)==XGE_OK);
 xge_emoji_pack saved=NULL,a=NULL,b=NULL,tiny=NULL,bad=NULL;PST_CHECK(xgeEmojiPackGetDefault(&saved)==XGE_OK);
 const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32' transform='translate(-16 0)'><rect width='48' height='32' fill='#ff8800'/></svg>";
 xge_emoji_metrics_t em={0};em.iSize=sizeof(em);em.fAdvanceEm=em.fWidthEm=1;em.fHeightEm=2;em.fBaselineRatio=.9f;
 PST_CHECK(xgeEmojiPackCreate(&a)==XGE_OK && xgeEmojiPackCreate(&b)==XGE_OK && xgeEmojiPackCreate(&tiny)==XGE_OK && xgeEmojiPackCreate(&bad)==XGE_OK);
 uint32_t id=0;PST_CHECK(xgeEmojiPackAddSvgMemory(a,"\xf0\x9f\x98\x80",svg,(int)strlen(svg),&em,&id)==XGE_OK);PST_CHECK(xgeEmojiPackAddSvgMemory(a,"A",svg,(int)strlen(svg),&em,&id)==XGE_OK);
 em.fAdvanceEm=3;PST_CHECK(xgeEmojiPackAddSvgMemory(b,"\xf0\x9f\x98\x80",svg,(int)strlen(svg),&em,&id)==XGE_OK);PST_CHECK(xgeEmojiPackAddSvgMemory(b,"A",svg,(int)strlen(svg),&em,&id)==XGE_OK);
 em.fAdvanceEm=.000001f;PST_CHECK(xgeEmojiPackAddSvgMemory(tiny,"\xf0\x9f\x98\x80",svg,(int)strlen(svg),&em,&id)==XGE_OK);
 const char invalidSvg[]="this is not SVG";PST_CHECK(xgeEmojiPackAddSvgMemory(bad,"\xf0\x9f\x98\x80",invalidSvg,sizeof(invalidSvg)-1,&em,&id)==XGE_OK);
 const char* texts[]={"AVATAR office e\xcc\x81", "A\xf0\x9f\x98\x80" "A\xe4\xb8\xad", "AV\r\nTA\nR\tB", "", "\xff", "A\xf0\x9f"};
 float sizes[]={8.5f,16,24,39.75f};float origins[]={12.25f,-25.75f,510.5f};xge_rect_i_t clip={8,8,496,112};
 for(int size=0;size<4;++size){xge_font_t font={0},fallback={0};PST_CHECK(xgeFontLoad(&font,args[2],sizes[size])==XGE_OK);PST_CHECK(xgeFontLoad(&fallback,args[3],sizes[size]*1.8f)==XGE_OK);font.pFallback=&fallback;
  PST_CHECK(xgeEmojiPackSetDefault(a)==XGE_OK);
  for(int corpus=0;corpus<6;++corpus)for(int origin=0;origin<3;++origin){xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=&font;d.sText=texts[corpus];d.iTextSize=-1;d.iFlags=XGE_TEXT_SHAPE_DEFAULT;d.iEmojiLinePolicy=size%2?XGE_EMOJI_LINE_EXPAND:XGE_EMOJI_LINE_STABLE;d.fEmojiScale=1.25f;
   pstCompare(&target,&d,origins[origin],9.5f,clip,0);
  }
  if(size==1){pstPinnedDefault(&font,a,b);PST_CHECK(xgeEmojiPackSetDefault(a)==XGE_OK);
   const char embedded[]={'A',0,'V','\n','B',0};xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=&font;d.sText=embedded;d.iTextSize=5;d.iFlags=XGE_TEXT_SHAPE_DEFAULT;d.fEmojiScale=1;
   pstCompare(&target,&d,12.25f,9.5f,clip,0);pstCompare(&target,&d,12.25f,9.5f,(xge_rect_i_t){0},0);
   d.sText="A\xf0\x9f\x98\x80";d.iTextSize=5;d.pEmojiPack=bad;pstCompare(&target,&d,12.25f,9.5f,clip,0);
   char* longText=malloc(2097161);PST_CHECK(longText);memset(longText,'V',2097152);memcpy(longText+2097152,"\xe4\xb8\xad\xf0\x9f\x98\x80",7);longText[2097159]=0;
   d.sText=longText;d.iTextSize=2097159;d.pEmojiPack=a;d.iEmojiLinePolicy=XGE_EMOJI_LINE_EXPAND;
   pstCompare(&target,&d,12.25f,9.5f,clip,0);pstCompare(&target,&d,-9999.25f,9.5f,clip,0);
   for(int i=0;i<5000;++i){memcpy(longText+i*4,"\xf0\x9f\x98\x80",4);}d.iTextSize=20000;d.pEmojiPack=tiny;d.iEmojiLinePolicy=XGE_EMOJI_LINE_STABLE;pstCompare(&target,&d,12.25f,9.5f,clip,1);free(longText);
  }
  font.pFallback=NULL;xgeFontFree(&font);xgeFontFree(&fallback);
 }
 void* invalid=NULL;PST_CHECK(xgeInternalTextPaintStream(NULL,0,0,0,NULL,0,&invalid)==XGE_ERROR_INVALID_ARGUMENT && !invalid);PST_CHECK(xgeInternalTextPaintStream(NULL,0,0,0,NULL,-1,&invalid)==XGE_ERROR_INVALID_ARGUMENT);
 if(saved)PST_CHECK(xgeEmojiPackSetDefault(saved)==XGE_OK);else xgeEmojiPackClearDefault();xgeEmojiPackFree(saved);xgeEmojiPackFree(a);xgeEmojiPackFree(b);xgeEmojiPackFree(tiny);xgeEmojiPackFree(bad);xgeRenderTargetFree(&target);
 PST_CHECK(paintStreamCases==78 && paintStreamPixels==78 && paintStreamReuses>30 && paintStreamOverflow==1 && paintStreamPinned==1);
 printf("paint stream comparison passed: %llu assertions; %llu cases; %llu pixel pairs; %llu cache replay pairs; %d overflow cases; %d pinned default cases\n",paintStreamAssertions,paintStreamCases,paintStreamPixels,paintStreamReuses,paintStreamOverflow,paintStreamPinned);fflush(stdout);
}
