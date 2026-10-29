#include "xl6_http_internal.h"
bool xl6HttpTrailersNameValidV2(xstrview name){return xrtHttpTrailerNameValid(name);}
bool xl6HttpTrailersSectionValidV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return false;
 bool valid=xrtHttpTrailerSectionValid(f,n);xl6HttpLeave(&g);return valid;}
int64_t xl6HttpTrailersDeclaredCountV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n,count=0;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
 bool ok=xrtHttpTrailerCount(f,n,&count);xl6HttpLeave(&g);return ok?(int64_t)count:0;}
bool xl6HttpTrailersDeclaredV2(void* raw,xstrview name)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return false;
 xhttpnext found=xrtHttpTrailerFind(f,n,name);xl6HttpLeave(&g);return found==XHTTP_NEXT_ITEM;}
xstrview xl6HttpTrailersNamesV2(void* raw)
{XL6_HttpGuard g={0};const xhttpfield* f;size_t n,size=0;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return xl6TextFailure;
 char* data=xrtHttpTrailerNamesBuild(f,n,&size);xl6HttpLeave(&g);return xl6TextOwn(data,size);}
int64_t xl6HttpTrailersWriteNamesIntoV2(void* raw,void* target)
{
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n,size=0;xbuffer* out=xl6BufferChecked(target);
    if(!out||!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
    bool ok=xrtHttpTrailerNamesWrite(f,n,NULL,0,&size);
    if(ok&&size>(size_t)INT64_MAX-out->Size){xrtSetErrorKind(XERR_RANGE);ok=false;}
    if(ok)ok=xl6BufferAllocation(xrtBufferReserve(out,out->Size+size));
    if(ok)ok=xrtHttpTrailerNamesWrite(f,n,out->Data?out->Data+out->Size:NULL,size,&size);
    if(ok)out->Size+=size;
    xl6HttpLeave(&g);return ok?(int64_t)size:0;
}
