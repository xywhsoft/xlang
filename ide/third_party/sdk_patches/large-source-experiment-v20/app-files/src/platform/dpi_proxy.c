#include "app_xrt.h"
#include "platform/dpi_proxy.h"
#include "xge.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#define FONT_MAGIC 0x5844464fu
#define SURFACE_MAGIC 0x58445355u
#define DRAW_MAGIC 0x58444452u
typedef struct dpi_font { unsigned magic; xui_font base,raw,pending; float size; struct dpi_font* next; } dpi_font;
typedef struct dpi_surface { unsigned magic; xui_surface raw,pending; xui_surface_desc_t desc; float factor; uint32_t generation; struct dpi_surface* next; } dpi_surface;
typedef struct dpi_draw { unsigned magic; xui_draw_context raw; dpi_surface* target; float scale; xui_rect_t clip; int clipped; } dpi_draw;
struct xide_dpi_proxy { xui_proxy_t raw; float scale; dpi_font* fonts; dpi_surface* surfaces; int draws; };
static xide_dpi_proxy* state(xui_proxy proxy) { return proxy ? proxy->pUser : NULL; }
static dpi_font* font(xui_font value) { dpi_font* f=(dpi_font*)value; return f && f->magic==FONT_MAGIC ? f : NULL; }
static dpi_surface* surface(xui_surface value) { dpi_surface* s=(dpi_surface*)value; return s && s->magic==SURFACE_MAGIC ? s : NULL; }
static dpi_draw* draw(xui_draw_context value) { dpi_draw* d=(dpi_draw*)value; return d && d->magic==DRAW_MAGIC ? d : NULL; }
static xui_font rawFont(xui_font f) { dpi_font* value=font(f); return value ? value->raw : NULL; }
static xui_surface rawSurface(xui_surface s) { dpi_surface* value=surface(s); return value ? value->raw : NULL; }
static xui_draw_context rawDraw(xui_draw_context d) { dpi_draw* value=draw(d); return value ? value->raw : NULL; }
static float surfaceScale(xui_surface s) { dpi_surface* value=surface(s); return value ? value->factor : 1; }
static float drawScale(xui_draw_context d) { dpi_draw* value=draw(d); return value ? value->scale : 1; }
static void bump(xui_surface s) { dpi_surface* value=surface(s); if(value && ++value->generation==0) value->generation=1; }
static int pixel(float value) { return (int)lroundf(value); }
static xui_rect_t rect(xui_rect_t r,float scale) { int right=pixel((r.fX+r.fW)*scale),bottom=pixel((r.fY+r.fH)*scale); r.fX=pixel(r.fX*scale); r.fY=pixel(r.fY*scale); r.fW=right-r.fX; r.fH=bottom-r.fY; return r; }
static xui_vec2_t vector(xui_vec2_t v,float scale) { v.fX*=scale; v.fY*=scale; return v; }
static int dimensions(xui_surface_desc_t logical,float scale,xui_surface_desc_t* physical)
{
    if(logical.iWidth<=0 || logical.iHeight<=0 || logical.iWidth>32768 || logical.iHeight>32768 || logical.iWidth*scale>32768 || logical.iHeight*scale>32768) return XUI_ERROR_INVALID_ARGUMENT;
    *physical=logical; physical->iWidth=pixel(logical.iWidth*scale); physical->iHeight=pixel(logical.iHeight*scale);
    if(physical->iWidth<1) { physical->iWidth=1; } if(physical->iHeight<1) physical->iHeight=1; return XUI_OK;
}
static int ownSurface(xide_dpi_proxy* s,xui_surface raw,xui_surface_desc_t desc,float factor,xui_surface* out)
{
    dpi_surface* item=calloc(1,sizeof(*item)); if(!item) { s->raw.surfaceDestroy(&s->raw,raw); return XUI_ERROR_OUT_OF_MEMORY; }
    item->magic=SURFACE_MAGIC; item->raw=raw; item->desc=desc; item->factor=factor; item->generation=1; item->next=s->surfaces; s->surfaces=item; *out=(xui_surface)item; return XUI_OK;
}
static int surfaceCreate(xui_proxy p,xui_surface* out,const xui_surface_desc_t* logical)
{
    xide_dpi_proxy* s=state(p); if(!s || !out || !logical) return XUI_ERROR_INVALID_ARGUMENT; *out=NULL;
    xui_surface_desc_t physical={0}; int result=dimensions(*logical,s->scale,&physical); xui_surface raw=NULL;
    if(result==XUI_OK) result=s->raw.surfaceCreate(&s->raw,&raw,&physical);
    return result==XUI_OK ? ownSurface(s,raw,*logical,s->scale,out) : result;
}
/* The pinned Windows OpenGL target upload rows are opposite its readback.
   Normalise incoming RGBA to the same top-left space used by UI drawing. */
