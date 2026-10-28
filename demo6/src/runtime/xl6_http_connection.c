#include "xl6_http_internal.h"
typedef struct XL6_HttpConnection {XL6_HttpTokens* options;xhttpconnectionstatus status;} XL6_HttpConnection;
void* xl6HttpConnectionFromHeadersV2(void* raw,int64_t version,int64_t flags)
{
    if((version!=10&&version!=11)||flags<0||flags>7){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return NULL;
    xhttpconnectionstatus status=xrtHttpConnectionPersistence((xhttpversion)version,f,n,(uint32)flags);
    XL6_HttpTokens* options=status==XHTTP_CONNECTION_ERROR?NULL:xl6HttpTokensCopyFields(f,n,(xstrview){"Connection",10});
    xl6HttpLeave(&g);if(!options)return NULL;
    XL6_HttpConnection* self=xrtMalloc(sizeof(*self));if(!self){xrtFree(options);return NULL;}
    *self=(XL6_HttpConnection){options,status};return self;
}
void xl6HttpConnectionDestroyV2(void* raw)
{XL6_HttpConnection* self=raw;if(self){xrtFree(self->options);xrtFree(self);}}
int64_t xl6HttpConnectionLengthV2(void* raw){return (int64_t)((XL6_HttpConnection*)raw)->options->count;}
int64_t xl6HttpConnectionStatusV2(void* raw){return ((XL6_HttpConnection*)raw)->status;}
xstrview xl6HttpConnectionOptionAtV2(void* raw,int64_t index)
{return xl6HttpTokensAtV2(((XL6_HttpConnection*)raw)->options,index);}
bool xl6HttpConnectionContainsV2(void* raw,xstrview option)
{return xl6HttpTokensContainsV2(((XL6_HttpConnection*)raw)->options,option);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpConnectionHandlePolicyV2Get,xl6HttpConnectionDestroyV2)
int64_t xl6HttpHeadersConnectionStatusV2(void* raw,int64_t version,int64_t flags)
{
    if((version!=10&&version!=11)||flags<0||flags>7){xrtSetErrorKind(XERR_RANGE);return 0;}
    XL6_HttpGuard g={0};const xhttpfield* f;size_t n;if(!xl6HttpHeadersBorrow(raw,&g,&f,&n))return 0;
    xhttpconnectionstatus result=xrtHttpConnectionPersistence((xhttpversion)version,f,n,(uint32)flags);
    xl6HttpLeave(&g);return result==XHTTP_CONNECTION_ERROR?0:(int64_t)result;
}
