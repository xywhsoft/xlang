#include "xl6_ws_upgrade_internal.h"

static XL6_WsClientOptions* clientOptions(const xwsupgradeclientconfig* value)
{
    if(!xrtWsUpgradeClientConfigValid(value))return NULL;
    size_t size=value->Protocols.Size;
    if(size>SIZE_MAX-sizeof(XL6_WsClientOptions)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_WsClientOptions* result=xrtMalloc(sizeof(*result)+size);if(!result)return NULL;
    result->value=*value;result->value.Protocols=(xstrview){result->data,size};
    if(size)memcpy(result->data,value->Protocols.Data,size);
    return result;
}
void* xl6WsClientOptionsCreateV2(xstrview protocols)
{xwsupgradeclientconfig value;xrtWsUpgradeClientConfigInit(&value);value.Protocols=protocols;return clientOptions(&value);}
void xl6WsClientOptionsDestroyV2(void* raw){xrtFree(raw);}
void* xl6WsClientOptionsWithDeflateV2(void* raw,void* offer,bool required)
{
    xwsupgradeclientconfig value=((XL6_WsClientOptions*)raw)->value;
    value.EnableDeflate=true;value.RequireDeflate=required;value.Deflate=((XL6_WsDeflate*)offer)->value;
    return clientOptions(&value);
}
xstrview xl6WsClientOptionsProtocolsV2(void* raw){return xl6TextCopy(((XL6_WsClientOptions*)raw)->value.Protocols);}
bool xl6WsClientOptionsDeflateEnabledV2(void* raw){return ((XL6_WsClientOptions*)raw)->value.EnableDeflate;}
bool xl6WsClientOptionsRequireDeflateV2(void* raw){return ((XL6_WsClientOptions*)raw)->value.RequireDeflate;}
void* xl6WsClientOptionsOfferV2(void* raw)
{
    const xwsupgradeclientconfig* config=&((XL6_WsClientOptions*)raw)->value;
    if(!config->EnableDeflate)return NULL;
    XL6_WsDeflate* result=xrtMalloc(sizeof(*result));if(result)result->value=config->Deflate;return result;
}
static bool configuredResponse(const xwsdeflate* offer,xwsdeflate* response,ptr data)
{
    const xwsdeflate* value=data;
    if(!xrtWsDeflateResponseCheck(offer,value))return false;
    *response=*value;return true;
}
static XL6_WsServerOptions* serverOptions(xwsupgradeserverconfig value,const xwsdeflate* response)
{
    if(!xrtWsUpgradeServerConfigValid(&value))return NULL;
    size_t size=value.Protocols.Size;
    if(size>SIZE_MAX-sizeof(XL6_WsServerOptions)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_WsServerOptions* result=xrtCalloc(1,sizeof(*result)+size);if(!result)return NULL;
    result->value=value;result->value.Protocols=(xstrview){result->data,size};
    if(size)memcpy(result->data,value.Protocols.Data,size);
    if(response){result->selected=true;result->response=*response;result->value.AcceptDeflate=configuredResponse;result->value.DeflateData=&result->response;}
    return result;
}
void* xl6WsServerOptionsCreateV2(xstrview protocols,bool enabled,bool required)
{xwsupgradeserverconfig value;xrtWsUpgradeServerConfigInit(&value);value.Protocols=protocols;value.EnableDeflate=enabled;value.RequireDeflate=required;return serverOptions(value,NULL);}
void xl6WsServerOptionsDestroyV2(void* raw){xrtFree(raw);}
void* xl6WsServerOptionsWithDeflateResponseV2(void* raw,void* response,bool required)
{
    xwsupgradeserverconfig value=((XL6_WsServerOptions*)raw)->value;
    value.EnableDeflate=true;value.RequireDeflate=required;
    return serverOptions(value,&((XL6_WsDeflate*)response)->value);
}
xstrview xl6WsServerOptionsProtocolsV2(void* raw){return xl6TextCopy(((XL6_WsServerOptions*)raw)->value.Protocols);}
bool xl6WsServerOptionsDeflateEnabledV2(void* raw){return ((XL6_WsServerOptions*)raw)->value.EnableDeflate;}
bool xl6WsServerOptionsRequireDeflateV2(void* raw){return ((XL6_WsServerOptions*)raw)->value.RequireDeflate;}

static XL6_WsUpgrade* snapshot(xstrview host,const char* key,
    const xwsupgradeclientconfig* client,xwsrole role,const xwsupgrade* negotiated)
{
    size_t size=sizeof(XL6_WsUpgrade);
    xstrview protocols=client?client->Protocols:(xstrview){0};
    xstrview selected=negotiated?negotiated->Protocol:(xstrview){0};
    if(!xl6HttpAccumulate(&size,host.Size)||!xl6HttpAccumulate(&size,protocols.Size)||!xl6HttpAccumulate(&size,selected.Size))return NULL;
    XL6_WsUpgrade* result=xrtCalloc(1,size);if(!result)return NULL;
    result->role=role;result->ready=negotiated!=NULL;
    if(client)result->client=*client;
    if(negotiated)result->value=*negotiated;
    if(key)memcpy(result->key,key,XWS_KEY_CAPACITY);
    char* data=result->data;result->host=xl6HttpStoreView(host,&data);
    result->client.Protocols=xl6HttpStoreView(protocols,&data);
    result->value.Protocol=xl6HttpStoreView(selected,&data);
    return result;
}
static void* client(xstrview host,const xwsupgradeclientconfig* config)
{
    if(!xrtWsUpgradeClientConfigValid(config))return NULL;
    char key[XWS_KEY_CAPACITY];if(!xrtWsKeyGenerate(key,sizeof(key)))return NULL;
    char wire[XWS_DEFLATE_MAX_SIZE];size_t size=0,count=0;
    if(config->EnableDeflate&&!xrtWsDeflateOfferWrite(&config->Deflate,wire,sizeof(wire),&size))return NULL;
    xhttpfield fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
    if(!xrtWsUpgradeRequestFields(host,(xstrview){key,XWS_KEY_SIZE},config->Protocols,
        (xstrview){wire,size},fields,XWS_UPGRADE_REQUEST_FIELDS_MAX,&count))return NULL;
    return snapshot(host,key,config,XWS_ROLE_CLIENT,NULL);
}
void* xl6WsUpgradeClientV2(xstrview host,void* options)
{return client(host,&((XL6_WsClientOptions*)options)->value);}
void* xl6WsUpgradeClientDefaultV2(xstrview host)
{xwsupgradeclientconfig value;xrtWsUpgradeClientConfigInit(&value);return client(host,&value);}
void* xl6WsUpgradeServerFromConfigV2(const XL6_HttpHead* request,const xwsupgradeserverconfig* options)
{
    xwsupgrade value;
    if(!xrtWsUpgradeRequestCheck(&request->value,options,&value))return NULL;
    return snapshot((xstrview){0},NULL,NULL,XWS_ROLE_SERVER,&value);
}
void* xl6WsUpgradeServerV2(void* request,void* options)
{return xl6WsUpgradeServerFromConfigV2(request,&((XL6_WsServerOptions*)options)->value);}
void* xl6WsUpgradeServerDefaultV2(void* request)
{xwsupgradeserverconfig value;xrtWsUpgradeServerConfigInit(&value);return xl6WsUpgradeServerFromConfigV2(request,&value);}
void* xl6WsUpgradeCheckResponseV2(void* raw,void* response)
{
    XL6_WsUpgrade* self=raw;XL6_HttpHead* head=response;
    if(self->role!=XWS_ROLE_CLIENT||self->ready){xrtSetErrorKind(XERR_STATE);return NULL;}
    xwsupgrade value;
    if(!xrtWsUpgradeResponseCheck(&head->value,(xstrview){self->key,XWS_KEY_SIZE},&self->client,&value))return NULL;
    return snapshot(self->host,self->key,&self->client,XWS_ROLE_CLIENT,&value);
}
void xl6WsUpgradeDestroyV2(void* raw){xrtFree(raw);}
bool xl6WsUpgradeReadyV2(void* raw){return ((XL6_WsUpgrade*)raw)->ready;}
int64_t xl6WsUpgradeRoleV2(void* raw){return ((XL6_WsUpgrade*)raw)->role;}
xstrview xl6WsUpgradeKeyV2(void* raw)
{XL6_WsUpgrade* self=raw;return xl6TextCopy((xstrview){self->key,self->role==XWS_ROLE_CLIENT?XWS_KEY_SIZE:0});}
xstrview xl6WsUpgradeProtocolV2(void* raw){return xl6TextCopy(((XL6_WsUpgrade*)raw)->value.Protocol);}
bool xl6WsUpgradeDeflateEnabledV2(void* raw)
{XL6_WsUpgrade* self=raw;return self->ready&&self->value.DeflateEnabled;}
xstrview xl6WsUpgradeExtensionsV2(void* raw)
{
    XL6_WsUpgrade* self=raw;
    if(self->role==XWS_ROLE_CLIENT&&!self->ready&&self->client.EnableDeflate){
        char text[XWS_DEFLATE_MAX_SIZE];size_t size=0;
        return xrtWsDeflateOfferWrite(&self->client.Deflate,text,sizeof(text),&size)?xl6TextCopy((xstrview){text,size}):xl6TextFailure;
    }
    return xl6TextCopy((xstrview){self->value.Extensions,self->value.ExtensionSize});
}
void* xl6WsUpgradeHeadersV2(void* raw)
{
    XL6_WsUpgrade* self=raw;xhttpfield fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];size_t count=0;
    if(self->role==XWS_ROLE_CLIENT){
        char text[XWS_DEFLATE_MAX_SIZE];size_t size=0;
        if(self->client.EnableDeflate&&!xrtWsDeflateOfferWrite(&self->client.Deflate,text,sizeof(text),&size))return NULL;
        if(!xrtWsUpgradeRequestFields(self->host,(xstrview){self->key,XWS_KEY_SIZE},self->client.Protocols,(xstrview){text,size},fields,XWS_UPGRADE_REQUEST_FIELDS_MAX,&count))return NULL;
        /* text belongs to this block, so materialize before it expires. */
        return xl6HttpHeadersCopyFields(fields,count);
    }
    if(!xrtWsUpgradeResponseFields((xstrview){self->value.Accept,XWS_ACCEPT_SIZE},self->value.Protocol,
        (xstrview){self->value.Extensions,self->value.ExtensionSize},fields,XWS_UPGRADE_REQUEST_FIELDS_MAX,&count))return NULL;
    return xl6HttpHeadersCopyFields(fields,count);
}
void* xl6WsUpgradeDeflateResponseV2(void* raw)
{
    XL6_WsUpgrade* self=raw;if(!self->ready||!self->value.DeflateEnabled)return NULL;
    XL6_WsDeflate* result=xrtMalloc(sizeof(*result));if(result)result->value=self->value.Deflate;return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsClientOptionsHandlePolicyV2Get,xl6WsClientOptionsDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsServerOptionsHandlePolicyV2Get,xl6WsServerOptionsDestroyV2)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6WsUpgradeHandlePolicyV2Get,xl6WsUpgradeDestroyV2)