static int surfaceCreateRGBA(xui_proxy p,xui_surface* out,int w,int h,const void* pixels,int stride,uint32_t flags)
{
    xide_dpi_proxy* s=state(p); if(!s || !out || w<=0 || h<=0 || w>32768 || h>32768) return XUI_ERROR_INVALID_ARGUMENT;
    *out=NULL; xui_surface raw=NULL; xui_surface_desc_t logical={XUI_SURFACE_KIND_TEXTURE,XUI_SURFACE_FORMAT_RGBA8,w,h,flags},physical=logical;
    int target=(flags&XUI_SURFACE_USAGE_TARGET)!=0,result=XUI_OK; unsigned char* upload=NULL;
    if(target) result=dimensions(logical,s->scale,&physical);
    if(result!=XUI_OK) return result;
    if(target && pixels) {
        if(stride<=0) stride=w*4;
        if(stride<w*4) return XUI_ERROR_INVALID_ARGUMENT;
        upload=malloc((size_t)physical.iWidth*physical.iHeight*4); if(!upload) return XUI_ERROR_OUT_OF_MEMORY;
        for(int y=0;y<physical.iHeight;++y) for(int x=0;x<physical.iWidth;++x) {
            int sx=(int)((int64_t)x*w/physical.iWidth),sy=(int)((int64_t)(physical.iHeight-1-y)*h/physical.iHeight);
            memcpy(upload+((size_t)y*physical.iWidth+x)*4,(const unsigned char*)pixels+(size_t)sy*stride+sx*4,4);
        }
        pixels=upload; stride=physical.iWidth*4;
    }
    result=s->raw.surfaceCreateRGBA(&s->raw,&raw,physical.iWidth,physical.iHeight,pixels,stride,flags); free(upload);
    xui_surface_desc_t desc={0}; if(result==XUI_OK) result=s->raw.surfaceGetDesc(&s->raw,raw,&desc);
    if(result!=XUI_OK) { if(raw) s->raw.surfaceDestroy(&s->raw,raw); return result; }
    desc.iWidth=w; desc.iHeight=h; return ownSurface(s,raw,desc,target ? s->scale : 1,out);
}
static int surfaceGetDesc(xui_proxy p,xui_surface value,xui_surface_desc_t* out)
{ if(!state(p) || !surface(value) || !out) return XUI_ERROR_INVALID_ARGUMENT; *out=surface(value)->desc; return XUI_OK; }
static int surfaceGetGeneration(xui_proxy p,xui_surface value,uint32_t* out)
{ if(!state(p) || !surface(value) || !out) return XUI_ERROR_INVALID_ARGUMENT; *out=surface(value)->generation; return XUI_OK; }
static void surfaceDestroy(xui_proxy p,xui_surface value)
{
    xide_dpi_proxy* s=state(p); dpi_surface* item=surface(value); if(!s || !item) return;
    dpi_surface** at=&s->surfaces; while(*at && *at!=item) at=&(*at)->next; if(*at) *at=item->next;
    s->raw.surfaceDestroy(&s->raw,item->raw); if(item->pending) s->raw.surfaceDestroy(&s->raw,item->pending); item->magic=0; free(item);
}
static int surfaceReadRGBA(xui_proxy p,xui_surface value,void* pixels,int stride)
{
    xide_dpi_proxy* s=state(p); dpi_surface* item=surface(value); if(!s || !item || !pixels) return XUI_ERROR_INVALID_ARGUMENT;
    if(stride<=0) stride=item->desc.iWidth*4;
    if(stride<item->desc.iWidth*4) return XUI_ERROR_INVALID_ARGUMENT;
    xui_surface_desc_t physical={0}; int result=s->raw.surfaceGetDesc(&s->raw,item->raw,&physical); if(result!=XUI_OK) return result;
    if(physical.iWidth==item->desc.iWidth && physical.iHeight==item->desc.iHeight) return s->raw.surfaceReadRGBA(&s->raw,item->raw,pixels,stride);
    size_t count=(size_t)physical.iWidth*physical.iHeight*4; unsigned char* bytes=malloc(count); if(!bytes) return XUI_ERROR_OUT_OF_MEMORY;
    result=s->raw.surfaceReadRGBA(&s->raw,item->raw,bytes,physical.iWidth*4);
    if(result==XUI_OK) for(int y=0;y<item->desc.iHeight;++y) for(int x=0;x<item->desc.iWidth;++x) {
        int sx=(int)(((int64_t)x*2+1)*physical.iWidth/(item->desc.iWidth*2)),sy=(int)(((int64_t)y*2+1)*physical.iHeight/(item->desc.iHeight*2));
        memcpy((unsigned char*)pixels+(size_t)y*stride+x*4,bytes+((size_t)sy*physical.iWidth+sx)*4,4);
    }
    free(bytes); return result;
}
static int surfaceUpdateRGBA(xui_proxy p,xui_surface value,xui_rect_i_t r,const void* pixels,int stride)
{
    xide_dpi_proxy* s=state(p); dpi_surface* item=surface(value); if(r.iW<=0 || r.iW>32768 || r.iH<=0 || r.iH>32768) return XUI_ERROR_INVALID_ARGUMENT;
    if(stride<=0) stride=r.iW*4;
    if(!s || !item || !pixels || r.iX<0 || r.iY<0 || r.iW<=0 || r.iH<=0 || r.iX>item->desc.iWidth-r.iW || r.iY>item->desc.iHeight-r.iH || stride<r.iW*4) return XUI_ERROR_INVALID_ARGUMENT;
    xui_rect_t mapped=rect((xui_rect_t){r.iX,r.iY,r.iW,r.iH},item->factor); xui_rect_i_t physical={mapped.fX,mapped.fY,mapped.fW,mapped.fH};
    if(physical.iW<=0 || physical.iH<=0) return XUI_OK;
    size_t count=(size_t)physical.iW*physical.iH*4; unsigned char* bytes=malloc(count); if(!bytes) return XUI_ERROR_OUT_OF_MEMORY;
    for(int y=0;y<physical.iH;++y) for(int x=0;x<physical.iW;++x) {
        int sx=(int)((int64_t)x*r.iW/physical.iW),sy=(int)((int64_t)((item->desc.iFlags&XUI_SURFACE_USAGE_TARGET) ? physical.iH-1-y : y)*r.iH/physical.iH);
        memcpy(bytes+((size_t)y*physical.iW+x)*4,(const unsigned char*)pixels+(size_t)sy*stride+sx*4,4);
    }
    if(item->desc.iFlags&XUI_SURFACE_USAGE_TARGET) { xui_surface_desc_t actual={0}; int read=s->raw.surfaceGetDesc(&s->raw,item->raw,&actual); if(read!=XUI_OK) { free(bytes); return read; } physical.iY=actual.iHeight-physical.iY-physical.iH; }
    int result=s->raw.surfaceUpdateRGBA(&s->raw,item->raw,physical,bytes,physical.iW*4); free(bytes); if(result==XUI_OK) bump(value); return result;
}
static int ownFont(xide_dpi_proxy* s,xui_font base,float size,xui_font* out)
{
    dpi_font* item=calloc(1,sizeof(*item)); if(!item) { s->raw.fontDestroy(&s->raw,base); return XUI_ERROR_OUT_OF_MEMORY; }
    item->base=base; item->raw=base; item->size=size;
    if(s->scale!=1) { int result=s->raw.fontCreateSized(&s->raw,&item->raw,base,size*s->scale); if(result!=XUI_OK) { s->raw.fontDestroy(&s->raw,base); free(item); return result; } }
    item->magic=FONT_MAGIC; item->next=s->fonts; s->fonts=item; *out=(xui_font)item; return XUI_OK;
}
static int fontLoadFile(xui_proxy p,xui_font* out,const char* path,float size,uint32_t flags)
{ xide_dpi_proxy* s=state(p); if(!s || !out) return XUI_ERROR_INVALID_ARGUMENT; *out=NULL; xui_font base=NULL; int result=s->raw.fontLoadFile(&s->raw,&base,path,size,flags); return result==XUI_OK ? ownFont(s,base,size,out) : result; }
static int fontLoadMemory(xui_proxy p,xui_font* out,const void* data,int length,float size,uint32_t flags)
{ xide_dpi_proxy* s=state(p); if(!s || !out) return XUI_ERROR_INVALID_ARGUMENT; *out=NULL; xui_font base=NULL; int result=s->raw.fontLoadMemory(&s->raw,&base,data,length,size,flags); return result==XUI_OK ? ownFont(s,base,size,out) : result; }
static int fontCreateSized(xui_proxy p,xui_font* out,xui_font source,float size)
{ xide_dpi_proxy* s=state(p); dpi_font* original=font(source); if(!s || !out || !original) return XUI_ERROR_INVALID_ARGUMENT; *out=NULL; xui_font base=NULL; int result=s->raw.fontCreateSized(&s->raw,&base,original->base,size); return result==XUI_OK ? ownFont(s,base,size,out) : result; }
/* Logical metrics remain stable while the separate draw instance is resized. */
static int fontGetMetrics(xui_proxy p,xui_font value,xui_font_metrics_t* metrics)
{ xide_dpi_proxy* s=state(p); dpi_font* f=font(value); return s && f ? s->raw.fontGetMetrics(&s->raw,f->base,metrics) : XUI_ERROR_INVALID_ARGUMENT; }
static int textMeasure(xui_proxy p,xui_font value,const char* text,xui_vec2_t* size)
{ xide_dpi_proxy* s=state(p); dpi_font* f=font(value); return s && f ? s->raw.textMeasure(&s->raw,f->base,text,size) : XUI_ERROR_INVALID_ARGUMENT; }
static int textShape(xui_proxy p,xui_font value,const char* text,int length,uint32_t flags,xui_text_shape_t* shaped)
{ xide_dpi_proxy* s=state(p); dpi_font* f=font(value); return s && f ? s->raw.textShape(&s->raw,f->base,text,length,flags,shaped) : XUI_ERROR_INVALID_ARGUMENT; }

