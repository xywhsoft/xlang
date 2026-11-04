#include "test/cases/foreign_xge3_binding_output_view.h"
#include <assert.h>
#include <string.h>
struct BindingOutputViewBox { unsigned depth;bool closing;unsigned char text[7],bytes[4]; };
static const char text[]={'A',0,(char)0xe4,(char)0xbd,(char)0xa0};
static const unsigned char data[]={0xff,0,0x81,0x42};
static int ends;static xmemdebugsnapshot counter;
int binding_output_view_create(BindingOutputViewBox** out)
{if(!out)return -1;*out=xrtCalloc(1,sizeof(**out));return *out ? 0:-1;}
void binding_output_view_destroy(BindingOutputViewBox* box)
{if(!box)return;box->closing=true;if(!box->depth)xrtFree(box);}
int binding_output_view_pair(xstrview* a,int* b,xbytesview* c,size_t* d)
{*a=(xstrview){text,sizeof(text)};*b=17;*c=(xbytesview){data,sizeof(data)};*d=sizeof(data);return 0;}
int binding_output_view_nullable(int mode,xstrview* a,xbytesview* b)
{
    const char* p=mode==1 ? text:NULL;size_t size=mode==2 ? 1:mode==3 ? SIZE_MAX:0;
    *a=(xstrview){p,size};*b=(xbytesview){(const uint8_t*)p,size};
    if(mode==4 || mode==5){*a=(xstrview){text,sizeof(text)};*b=(xbytesview){NULL,mode==4 ? 1:SIZE_MAX};}
    return 0;
}
int binding_output_view_required(int mode,xstrview* a,xbytesview* b)
{return binding_output_view_nullable(mode,a,b);}
void binding_output_view_void(xstrview* out){*out=(xstrview){text,sizeof(text)};}
int binding_output_view_bytes(xbytesview* out){*out=(xbytesview){data,sizeof(data)};return 0;}
int binding_output_view_span(const uint8_t* p,size_t size,xstrview* a,xbytesview* b)
{*a=(xstrview){(const char*)p,size};*b=(xbytesview){p,size};return 0;}
int binding_output_view_method(BindingOutputViewBox* box,int mode,int failAfter,xstrview* a,xbytesview* b,int* count)
{
    assert(box && box->depth);memcpy(box->text,text,sizeof(text));memcpy(box->bytes,data,sizeof(data));
    *a=(xstrview){(const char*)box->text,sizeof(text)};*b=(xbytesview){box->bytes,sizeof(data)};*count=23;
    if(mode==1){*a=(xstrview){(const char*)1,SIZE_MAX};*b=(xbytesview){(const uint8_t*)1,SIZE_MAX};return -7;}
    if(failAfter>=0)(void)xrtMemDebugFailAfter(failAfter);
    return 0;
}
static bool begin(void* raw,void* storage)
{BindingOutputViewBox* box=raw;BindingOutputViewScope* scope=storage;if(scope->box || box->closing)return false;scope->box=box;++box->depth;return true;}
static bool end(void* storage)
{BindingOutputViewScope* scope=storage;BindingOutputViewBox* box=scope->box;assert(box && box->depth);scope->box=NULL;--box->depth;
 memset(box->text,0xdd,sizeof(box->text));memset(box->bytes,0xdd,sizeof(box->bytes));++ends;
 if(!box->depth && box->closing)xrtFree(box);
 return true;}
static bool destroy(void* raw){binding_output_view_destroy(raw);return true;}
static bool busy(const void* raw){return ((const BindingOutputViewBox*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 bindingOutputViewPolicy={
 sizeof(bindingOutputViewPolicy),XL6_FOREIGN_VIEW_V1_TAG,"binding.output.view",0,1,NULL,NULL,destroy,
 sizeof(BindingOutputViewScope),_Alignof(BindingOutputViewScope),begin,end,NULL,busy,0,NULL,NULL};
int binding_output_view_end_count(void){return ends;}
void binding_output_view_reset(void){(void)xrtMemDebugFailAfter(-1);}
void binding_output_view_counter_begin(void){xrtMemDebugSnapshot(&counter);}
void binding_output_view_counter_end(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.AllocCount-counter.AllocCount==after.FreeCount-counter.FreeCount);}
