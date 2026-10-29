#include "xl6_http_internal.h"
XL6_HttpAuthority* xl6HttpAuthorityCopy(const xhttpauthority* value)
{
    if(value->Text.Size>SIZE_MAX-sizeof(XL6_HttpAuthority)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_HttpAuthority* self=xrtMalloc(sizeof(*self)+value->Text.Size);if(!self)return NULL;
    self->value=*value;if(value->Text.Size)memcpy(self->data,value->Text.Data,value->Text.Size);
    self->value.Text=(xstrview){self->data,value->Text.Size};
    self->value.Host=xl6HttpRebase(value->Host,value->Text.Data,self->data);
    self->value.PortText=xl6HttpRebase(value->PortText,value->Text.Data,self->data);return self;
}
void* xl6HttpAuthorityParseV2(xstrview text,int64_t size)
{xhttpauthority value;if(!xl6TextSelectRaw(&text,size)||!xrtHttpHostParse(text,&value))return NULL;return xl6HttpAuthorityCopy(&value);}
void xl6HttpAuthorityDestroyV2(void* raw){xrtFree(raw);}
xstrview xl6HttpAuthorityTextV2(void* raw){return xl6HttpText(((XL6_HttpAuthority*)raw)->value.Text);}
xstrview xl6HttpAuthorityHostV2(void* raw){return xl6HttpText(((XL6_HttpAuthority*)raw)->value.Host);}
xstrview xl6HttpAuthorityPortTextV2(void* raw){return xl6TextCopy(((XL6_HttpAuthority*)raw)->value.PortText);}
int64_t xl6HttpAuthorityFlagsV2(void* raw){return ((XL6_HttpAuthority*)raw)->value.Flags;}
bool xl6HttpAuthorityHasPortV2(void* raw){return (((XL6_HttpAuthority*)raw)->value.Flags&XHTTP_AUTHORITY_HAS_PORT)!=0;}
bool xl6HttpAuthorityHasPortValueV2(void* raw){return (((XL6_HttpAuthority*)raw)->value.Flags&XHTTP_AUTHORITY_PORT_VALUE)!=0;}
int64_t xl6HttpAuthorityPortV2(void* raw,int64_t fallback)
{uint16 port=0,value;if(!xl6HttpU16(fallback,&value)||!xrtHttpAuthorityPort(&((XL6_HttpAuthority*)raw)->value,value,&port))return 0;return port;}
bool xl6HttpAuthorityHostValidV2(xstrview text,int64_t size){return xl6TextSelectRaw(&text,size)&&xrtHttpHostValid(text);}
bool xl6HttpAuthorityIpv4ValidV2(xstrview text,int64_t size){return xl6TextSelectRaw(&text,size)&&xrtHttpIpv4Valid(text);}
bool xl6HttpAuthorityIpv6ValidV2(xstrview text,int64_t size){return xl6TextSelectRaw(&text,size)&&xrtHttpIpv6Valid(text);}
bool xl6HttpAuthorityHostEqualV2(xstrview left,xstrview right){return xrtHttpHostEqual(left,right);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpAuthorityHandlePolicyV2Get,xl6HttpAuthorityDestroyV2)