int xideDpiVisitTextShape(xui_proxy p,xui_font value,const char* text,int length,uint32_t flags,
    xui_text_cluster_visit_proc visitor,void* visitorUser,xui_text_shape_t* shape,void* providerUser)
{
    (void)providerUser;
    if(!p || p->textShape!=textShape) return XUI_ERROR_UNSUPPORTED;
    xide_dpi_proxy* s=state(p); dpi_font* f=font(value);
    if(!s || !f) return XUI_ERROR_INVALID_ARGUMENT;
    return xuiProxyXgeVisitTextShape(&s->raw,f->base,text,length,flags,visitor,visitorUser,shape);
}
int xideDpiVisitTextShapeCached(xui_proxy p,xui_font value,const char* text,int length,uint32_t flags,
    xui_text_cluster_visit_proc visitor,void* visitorUser,xui_text_shape_t* shape,xui_text_shape_cursor old,
    int common,xui_text_shape_cursor* fresh,xui_text_shape_resume_t* resume,void* providerUser)
{
    (void)providerUser;
    if(!p || p->textShape!=textShape) return XUI_ERROR_UNSUPPORTED;
    xide_dpi_proxy* s=state(p);dpi_font* f=font(value);
    if(!s || !f) return XUI_ERROR_INVALID_ARGUMENT;
    return xuiProxyXgeVisitTextShapeCached(&s->raw,f->base,text,length,flags,visitor,visitorUser,shape,old,common,fresh,resume,NULL);
}

static void fontDestroy(xui_proxy p,xui_font value)
{
    xide_dpi_proxy* s=state(p); dpi_font* f=font(value); if(!s || !f) return;
    dpi_font** at=&s->fonts; while(*at && *at!=f) at=&(*at)->next; if(*at) *at=f->next;
    if(f->raw!=f->base) { s->raw.fontDestroy(&s->raw,f->raw); } s->raw.fontDestroy(&s->raw,f->base); if(f->pending) s->raw.fontDestroy(&s->raw,f->pending); f->magic=0; free(f);
}
static int drawBegin(xui_proxy p,xui_draw_context* out,xui_surface target)
{
    xide_dpi_proxy* s=state(p); dpi_surface* item=surface(target); if(!s || !out || (target && !item)) return XUI_ERROR_INVALID_ARGUMENT; *out=NULL;
    dpi_draw* d=calloc(1,sizeof(*d)); if(!d) return XUI_ERROR_OUT_OF_MEMORY; int result=s->raw.drawBegin(&s->raw,&d->raw,item ? item->raw : NULL);
    if(result!=XUI_OK) { free(d); return result; } d->magic=DRAW_MAGIC; d->target=item; d->scale=item ? item->factor : s->scale; ++s->draws; *out=(xui_draw_context)d; return XUI_OK;
}
static int drawEnd(xui_proxy p,xui_draw_context value)
{ xide_dpi_proxy* s=state(p); dpi_draw* d=draw(value); if(!s || !d) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.drawEnd(&s->raw,d->raw); if(result==XUI_OK) bump((xui_surface)d->target); --s->draws; d->magic=0; free(d); return result; }
static int drawClipGet(xui_proxy p,xui_draw_context value,xui_rect_t* r,int* clipped)
{ dpi_draw* d=draw(value); if(!state(p) || !d || !r || !clipped) return XUI_ERROR_INVALID_ARGUMENT; *r=d->clip; *clipped=d->clipped; return XUI_OK; }
static int drawClipSet(xui_proxy p,xui_draw_context value,xui_rect_t r)
{ xide_dpi_proxy* s=state(p); dpi_draw* d=draw(value); if(!s || !d) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.drawClipSet(&s->raw,d->raw,rect(r,d->scale)); if(result==XUI_OK) { d->clip=r; d->clipped=1; } return result; }
static int drawClipClear(xui_proxy p,xui_draw_context value)
{ xide_dpi_proxy* s=state(p); dpi_draw* d=draw(value); if(!s || !d) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.drawClipClear(&s->raw,d->raw); if(result==XUI_OK) d->clipped=0; return result; }

