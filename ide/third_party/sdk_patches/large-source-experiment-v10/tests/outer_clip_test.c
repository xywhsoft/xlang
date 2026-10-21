#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
static unsigned long long checks,fastCulls,pixelCases;
static int started,failed;
#define CHECK(e) do { ++checks; if(!(e)) { fprintf(stderr,"outer clip check %d: %s\n",__LINE__,#e); fflush(stderr); exit(1); } } while(0)
static const char* svgs[]={
 "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><rect width='32' height='32' fill='#ff0000'/></svg>",
 "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><rect x='-200' y='-200' width='432' height='432' fill='#0080ff' stroke='#ff0000' stroke-width='12'/></svg>",
 "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><g transform='translate(8 8) rotate(45)'><circle r='24' fill='#8000ff'/><rect x='-12' y='-12' width='24' height='24' fill='#00ff00'/></g></svg>",
 "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><defs><linearGradient id='g'><stop stop-color='#ff0000'/><stop offset='1' stop-color='#0000ff'/></linearGradient></defs><rect x='-10' y='-10' width='52' height='52' fill='url(#g)'/></svg>",
 "<svg xmlns='http://www.w3.org/2000/svg' viewBox='-8 -8 32 32' preserveAspectRatio='none'><path d='M -40 -40 L 64 0 L 0 64 Z' fill='#ff8000' stroke='#004080' stroke-width='4'/></svg>",
 "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><defs><clipPath id='c'><circle cx='16' cy='16' r='12'/></clipPath></defs><g clip-path='url(#c)'><rect x='-100' y='-100' width='300' height='300' fill='#008080'/></g></svg>"
};
static const xge_shape_ex_matrix_t matrices[]={
 {1,0,0,1,0,0},{1,0,0,1,200,0},{1,0,0,1,-200,20},{2,0,0,0.5f,0,0},
 {-1,0,0,1,128,0},{0,1,-1,0,128,0},{1,0.25f,0.5f,1,0,0},{0.75f,-0.5f,0.25f,1.25f,10,-12}
};
static const xge_rect_i_t clips[]={
 {40,30,60,40},{0,0,128,96},{-32,-24,16,16},{64,48,1,1},
 {0,0,0,0},{40,30,-1,40},{-33554432,-33554432,100,100},{33554400,33554400,100,100}
};
static int outside(xge_rect_t a,xge_rect_i_t b) {return __xgeTextOutsidePixelClip(a,b);}
static void geometry(void) {
 xge_rect_t b={0};CHECK(__xgeSvgDrawOuterClipBounds(NULL,(xge_rect_t){0,0,32,32},&b)==XGE_ERROR_INVALID_ARGUMENT);
 for(size_t corpus=0;corpus<sizeof(svgs)/sizeof(svgs[0]);++corpus) {
  xge_svg svg=NULL;CHECK(xgeSvgCreate(&svg)==XGE_OK);CHECK(xgeSvgLoadMemory(svg,svgs[corpus],(int)strlen(svgs[corpus]))==XGE_OK);
  CHECK(__xgeSvgDrawOuterClipBounds(svg,(xge_rect_t){0,0,32,32},NULL)==XGE_ERROR_INVALID_ARGUMENT);
  CHECK(__xgeSvgDrawOuterClipBounds(svg,(xge_rect_t){0,0,-1,32},&b)==XGE_ERROR_INVALID_ARGUMENT);
  CHECK(__xgeSvgDrawOuterClipBounds(svg,(xge_rect_t){0,0,32,0},&b)==XGE_ERROR_INVALID_ARGUMENT);
  for(size_t matrix=0;matrix<sizeof(matrices)/sizeof(matrices[0]);++matrix) {
   CHECK(xgeSvgPaintTransformSet(&svg->tPicturePaint,&matrices[matrix])==XGE_OK);
   for(int layer=1;layer<4;++layer) {
    xge_svg_draw_item_t* paint=layer==1 ? &svg->tRootPaint : layer==2 ? &svg->tContentPaint : &svg->tDocumentPaint;
    CHECK(xgeSvgPaintTransformIdentity(paint)==XGE_OK);
    if(matrix==7) {xge_shape_ex_matrix_t chained={1,0,0,1,(float)(layer*3),(float)(layer*-2)};CHECK(xgeSvgPaintTransformSet(paint,&chained)==XGE_OK);}
   }
   for(int at=0;at<64;++at) {
    float x=(float)(at*13-256),y=(float)(at%7*16-32);if(at==62)x=-33554432;if(at==63)x=33554432;
    xge_rect_t dst={x,y,(float)(16+at%5*8),(float)(16+at%3*8)},outer={0},full={0};
    CHECK(__xgeSvgDrawOuterClipBounds(svg,dst,&outer)==XGE_OK);CHECK(xgeSvgGetDrawBounds(svg,dst,0.35f,&full)==XGE_OK);
    if(full.fW>0 && full.fH>0) CHECK((double)full.fX>=outer.fX-0.01 && (double)full.fY>=outer.fY-0.01 && (double)full.fX+full.fW<=(double)outer.fX+outer.fW+0.01 && (double)full.fY+full.fH<=(double)outer.fY+outer.fH+0.01);
    for(size_t clip=0;clip<sizeof(clips)/sizeof(clips[0]);++clip) {
     if(outside(dst,clips[clip]) && outside(outer,clips[clip])) {++fastCulls;CHECK(outside(full,clips[clip]) || full.fW<=0 || full.fH<=0);}
    }
   }
  }
  xge_rect_t saved=svg->tViewBox;svg->tViewBox.fW=0;CHECK(__xgeSvgDrawOuterClipBounds(svg,(xge_rect_t){0,0,32,32},&b)==XGE_ERROR_INVALID_ARGUMENT);svg->tViewBox=saved;
  xgeSvgDestroy(svg);
 }
 CHECK(fastCulls>10000);
 CHECK(!outside((xge_rect_t){INFINITY,0,32,32},clips[0]) && !outside((xge_rect_t){NAN,0,32,32},clips[0]));
 CHECK(!outside((xge_rect_t){0,0,-1,32},clips[0]));
}
static void render(xge_render_target target,xge_emoji_pack pack,uint32_t id,xge_rect_t dst,int old,unsigned char* pixels,int bScreenSpace) {
 xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,XGE_COLOR_RGBA(240,240,240,255));CHECK(xgePassBegin(&pass)==XGE_OK);
 xge_rect_i_t clip={40,30,60,40};xgeClipSetPixels(clip);
 int ret=old ? diagOldEmojiDraw(pack,id,dst,bScreenSpace) : __xgeEmojiDraw(pack,id,dst,bScreenSpace);CHECK(ret==XGE_OK);
 xge_rect_i_t after=xgeClipGetPixels();CHECK(after.iX==clip.iX && after.iY==clip.iY && after.iW==clip.iW && after.iH==clip.iH);
 CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();CHECK(xgeRenderTargetReadPixels(target,pixels,128*4)==XGE_OK);
}
static int verify(void* user) {
 if(started)return XGE_OK;
 started=1;const char* output=user;CHECK(xgeBegin()==XGE_OK);geometry();
 xge_render_target_t target={0};CHECK(xgeRenderTargetCreate(&target,128,96)==XGE_OK);
 unsigned char* a=xrtMalloc(128*96*4);unsigned char* b=xrtMalloc(128*96*4);CHECK(a && b);
 unsigned visible=0,transformedVisible=0;
 const xge_rect_t positions[]={{50,40,32,32},{-160,32,32,32},{-256,32,32,32},{200,32,32,32},{20,20,40,40},{96,60,32,32},{-33554432,32,32,32},{33554432,32,32,32}};
 for(size_t corpus=0;corpus<sizeof(svgs)/sizeof(svgs[0]);++corpus) {
  xge_emoji_pack pack=NULL;CHECK(xgeEmojiPackCreate(&pack)==XGE_OK);xge_emoji_metrics_t metric={0};metric.iSize=sizeof(metric);metric.fAdvanceEm=metric.fWidthEm=metric.fHeightEm=1;metric.fBaselineRatio=0.9f;
  uint32_t id=0;CHECK(xgeEmojiPackAddSvgMemory(pack,"\xf0\x9f\x98\x80",svgs[corpus],(int)strlen(svgs[corpus]),&metric,&id)==XGE_OK && id);
  xrtMutexLock(pack->pCacheMutex);int loaded=__xgeEmojiEntryLoadLocked(pack,(int)id-1);xrtMutexUnlock(pack->pCacheMutex);CHECK(loaded==XGE_OK);xge_svg svg=pack->pEntries[id-1].pSvg;
  for(size_t matrix=0;matrix<sizeof(matrices)/sizeof(matrices[0]);++matrix) {
   CHECK(xgeSvgPaintTransformSet(&svg->tPicturePaint,&matrices[matrix])==XGE_OK);
   for(size_t at=0;at<sizeof(positions)/sizeof(positions[0]);++at) {
    for(int screen=0;screen<2;++screen) {
     render(&target,pack,id,positions[at],1,a,screen);render(&target,pack,id,positions[at],0,b,screen);CHECK(!memcmp(a,b,128*96*4));++pixelCases;
     int count=0;for(int pixel=0;pixel<128*96;++pixel)if(memcmp(b+pixel*4,b,4))++count;
     if(corpus==0 && matrix==0 && at==0 && screen) {CHECK(count>100);++visible;}
     if(corpus==0 && matrix==1 && at==1 && screen) {CHECK(count>100);++transformedVisible;}
     if(corpus==0 && matrix==1 && at==1 && screen) {
      char path[1024];snprintf(path,sizeof(path),"%s/native-outer-clip-old.png",output);CHECK(stbi_write_png(path,128,96,4,a,128*4));
      snprintf(path,sizeof(path),"%s/native-outer-clip-new.png",output);CHECK(stbi_write_png(path,128,96,4,b,128*4));
     }
    }
   }
  }
  xgeEmojiPackFree(pack);
 }
 CHECK(visible==1 && transformedVisible==1);xrtFree(a);xrtFree(b);xgeRenderTargetFree(&target);CHECK(xgeEnd()==XGE_OK);
 printf("outer clip verification passed: %llu assertions; %llu geometric cull proofs; %llu old/new pixel cases; visible nominal-offscreen root-transform control; one executable XRT owner\n",checks,fastCulls,pixelCases);fflush(stdout);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 xge_desc_t desc={0};desc.iWidth=256;desc.iHeight=192;desc.sTitle="Private SVG outer-clip verification";desc.iFlags=XGE_INIT_WINDOW;desc.iRunMode=XGE_RUN_GAME_LOOP;
 int code=xgeInit(&desc);if(code==XGE_OK)code=xgeRun(verify,argv[1]);xgeUnit();return code==XGE_OK && started && !failed ? 0 : 1;
}
