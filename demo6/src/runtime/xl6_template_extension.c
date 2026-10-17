#include "xl6_template_internal.h"
typedef struct Extension {uint64_t registry;size_t index;} Extension;
static xatomic64 registrySequence={0};
static bool nextRegistry(uint64_t* result)
{
    uint64_t number=xrtAtomic64Load(&registrySequence,XMEMORY_RELAXED);
    for (;;) {
        if (number==UINT64_MAX) {xrtSetErrorKind(XERR_RANGE);return false;}
        if (xrtAtomic64CompareExchange(&registrySequence,&number,number+1,XMEMORY_RELAXED,XMEMORY_RELAXED)) {
            *result=number+1;return true;
        }
    }
}
static bool extension(xtemplatecall* call)
{
    const Extension* entry=xrtTemplateCallData(call);
    return xl6TemplateInvokeV1(call,entry->registry,entry->index,xrtTemplateCallName(call),NULL);
}
static void* createExtensions(xstrview source,const xvalue* options,const xvalue* definitions,int64_t size)
{
    xtemplateconfig config;uint64_t registryId=0;
    if (!xl6TextSelect(&source,size) || !xl6TemplateCompileConfigV1(options,&config) || !nextRegistry(&registryId)) return NULL;
    xvalue* snapshot=xl6TemplateDataV1(definitions);if (!snapshot) return NULL;
    if (xrtValueType(snapshot)!=XVALUE_ARRAY) {xl6TemplateDropValueV1(snapshot);xrtSetErrorKind(XERR_TYPE);return NULL;}
    size_t count=xrtValueCount(snapshot);
    if (count>SIZE_MAX/sizeof(xtemplateextension)) {xl6TemplateDropValueV1(snapshot);xrtSetErrorKind(XERR_RANGE);return NULL;}
    xtemplateextension* entries=count?xrtCalloc(count,sizeof(*entries)):NULL;
    if (count && !entries) {xl6TemplateDropValueV1(snapshot);return NULL;}
    xtemplateregistry* registry=NULL;xtemplate* compiled=NULL;
    for (size_t i=0;i<count;i++) {
        const xvalue* row=xrtValueArrayGet(snapshot,i);int64_t type,minimum,maximum;
        if (!row || xrtValueType(row)!=XVALUE_ARRAY || xrtValueCount(row)!=4 ||
            !xrtValueGetString(xrtValueArrayGet(row,0),&entries[i].Name) ||
            !xrtValueGetInt(xrtValueArrayGet(row,1),&type) || !xrtValueGetInt(xrtValueArrayGet(row,2),&minimum) ||
            !xrtValueGetInt(xrtValueArrayGet(row,3),&maximum)) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_ARGUMENT);
            goto done;
        }
        if (type<1 || type>4 || minimum<0 || maximum<minimum || (uint64_t)maximum>SIZE_MAX) {
            xrtSetErrorKind(XERR_RANGE);goto done;
        }
        Extension* data=xrtMalloc(sizeof(*data));if (!data) goto done;
        *data=(Extension){registryId,i};entries[i].Type=(xtemplateextensiontype)type;
        entries[i].MinArguments=(size_t)minimum;entries[i].MaxArguments=(size_t)maximum;
        entries[i].Call=extension;entries[i].Data=data;entries[i].Drop=xrtFree;
    }
    registry=xrtTemplateRegistryCreate(entries,count);
    if (!registry) goto done;
    for (size_t i=0;i<count;i++) entries[i].Data=NULL;
    config.Registry=registry;compiled=xrtTemplateCompileConfig(source,&config);