static int getCaps(xui_proxy pProxy, xui_proxy_caps_t* pCaps)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.getCaps(&s->raw,pCaps); }

static int clipboardSetText(xui_proxy pProxy, const char* sText)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.clipboardSetText(&s->raw,sText); }

static int clipboardGetText(xui_proxy pProxy, char* sText, int iCapacity)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.clipboardGetText(&s->raw,sText,iCapacity); }

static int imeGetEnabled(xui_proxy pProxy)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.imeGetEnabled(&s->raw); }

static int imeSetEnabled(xui_proxy pProxy, int bEnabled)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.imeSetEnabled(&s->raw,bEnabled); }

/* The ordered SDK input bridge already maps logical candidate rectangles. */
static int imeSetCandidateRect(xui_proxy pProxy, xui_rect_t tRect)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.imeSetCandidateRect(&s->raw,tRect); }

static int surfaceLoadFile(xui_proxy pProxy, xui_surface* ppSurface, const char* sPath, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!ppSurface) return XUI_ERROR_INVALID_ARGUMENT; *ppSurface=NULL; xui_surface raw=NULL; xui_surface_desc_t description={0}; int result=s->raw.surfaceLoadFile(&s->raw,&raw,sPath,iFlags); if(result==XUI_OK) result=s->raw.surfaceGetDesc(&s->raw,raw,&description); if(result!=XUI_OK) { if(raw) s->raw.surfaceDestroy(&s->raw,raw); return result; } return ownSurface(s,raw,description,1,ppSurface); }

static int surfaceLoadMemory(xui_proxy pProxy, xui_surface* ppSurface, const void* pData, int iSize, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!ppSurface) return XUI_ERROR_INVALID_ARGUMENT; *ppSurface=NULL; xui_surface raw=NULL; xui_surface_desc_t description={0}; int result=s->raw.surfaceLoadMemory(&s->raw,&raw,pData,iSize,iFlags); if(result==XUI_OK) result=s->raw.surfaceGetDesc(&s->raw,raw,&description); if(result!=XUI_OK) { if(raw) s->raw.surfaceDestroy(&s->raw,raw); return result; } return ownSurface(s,raw,description,1,ppSurface); }

/* SVG dimensions are raster pixels supplied by XUI's DPI icon cache. */
static int surfaceLoadSvgFile(xui_proxy pProxy, xui_surface* ppSurface, const char* sPath, int iWidth, int iHeight, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!ppSurface) return XUI_ERROR_INVALID_ARGUMENT; *ppSurface=NULL; xui_surface raw=NULL; xui_surface_desc_t description={0}; int result=s->raw.surfaceLoadSvgFile(&s->raw,&raw,sPath,iWidth,iHeight,iFlags); if(result==XUI_OK) result=s->raw.surfaceGetDesc(&s->raw,raw,&description); if(result!=XUI_OK) { if(raw) s->raw.surfaceDestroy(&s->raw,raw); return result; } description.iWidth=iWidth; description.iHeight=iHeight; return ownSurface(s,raw,description,1,ppSurface); }

static int surfaceLoadSvgMemory(xui_proxy pProxy, xui_surface* ppSurface, const void* pData, int iSize, int iWidth, int iHeight, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!ppSurface) return XUI_ERROR_INVALID_ARGUMENT; *ppSurface=NULL; xui_surface raw=NULL; xui_surface_desc_t description={0}; int result=s->raw.surfaceLoadSvgMemory(&s->raw,&raw,pData,iSize,iWidth,iHeight,iFlags); if(result==XUI_OK) result=s->raw.surfaceGetDesc(&s->raw,raw,&description); if(result!=XUI_OK) { if(raw) s->raw.surfaceDestroy(&s->raw,raw); return result; } description.iWidth=iWidth; description.iHeight=iHeight; return ownSurface(s,raw,description,1,ppSurface); }

static int surfaceDraw(xui_proxy pProxy, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.surfaceDraw(&s->raw,rawSurface(pSurface),rect(tSrc,surfaceScale(pSurface)),rect(tDst,s->scale),iColor,iFlags); }

