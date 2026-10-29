#include "xl6_http_internal.h"
void* xl6HttpTargetParseV2(xstrview method,xstrview text,int64_t size)
{
    xhttptarget value;if(!xl6TextSelectRaw(&text,size)||!xrtHttpTargetParse(method,text,&value))return NULL;
    if(method.Size>SIZE_MAX-sizeof(XL6_HttpTarget)||text.Size>SIZE_MAX-sizeof(XL6_HttpTarget)-method.Size){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_HttpTarget* self=xrtMalloc(sizeof(*self)+method.Size+text.Size);if(!self)return NULL;
    self->value=value;if(method.Size)memcpy(self->data,method.Data,method.Size);
    if(text.Size)memcpy(self->data+method.Size,text.Data,text.Size);
    const char* target=self->data+method.Size;
    self->value.Method=(xstrview){self->data,method.Size};
    self->value.Text=(xstrview){target,text.Size};
    self->value.Scheme=xl6HttpRebase(value.Scheme,text.Data,target);
    self->value.Authority=xl6HttpRebase(value.Authority,text.Data,target);
    self->value.Path=xl6HttpRebase(value.Path,text.Data,target);
    self->value.Query=xl6HttpRebase(value.Query,text.Data,target);
    self->value.Host.Text=xl6HttpRebase(value.Host.Text,text.Data,target);
    self->value.Host.Host=xl6HttpRebase(value.Host.Host,text.Data,target);
    self->value.Host.PortText=xl6HttpRebase(value.Host.PortText,text.Data,target);return self;
}
void xl6HttpTargetDestroyV2(void* raw){xrtFree(raw);}
int64_t xl6HttpTargetFormV2(void* raw){return ((XL6_HttpTarget*)raw)->value.Form;}
int64_t xl6HttpTargetFlagsV2(void* raw){return ((XL6_HttpTarget*)raw)->value.Flags;}
#define GET(Name,Field) xstrview xl6HttpTarget##Name##V2(void* raw){return xl6HttpText(((XL6_HttpTarget*)raw)->value.Field);}
GET(Method,Method) GET(Text,Text) GET(Scheme,Scheme) GET(AuthorityText,Authority) GET(Path,Path) GET(Query,Query)
#undef GET
void* xl6HttpTargetAuthorityV2(void* raw,xstrview host)
{xhttpauthority authority;if(!xrtHttpTargetAuthority(&((XL6_HttpTarget*)raw)->value,host,&authority))return NULL;return xl6HttpAuthorityCopy(&authority);}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6HttpTargetHandlePolicyV2Get,xl6HttpTargetDestroyV2)
