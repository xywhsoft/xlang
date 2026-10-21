#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static unsigned long long assertions,shapeCases,lookupCases,pixelCases,visibleControls;
static int started;
#define CHECK(e) do { ++assertions; if(!(e)) { fprintf(stderr,"shape lookup check %d: %s\n",__LINE__,#e); fflush(stderr); exit(1); } } while(0)
static void runsEqual(xge_glyph_run_t* a,xge_glyph_run_t* b) {
 CHECK(a->iSize==b->iSize && a->iFlags==b->iFlags && a->iTextSize==b->iTextSize && a->iGlyphCount==b->iGlyphCount);
 CHECK(a->fWidth==b->fWidth && a->fHeight==b->fHeight && a->fAscent==b->fAscent && a->fDescent==b->fDescent && a->fLineHeight==b->fLineHeight);
 for(int i=0;i<a->iGlyphCount;++i) CHECK(!memcmp(&a->pGlyphs[i],&b->pGlyphs[i],sizeof(a->pGlyphs[i])));
 xge_glyph_run_backend_t* x=a->pBackend; xge_glyph_run_backend_t* y=b->pBackend;
 CHECK((x==NULL)==(y==NULL));
 if(x) {CHECK(x->iFontCount==y->iFontCount && x->pEmojiPack==y->pEmojiPack);for(int i=0;i<x->iFontCount;++i)CHECK(x->ppFonts[i]==y->ppFonts[i]);}
}
static void compare(xge_font font,xge_emoji_pack pack,const char* text,int length,uint32_t flags,int presentation,int policy,xge_render_target target,int draw) {
 xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=font;d.pEmojiPack=pack;d.sText=text;d.iTextSize=length;d.iFlags=flags;d.iEmojiPresentation=presentation;d.iEmojiLinePolicy=policy;d.fEmojiScale=1.25f;
 xge_glyph_run_t old={0},now={0};int refs=font->iRefCount;int a=diagOldTextShape(&d,&old),b=xgeTextShape(&d,&now);CHECK(a==b);++shapeCases;
 if(a==XGE_OK)runsEqual(&old,&now);
 compareStream(&d,&old,a);
 if(draw && a==XGE_OK) {
  unsigned char x[256*128*4],y[256*128*4];xge_glyph_run_t* runs[]={&old,&now};unsigned char* buffers[]={x,y};
  for(int i=0;i<2;++i) {xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,XGE_COLOR_RGBA(240,240,240,255));CHECK(xgePassBegin(&pass)==XGE_OK);xgeClipSetPixels((xge_rect_i_t){8,8,240,112});xgeGlyphRunDraw(runs[i],12,20,0xff102030u,XGE_DRAW_SCREEN_SPACE);CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();CHECK(xgeRenderTargetReadPixels(target,buffers[i],256*4)==XGE_OK);}
  CHECK(!memcmp(x,y,sizeof(x)));++pixelCases;
  if(length==-1 && text[0]=='A') {unsigned count=0;for(size_t p=0;p<sizeof(y);p+=4)if(memcmp(y+p,y,4))++count;CHECK(count>50);++visibleControls;}

 }
 xgeGlyphRunFree(&old);xgeGlyphRunFree(&now);CHECK(font->iRefCount==refs);
}
static void mapping(xge_font font) {
 xge_text_shape_lookup_t cache[XGE_TEXT_SHAPE_LOOKUP_SLOTS]={0};CHECK(sizeof(cache)==2048);
 for(uint32_t cp=0;cp<=0x10ffffu;++cp) {
  if(cp>=0xd800u && cp<=0xdfffu)continue;
  int oldGlyph=-77,newGlyph=-77;xge_font a=__xgeFontResolveCodepoint(font,cp,&oldGlyph);xge_font b=__xgeTextShapeResolveCodepoint(font,cp,&newGlyph,cache);CHECK(a==b && oldGlyph==newGlyph);++lookupCases;
  if(cp%113u==0) {int again=-77;CHECK(__xgeTextShapeResolveCodepoint(font,cp,&again,cache)==a && again==oldGlyph);++lookupCases;}
 }
}
static int verify(void* user) {
 if(started)return XGE_OK;
 started=1;char** args=user;CHECK(xgeBegin()==XGE_OK);
 xge_render_target_t target={0};CHECK(xgeRenderTargetCreate(&target,256,128)==XGE_OK);
 xge_emoji_pack pack=NULL;CHECK(xgeEmojiPackCreate(&pack)==XGE_OK);xge_emoji_metrics_t em={0};em.iSize=sizeof(em);em.fAdvanceEm=em.fWidthEm=em.fHeightEm=1;em.fBaselineRatio=0.9f;
 const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><circle cx='16' cy='16' r='14' fill='#ff8800'/></svg>";
 uint32_t emoji=0;CHECK(xgeEmojiPackAddSvgMemory(pack,"\xf0\x9f\x98\x80",svg,(int)strlen(svg),&em,&emoji)==XGE_OK);CHECK(xgeEmojiPackAddSvgMemory(pack,"\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb",svg,(int)strlen(svg),&em,&emoji)==XGE_OK);
 const char* corpora[]={"", "AVATAR 0123456789\t\r\nC header\rnext", "\xe4\xb8\xad\xe6\x96\x87\xf0\x9f\x98\x80 e\xcc\x81 \xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb", "\xd7\x90\xd7\x91 Arabic \xd8\xa7\xd8\xa8 \xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7", "\xf0\x90\x80\x80 \xf4\x8f\xbf\xbf", "\xff", "\xc0\xaf", "\xed\xa0\x80", "\xf0\x9f"};
 int lengths[]={1,127,255,256,257,1024,8192};float sizes[]={8,16,20,32};
 char* text=malloc(1048577);CHECK(text);
 for(int size=0;size<4;++size) {
  xge_font_t latin={0},cjk={0};CHECK(xgeFontLoad(&latin,args[2],sizes[size])==XGE_OK);CHECK(xgeFontLoad(&cjk,args[3],sizes[size])==XGE_OK);
  /* Change fallback order between complete calls; no local table can survive. */
  for(int chain=0;chain<3;++chain) {
   xge_font font=chain==2 ? &cjk : &latin;latin.pFallback=chain==1 ? &cjk : NULL;
   if(size==1)mapping(font);
   for(size_t corpus=0;corpus<sizeof(corpora)/sizeof(corpora[0]);++corpus) {
    size_t n=strlen(corpora[corpus]);
    compare(font,pack,corpora[corpus],-1,XGE_TEXT_SHAPE_DEFAULT,XGE_EMOJI_PRESENTATION_AUTO,XGE_EMOJI_LINE_STABLE,&target,1);
    if(!n)continue;
    for(size_t limit=0;limit<sizeof(lengths)/sizeof(lengths[0]);++limit) {
     int used=0;while(used+(int)n<=lengths[limit]) {memcpy(text+used,corpora[corpus],n);used+=(int)n;}text[used]=0;
     for(int flags=0;flags<4;++flags)compare(font,pack,text,used,(uint32_t)flags,flags==3 ? XGE_EMOJI_PRESENTATION_TEXT : XGE_EMOJI_PRESENTATION_AUTO,flags%2 ? XGE_EMOJI_LINE_EXPAND : XGE_EMOJI_LINE_STABLE,&target,limit==5 && flags==3);
    }
   }
   const char embedded[]={'A',0,'V','\n','B',0};compare(font,pack,embedded,5,0,XGE_EMOJI_PRESENTATION_DISABLED,XGE_EMOJI_LINE_STABLE,&target,0);
   memset(text,'A',255);text[255]=(char)0xff;text[256]=0;compare(font,pack,text,256,XGE_TEXT_SHAPE_DEFAULT,0,0,&target,0);
  }
  latin.pFallback=NULL;xgeFontFree(&latin);xgeFontFree(&cjk);
 }
 xge_font_t font={0};CHECK(xgeFontLoad(&font,args[2],20)==XGE_OK);
 const char* motif=corpora[2];size_t n=strlen(motif),used=0;while(used+n<=1048576) {memcpy(text+used,motif,n);used+=n;}text[used]=0;
 compare(&font,pack,text,(int)used,XGE_TEXT_SHAPE_DEFAULT,0,0,&target,0);
 /* No-backend and depth-limited cyclic fallback misses retain the old output. */
 xge_font_t invalid={0};invalid.pFallback=&invalid;memset(text,'A',512);text[512]=0;compare(&invalid,pack,text,512,0,0,0,&target,0);invalid.pFallback=NULL;
 xge_text_shape_desc_t desc={0};xge_glyph_run_t result={0};CHECK(xgeTextShape(NULL,&result)==diagOldTextShape(NULL,&result));CHECK(xgeTextShape(&desc,&result)==diagOldTextShape(&desc,&result));desc.iSize=sizeof(desc);desc.pFont=&font;desc.sText="x";desc.iTextSize=-2;CHECK(xgeTextShape(&desc,&result)==diagOldTextShape(&desc,&result));
 verifyLayoutGeometry(args);
 verifyLayoutProviderLifecycle(args);
 CHECK(visibleControls==12);xgeFontFree(&font);free(text);xgeEmojiPackFree(pack);xgeRenderTargetFree(&target);CHECK(xgeEnd()==XGE_OK);
 printf("shape lookup comparison passed: %llu assertions; %llu complete shape cases; %llu scalar lookup checks; %llu old/new pixel cases; 2048 bytes call-local table; one executable XRT owner\n",assertions,shapeCases,lookupCases,pixelCases);fflush(stdout);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 xge_desc_t d={0};d.iWidth=256;d.iHeight=192;d.sTitle="Private shape lookup comparison";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
 int code=xgeInit(&d);if(code==XGE_OK)code=xgeRun(verify,argv);xgeUnit();return code==XGE_OK && started ? 0 : 1;
}