static int surfaceClear(xui_proxy pProxy, xui_surface pTarget, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.surfaceClear(&s->raw,rawSurface(pTarget),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int surfaceClearRect(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.surfaceClearRect(&s->raw,rawSurface(pTarget),rect(tRect,surfaceScale(pTarget)),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int surfaceDrawTo(xui_proxy pProxy, xui_surface pTarget, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.surfaceDrawTo(&s->raw,rawSurface(pTarget),rawSurface(pSurface),rect(tSrc,surfaceScale(pSurface)),rect(tDst,surfaceScale(pTarget)),iColor,iFlags); if(result==XUI_OK) bump(pTarget); return result; }

static int surfaceDrawQuad(xui_proxy pProxy, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!pVertices) return XUI_ERROR_INVALID_ARGUMENT; xui_surface_vertex_t transformed[4]; for(int i=0;i<4;++i) { transformed[i]=pVertices[i]; transformed[i].fX*=s->scale; transformed[i].fY*=s->scale; } int result=s->raw.surfaceDrawQuad(&s->raw,rawSurface(pSurface),transformed,iFlags); return result; }

static int surfaceDrawQuadTo(xui_proxy pProxy, xui_surface pTarget, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!pVertices) return XUI_ERROR_INVALID_ARGUMENT; xui_surface_vertex_t transformed[4]; for(int i=0;i<4;++i) { transformed[i]=pVertices[i]; transformed[i].fX*=surfaceScale(pTarget); transformed[i].fY*=surfaceScale(pTarget); } int result=s->raw.surfaceDrawQuadTo(&s->raw,rawSurface(pTarget),rawSurface(pSurface),transformed,iFlags); return result; }

static int surfaceGetSampler(xui_proxy pProxy, xui_surface pSurface, xui_surface_sampler_t* pSampler)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.surfaceGetSampler(&s->raw,rawSurface(pSurface),pSampler); }

static int surfaceSetSampler(xui_proxy pProxy, xui_surface pSurface, const xui_surface_sampler_t* pSampler)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.surfaceSetSampler(&s->raw,rawSurface(pSurface),pSampler); if(result==XUI_OK) bump(pSurface); return result; }