done:;
    xerror* error=xrtTakeError();
    xrtTemplateRegistryRelease(registry);
    for (size_t i=0;i<count;i++) xrtFree(entries[i].Data);
    xrtFree(entries);xl6TemplateDropValueV1(snapshot);xrtClearError();xrtSetErrorTake(error);
    XL6_Template* result=xl6TemplateTakeV1(compiled,registryId,true);
    if (result) result->extensionCount=count;
    return result;
}
void* xl6TemplateCreateExtensionsV1(xstrview source,const xvalue* options,const xvalue* definitions,int64_t size)
{
    void* result=createExtensions(source,options,definitions,size);
    if (!result) xl6TemplateForeignFailureV1();
    return result;
}
static xtemplatecall* call(void* raw)
{
    XL6_TemplateToken* token=xl6TemplateTokenCheckV1(raw);if (!token) return NULL;
    if (!token->call) {xrtSetErrorKind(XERR_STATE);return NULL;}return token->call;
}
int64_t xl6TemplateCallArgumentCountV1(void* raw)
{xtemplatecall* value=call(raw);return value?(int64_t)xrtTemplateCallArgumentCount(value):0;}
xvalue* xl6TemplateCallArgumentV1(void* raw,int64_t index)
{
    xtemplatecall* value=call(raw);xtemplateargview argument;if (!value) return NULL;
    if (index<0) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    if (!xrtTemplateCallArgument(value,(size_t)index,&argument)) return NULL;
    xvalue* result=xrtValueObject();if (!result) return NULL;
    bool ok=xl6TemplateFieldTakeV1(result,"index",xrtValueUInt(argument.Index)) &&
        xl6TemplateFieldTakeV1(result,"name",xrtValueString(argument.Name)) &&
        xl6TemplateFieldTakeV1(result,"source",xrtValueString(argument.Source));
    if (!ok) {xl6TemplateDropValueV1(result);return NULL;}return result;
}
int64_t xl6TemplateCallFindV1(void* raw,xstrview name,int64_t size)
{
    xtemplatecall* value=call(raw);xtemplateargview argument;
    if (!value || !xl6TextSelect(&name,size)) return -1;
    return xrtTemplateCallFind(value,name,&argument)?(int64_t)argument.Index:-1;
}
xvalue* xl6TemplateCallEvalV1(void* raw,int64_t index)
{
    xtemplatecall* value=call(raw);xtemplateargview argument;xtemplatevalue evaluated;
    if (!value) return NULL;
    if (index<0) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    if (!xrtTemplateCallArgument(value,(size_t)index,&argument) || !xrtTemplateCallEval(value,&argument,&evaluated)) return NULL;
    if (evaluated.Value) return xl6TemplateDataV1(evaluated.Value);
    switch (evaluated.Type) {
        case XVALUE_NULL:return xrtValueNull();case XVALUE_BOOL:return xrtValueBool(evaluated.Bool);
        case XVALUE_INT:return xrtValueInt(evaluated.Integer);case XVALUE_UINT:return xrtValueUInt(evaluated.Unsigned);
        case XVALUE_FLOAT:return xrtValueFloat(evaluated.Float);case XVALUE_STRING:return xrtValueString(evaluated.Text);
        case XVALUE_TIME:return xrtValueTime(evaluated.Time);default:xrtSetErrorKind(XERR_TYPE);return NULL;
    }
}
bool xl6TemplateCallWriteV1(void* raw,xstrview text,int64_t size)
{xtemplatecall* value=call(raw);return value && xl6TextSelect(&text,size) && xrtTemplateCallWrite(value,text);}
bool xl6TemplateCallRenderBodyV1(void* raw)
{xtemplatecall* value=call(raw);return value && xrtTemplateCallRender(value);}
bool xl6TemplateCallRenderCurrentV1(void* raw,const xvalue* currentValue)
{
    xtemplatecall* value=call(raw);if (!value) return false;
    xvalue* snapshot=xl6TemplateDataV1(currentValue);if (!snapshot) return false;
    bool ok=xrtTemplateCallRenderCurrent(value,snapshot);xl6TemplateDropValueV1(snapshot);return ok;
}
xstrview xl6TemplateCallRawV1(void* raw)
{xtemplatecall* value=call(raw);return value?xl6TextCopy(xrtTemplateCallRaw(value)):xl6TextFailure;}
xvalue* xl6TemplateCallScopeV1(void* raw,int64_t scope)
{
    xtemplatecall* value=call(raw);if (!value) return NULL;const xvalue* source;
    switch (scope) {case 0:source=xrtTemplateCallCurrent(value);break;case 1:source=xrtTemplateCallRoot(value);break;
        case 2:source=xrtTemplateCallGlobal(value);break;default:xrtSetErrorKind(XERR_RANGE);return NULL;}
    return xl6TemplateDataV1(source);
}
void* xl6TemplateEngineGuardCreateV1(void)
{
    XL6_TextThreadOwner* owner=xrtCalloc(1,sizeof(*owner));if (!owner) return NULL;
    if (!xl6TextThreadInit(owner)) {xrtFree(owner);return NULL;}return owner;
}
void xl6TemplateEngineGuardDestroyV1(void* raw)
{if (raw) {xl6TextThreadDrop(raw);xrtFree(raw);}}
bool xl6TemplateEngineGuardCheckV1(void* raw)
{if (!raw) {xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TextThreadCheck(raw);}
bool xl6TemplateEngineValidateV1(void* raw,xstrview name,int64_t type,int64_t minimum,int64_t maximum)
{
    if (!xl6TemplateEngineGuardCheckV1(raw) || !xl6TextSelect(&name,-1)) return false;
    if (type<1 || type>4 || minimum<0 || maximum<minimum || (uint64_t)maximum>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE);return false;
    }
    xtemplateextension entry={.Name=name,.Type=(xtemplateextensiontype)type,
        .MinArguments=(size_t)minimum,.MaxArguments=(size_t)maximum,.Call=extension};
    xtemplateregistry* registry=xrtTemplateRegistryCreate(&entry,1);
    if (!registry) return false;
    xrtTemplateRegistryRelease(registry);return true;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TemplateEngineGuardHandlePolicyV1Get,xl6TemplateEngineGuardDestroyV1)