static int shapePoint(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fSize, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapePoint(&s->raw,rawSurface(pTarget),fX*surfaceScale(pTarget),fY*surfaceScale(pTarget),fSize*surfaceScale(pTarget),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int shapeLine(xui_proxy pProxy, xui_surface pTarget, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapeLine(&s->raw,rawSurface(pTarget),fX0*surfaceScale(pTarget),fY0*surfaceScale(pTarget),fX1*surfaceScale(pTarget),fY1*surfaceScale(pTarget),fWidth*surfaceScale(pTarget),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int shapeTriangleFill(xui_proxy pProxy, xui_surface pTarget, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapeTriangleFill(&s->raw,rawSurface(pTarget),vector(tA,surfaceScale(pTarget)),vector(tB,surfaceScale(pTarget)),vector(tC,surfaceScale(pTarget)),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int shapeTriangleStroke(xui_proxy pProxy, xui_surface pTarget, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapeTriangleStroke(&s->raw,rawSurface(pTarget),vector(tA,surfaceScale(pTarget)),vector(tB,surfaceScale(pTarget)),vector(tC,surfaceScale(pTarget)),fWidth*surfaceScale(pTarget),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int shapeRectFill(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapeRectFill(&s->raw,rawSurface(pTarget),rect(tRect,surfaceScale(pTarget)),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int shapeRectStroke(xui_proxy pProxy, xui_surface pTarget, xui_rect_t tRect, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapeRectStroke(&s->raw,rawSurface(pTarget),rect(tRect,surfaceScale(pTarget)),fWidth*surfaceScale(pTarget),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int shapeCircleFill(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fRadius, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapeCircleFill(&s->raw,rawSurface(pTarget),fX*surfaceScale(pTarget),fY*surfaceScale(pTarget),fRadius*surfaceScale(pTarget),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int shapeCircleStroke(xui_proxy pProxy, xui_surface pTarget, float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.shapeCircleStroke(&s->raw,rawSurface(pTarget),fX*surfaceScale(pTarget),fY*surfaceScale(pTarget),fRadius*surfaceScale(pTarget),fWidth*surfaceScale(pTarget),iColor); if(result==XUI_OK) bump(pTarget); return result; }

static int textDraw(xui_proxy pProxy, xui_surface pTarget, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; int result=s->raw.textDraw(&s->raw,rawSurface(pTarget),rawFont(pFont),sText,rect(tRect,surfaceScale(pTarget)),iColor,iFlags); if(result==XUI_OK) bump(pTarget); return result; }

static int drawClearRect(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawClearRect(&s->raw,rawDraw(pDraw),rect(tRect,drawScale(pDraw)),iColor); }

static int drawSurface(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawSurface(&s->raw,rawDraw(pDraw),rawSurface(pSurface),rect(tSrc,surfaceScale(pSurface)),rect(tDst,drawScale(pDraw)),iColor,iFlags); }

static int drawSurfaceQuad(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!pVertices) return XUI_ERROR_INVALID_ARGUMENT; xui_surface_vertex_t transformed[4]; for(int i=0;i<4;++i) { transformed[i]=pVertices[i]; transformed[i].fX*=drawScale(pDraw); transformed[i].fY*=drawScale(pDraw); } int result=s->raw.drawSurfaceQuad(&s->raw,rawDraw(pDraw),rawSurface(pSurface),transformed,iFlags); return result; }

static int drawMeshTriangles(xui_proxy pProxy, xui_draw_context pDraw, const xui_mesh_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!pVertices || iVertexCount<0 || (size_t)iVertexCount>SIZE_MAX/sizeof(xui_mesh_vertex_t)) return XUI_ERROR_INVALID_ARGUMENT; xui_mesh_vertex_t* transformed=malloc((size_t)iVertexCount*sizeof(*transformed)); if(!transformed && iVertexCount) return XUI_ERROR_OUT_OF_MEMORY; for(int i=0;i<iVertexCount;++i) { transformed[i]=pVertices[i]; transformed[i].fX*=drawScale(pDraw); transformed[i].fY*=drawScale(pDraw); } int result=s->raw.drawMeshTriangles(&s->raw,rawDraw(pDraw),transformed,iVertexCount,pIndices,iIndexCount,iFlags); free(transformed); return result; }

static int drawPath(xui_proxy pProxy, xui_draw_context pDraw, const xui_path_command_t* pCommands, int iCommandCount, const xui_path_style_t* pStyle, float fTolerance)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; if(!pCommands || iCommandCount<0 || (size_t)iCommandCount>SIZE_MAX/sizeof(*pCommands)) return XUI_ERROR_INVALID_ARGUMENT; xui_path_command_t* commands=malloc((size_t)iCommandCount*sizeof(*commands)); if(!commands && iCommandCount) return XUI_ERROR_OUT_OF_MEMORY; for(int i=0;i<iCommandCount;++i) { commands[i]=pCommands[i]; for(int j=0;j<3;++j) commands[i].arrPoints[j]=vector(commands[i].arrPoints[j],drawScale(pDraw)); } xui_path_style_t style={0}; float* dashes=NULL; if(pStyle) { style=*pStyle; style.fStrokeWidth*=drawScale(pDraw); style.fDashOffset*=drawScale(pDraw); if(style.iDashCount>0) { if(!style.pDashPattern || (size_t)style.iDashCount>SIZE_MAX/sizeof(float)) { free(commands); return XUI_ERROR_INVALID_ARGUMENT; } dashes=malloc((size_t)style.iDashCount*sizeof(float)); if(!dashes) { free(commands); return XUI_ERROR_OUT_OF_MEMORY; } for(int i=0;i<style.iDashCount;++i) dashes[i]=style.pDashPattern[i]*drawScale(pDraw); style.pDashPattern=dashes; } } int result=s->raw.drawPath(&s->raw,rawDraw(pDraw),commands,iCommandCount,pStyle ? &style : NULL,fTolerance*drawScale(pDraw)); free(commands); free(dashes); return result; }

static int drawSvgPath(xui_proxy pProxy, xui_draw_context pDraw, const char* sPath, xui_rect_t tViewBox, xui_rect_t tTarget, const xui_path_style_t* pStyle, float fTolerance)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT; xui_path_style_t style={0}; float* dashes=NULL; if(pStyle) { style=*pStyle; style.fStrokeWidth*=drawScale(pDraw); style.fDashOffset*=drawScale(pDraw); if(style.iDashCount>0) { if(!style.pDashPattern || (size_t)style.iDashCount>SIZE_MAX/sizeof(float)) { return XUI_ERROR_INVALID_ARGUMENT; } dashes=malloc((size_t)style.iDashCount*sizeof(float)); if(!dashes) { return XUI_ERROR_OUT_OF_MEMORY; } for(int i=0;i<style.iDashCount;++i) dashes[i]=style.pDashPattern[i]*drawScale(pDraw); style.pDashPattern=dashes; } } int result=s->raw.drawSvgPath(&s->raw,rawDraw(pDraw),sPath,tViewBox,rect(tTarget,drawScale(pDraw)),pStyle ? &style : NULL,fTolerance*drawScale(pDraw)); free(dashes); return result; }

static int drawPoint(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fSize, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawPoint(&s->raw,rawDraw(pDraw),fX*drawScale(pDraw),fY*drawScale(pDraw),fSize*drawScale(pDraw),iColor); }

static int drawLine(xui_proxy pProxy, xui_draw_context pDraw, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawLine(&s->raw,rawDraw(pDraw),fX0*drawScale(pDraw),fY0*drawScale(pDraw),fX1*drawScale(pDraw),fY1*drawScale(pDraw),fWidth*drawScale(pDraw),iColor); }

static int drawTriangleFill(xui_proxy pProxy, xui_draw_context pDraw, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawTriangleFill(&s->raw,rawDraw(pDraw),vector(tA,drawScale(pDraw)),vector(tB,drawScale(pDraw)),vector(tC,drawScale(pDraw)),iColor); }

static int drawTriangleStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawTriangleStroke(&s->raw,rawDraw(pDraw),vector(tA,drawScale(pDraw)),vector(tB,drawScale(pDraw)),vector(tC,drawScale(pDraw)),fWidth*drawScale(pDraw),iColor); }

static int drawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawRectFill(&s->raw,rawDraw(pDraw),rect(tRect,drawScale(pDraw)),iColor); }

static int drawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawRectStroke(&s->raw,rawDraw(pDraw),rect(tRect,drawScale(pDraw)),fWidth*drawScale(pDraw),iColor); }

static int drawCircleFill(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fRadius, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawCircleFill(&s->raw,rawDraw(pDraw),fX*drawScale(pDraw),fY*drawScale(pDraw),fRadius*drawScale(pDraw),iColor); }

static int drawCircleStroke(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawCircleStroke(&s->raw,rawDraw(pDraw),fX*drawScale(pDraw),fY*drawScale(pDraw),fRadius*drawScale(pDraw),fWidth*drawScale(pDraw),iColor); }

static int drawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawText(&s->raw,rawDraw(pDraw),rawFont(pFont),sText,rect(tRect,drawScale(pDraw)),iColor,iFlags); }

static int drawTextSpans(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, int iTextSize, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags, const xui_text_paint_span_t* pSpans, int iSpanCount)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.drawTextSpans(&s->raw,rawDraw(pDraw),rawFont(pFont),sText,iTextSize,rect(tRect,drawScale(pDraw)),iColor,iFlags,pSpans,iSpanCount); }

static int clipboardSetItems(xui_proxy pProxy, const xui_clipboard_item_t* pItems, int iItemCount)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.clipboardSetItems(&s->raw,pItems,iItemCount); }

static int clipboardGetData(xui_proxy pProxy, const char* sFormat, void* pData, size_t iCapacity)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.clipboardGetData(&s->raw,sFormat,pData,iCapacity); }

static int zstdDecompress(xui_proxy pProxy, void* pOutput, int iOutputCapacity, const void* pInput, int iInputSize, int* pOutputSize)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return XUI_ERROR_INVALID_ARGUMENT;return s->raw.zstdDecompress(&s->raw,pOutput,iOutputCapacity,pInput,iInputSize,pOutputSize); }

static double clockSeconds(xui_proxy pProxy)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return 0;return s->raw.clockSeconds(&s->raw); }

static void requestFrame(xui_proxy pProxy, float fDelaySeconds)
{ xide_dpi_proxy* s=state(pProxy); if(!s) return;s->raw.requestFrame(&s->raw,fDelaySeconds); }

int xideDpiProxySetScale(xide_dpi_proxy* s,float scale)
{
    if(!s || !isfinite(scale) || scale<0.5f || scale>8 || s->draws) return XUI_ERROR_INVALID_ARGUMENT;
    if(scale==s->scale) return XUI_OK;
    int result=XUI_OK;
    for(dpi_font* f=s->fonts;f && result==XUI_OK;f=f->next) result=s->raw.fontCreateSized(&s->raw,&f->pending,f->base,f->size*scale);
    for(dpi_surface* item=s->surfaces;item && result==XUI_OK;item=item->next) if(item->desc.iFlags&XUI_SURFACE_USAGE_TARGET) {
        xui_surface_desc_t physical={0}; result=dimensions(item->desc,scale,&physical);
        if(result==XUI_OK) result=s->raw.surfaceCreate(&s->raw,&item->pending,&physical);
        if(result==XUI_OK) {
            xui_surface_sampler_t sampler={0}; result=s->raw.surfaceGetSampler(&s->raw,item->raw,&sampler);
            if(result==XUI_OK) result=s->raw.surfaceSetSampler(&s->raw,item->pending,&sampler);
        }
    }
    if(result!=XUI_OK) {
        for(dpi_font* f=s->fonts;f;f=f->next) { if(f->pending) s->raw.fontDestroy(&s->raw,f->pending); f->pending=NULL; }
        for(dpi_surface* item=s->surfaces;item;item=item->next) { if(item->pending) s->raw.surfaceDestroy(&s->raw,item->pending); item->pending=NULL; }
        return result;
    }
    for(dpi_font* f=s->fonts;f;f=f->next) { if(f->raw!=f->base) s->raw.fontDestroy(&s->raw,f->raw); f->raw=f->pending; f->pending=NULL; }
    for(dpi_surface* item=s->surfaces;item;item=item->next) if(item->pending) { s->raw.surfaceDestroy(&s->raw,item->raw); item->raw=item->pending; item->pending=NULL; item->factor=scale; bump((xui_surface)item); }
    s->scale=scale; return XUI_OK;
}
int xideDpiProxyCreate(xide_dpi_proxy** out,xui_proxy_t* proxy,float scale)
{
    if(!out || !proxy || !isfinite(scale) || scale<0.5f || scale>8) { return XUI_ERROR_INVALID_ARGUMENT; } *out=NULL;
    xide_dpi_proxy* s=calloc(1,sizeof(*s)); if(!s) return XUI_ERROR_OUT_OF_MEMORY; s->raw=xuiProxyXge(); s->scale=scale; *proxy=s->raw; proxy->pUser=s;
    if(s->raw.getCaps) proxy->getCaps=getCaps;
    if(s->raw.clipboardSetText) proxy->clipboardSetText=clipboardSetText;
    if(s->raw.clipboardGetText) proxy->clipboardGetText=clipboardGetText;
    if(s->raw.imeGetEnabled) proxy->imeGetEnabled=imeGetEnabled;
    if(s->raw.imeSetEnabled) proxy->imeSetEnabled=imeSetEnabled;
    if(s->raw.imeSetCandidateRect) proxy->imeSetCandidateRect=imeSetCandidateRect;
    if(s->raw.surfaceCreate) proxy->surfaceCreate=surfaceCreate;
    if(s->raw.surfaceCreateRGBA) proxy->surfaceCreateRGBA=surfaceCreateRGBA;
    if(s->raw.surfaceLoadFile) proxy->surfaceLoadFile=surfaceLoadFile;
    if(s->raw.surfaceLoadMemory) proxy->surfaceLoadMemory=surfaceLoadMemory;
    if(s->raw.surfaceLoadSvgFile) proxy->surfaceLoadSvgFile=surfaceLoadSvgFile;
    if(s->raw.surfaceLoadSvgMemory) proxy->surfaceLoadSvgMemory=surfaceLoadSvgMemory;
    if(s->raw.surfaceUpdateRGBA) proxy->surfaceUpdateRGBA=surfaceUpdateRGBA;
    if(s->raw.surfaceReadRGBA) proxy->surfaceReadRGBA=surfaceReadRGBA;
    if(s->raw.surfaceGetDesc) proxy->surfaceGetDesc=surfaceGetDesc;
    if(s->raw.surfaceDraw) proxy->surfaceDraw=surfaceDraw;
    if(s->raw.surfaceClear) proxy->surfaceClear=surfaceClear;
    if(s->raw.surfaceClearRect) proxy->surfaceClearRect=surfaceClearRect;
    if(s->raw.surfaceDrawTo) proxy->surfaceDrawTo=surfaceDrawTo;
    if(s->raw.surfaceDrawQuad) proxy->surfaceDrawQuad=surfaceDrawQuad;
    if(s->raw.surfaceDrawQuadTo) proxy->surfaceDrawQuadTo=surfaceDrawQuadTo;
    if(s->raw.surfaceGetSampler) proxy->surfaceGetSampler=surfaceGetSampler;
    if(s->raw.surfaceSetSampler) proxy->surfaceSetSampler=surfaceSetSampler;
    if(s->raw.surfaceGetGeneration) proxy->surfaceGetGeneration=surfaceGetGeneration;
    if(s->raw.surfaceDestroy) proxy->surfaceDestroy=surfaceDestroy;
    if(s->raw.shapePoint) proxy->shapePoint=shapePoint;
    if(s->raw.shapeLine) proxy->shapeLine=shapeLine;
    if(s->raw.shapeTriangleFill) proxy->shapeTriangleFill=shapeTriangleFill;
    if(s->raw.shapeTriangleStroke) proxy->shapeTriangleStroke=shapeTriangleStroke;
    if(s->raw.shapeRectFill) proxy->shapeRectFill=shapeRectFill;
    if(s->raw.shapeRectStroke) proxy->shapeRectStroke=shapeRectStroke;
    if(s->raw.shapeCircleFill) proxy->shapeCircleFill=shapeCircleFill;
    if(s->raw.shapeCircleStroke) proxy->shapeCircleStroke=shapeCircleStroke;
    if(s->raw.fontLoadFile) proxy->fontLoadFile=fontLoadFile;
    if(s->raw.fontLoadMemory) proxy->fontLoadMemory=fontLoadMemory;
    if(s->raw.fontGetMetrics) proxy->fontGetMetrics=fontGetMetrics;
    if(s->raw.fontDestroy) proxy->fontDestroy=fontDestroy;
    if(s->raw.textMeasure) proxy->textMeasure=textMeasure;
    if(s->raw.textDraw) proxy->textDraw=textDraw;
    if(s->raw.drawBegin) proxy->drawBegin=drawBegin;
    if(s->raw.drawEnd) proxy->drawEnd=drawEnd;
    if(s->raw.drawClearRect) proxy->drawClearRect=drawClearRect;
    if(s->raw.drawSurface) proxy->drawSurface=drawSurface;
    if(s->raw.drawSurfaceQuad) proxy->drawSurfaceQuad=drawSurfaceQuad;
    if(s->raw.drawMeshTriangles) proxy->drawMeshTriangles=drawMeshTriangles;
    if(s->raw.drawPath) proxy->drawPath=drawPath;
    if(s->raw.drawSvgPath) proxy->drawSvgPath=drawSvgPath;
    if(s->raw.drawPoint) proxy->drawPoint=drawPoint;
    if(s->raw.drawLine) proxy->drawLine=drawLine;
    if(s->raw.drawTriangleFill) proxy->drawTriangleFill=drawTriangleFill;
    if(s->raw.drawTriangleStroke) proxy->drawTriangleStroke=drawTriangleStroke;
    if(s->raw.drawRectFill) proxy->drawRectFill=drawRectFill;
    if(s->raw.drawRectStroke) proxy->drawRectStroke=drawRectStroke;
    if(s->raw.drawCircleFill) proxy->drawCircleFill=drawCircleFill;
    if(s->raw.drawCircleStroke) proxy->drawCircleStroke=drawCircleStroke;
    if(s->raw.drawText) proxy->drawText=drawText;
    if(s->raw.drawTextSpans) proxy->drawTextSpans=drawTextSpans;
    if(s->raw.drawClipGet) proxy->drawClipGet=drawClipGet;
    if(s->raw.drawClipSet) proxy->drawClipSet=drawClipSet;
    if(s->raw.drawClipClear) proxy->drawClipClear=drawClipClear;
    if(s->raw.textShape) proxy->textShape=textShape;
    if(s->raw.clipboardSetItems) proxy->clipboardSetItems=clipboardSetItems;
    if(s->raw.clipboardGetData) proxy->clipboardGetData=clipboardGetData;
    if(s->raw.fontCreateSized) proxy->fontCreateSized=fontCreateSized;
    if(s->raw.zstdDecompress) proxy->zstdDecompress=zstdDecompress;
    if(s->raw.clockSeconds) proxy->clockSeconds=clockSeconds;
    if(s->raw.requestFrame) proxy->requestFrame=requestFrame;
    *out=s; return XUI_OK;
}
float xideDpiProxyScale(const xide_dpi_proxy* s) { return s ? s->scale : 1; }
void xideDpiProxyDestroy(xide_dpi_proxy* s)
{
    if(!s) return;
    xui_proxy_t proxy={0}; proxy.pUser=s;
    while(s->fonts) fontDestroy(&proxy,(xui_font)s->fonts);
    while(s->surfaces) surfaceDestroy(&proxy,(xui_surface)s->surfaces);
    free(s);
}
int xideDpiSurfaceReadPixels(xide_dpi_proxy* s,xui_surface value,void* pixels,int stride,int* width,int* height)
{
    dpi_surface* item=surface(value); if(!s || !item || !width || !height) return XUI_ERROR_INVALID_ARGUMENT;
    xui_surface_desc_t description={0}; int result=s->raw.surfaceGetDesc(&s->raw,item->raw,&description); if(result!=XUI_OK) return result;
    *width=description.iWidth; *height=description.iHeight;
    if(!pixels) { return XUI_OK; } if(stride<description.iWidth*4) return XUI_ERROR_INVALID_ARGUMENT;
    return s->raw.surfaceReadRGBA(&s->raw,item->raw,pixels,stride);
}
int xideDpiSurfacePresent(xide_dpi_proxy* s,xui_surface value,int width,int height)
{
    dpi_surface* item=surface(value); if(!s || !item || width<=0 || height<=0) return XUI_ERROR_INVALID_ARGUMENT;
    xui_surface_desc_t description={0}; int result=s->raw.surfaceGetDesc(&s->raw,item->raw,&description); if(result!=XUI_OK) return result;
    return s->raw.surfaceDraw(&s->raw,item->raw,(xui_rect_t){0,0,description.iWidth,description.iHeight},(xui_rect_t){0,0,width,height},XUI_COLOR_WHITE,XUI_SURFACE_DRAW_SCREEN_SPACE);
}

int xideDpiFontPixelMetrics(xide_dpi_proxy* s,xui_font value,xui_font_metrics_t* metrics)
{ dpi_font* f=font(value); return s && f ? s->raw.fontGetMetrics(&s->raw,f->raw,metrics) : XUI_ERROR_INVALID_ARGUMENT; }

#ifndef XIDE_CURSOR_LAYOUT_ENABLED
#define XIDE_CURSOR_LAYOUT_ENABLED 1
#endif
#ifndef XIDE_STREAM_LAYOUT_ENABLED
#define XIDE_STREAM_LAYOUT_ENABLED 1
#endif
int xideDpiCodeEditCreate(xui_context context,xui_widget* output,const xui_code_edit_desc_t* desc)
{
    int result=xuiCodeEditCreate(context,output,desc);
    if(result!=XUI_OK) return result;
#if XIDE_STREAM_LAYOUT_ENABLED
    result=xuiCodeEditSetTextShapeVisitor(*output,xideDpiVisitTextShape,NULL);
#if XIDE_CURSOR_LAYOUT_ENABLED
    if(result==XUI_OK) result=xuiCodeEditSetTextShapeCursorVisitor(*output,xideDpiVisitTextShapeCached,xuiProxyXgeTextShapeCursorFree,NULL);
#endif
    if(result!=XUI_OK) { xuiWidgetDestroy(*output); *output=NULL; }
#endif
    return result;
}
