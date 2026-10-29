#include "xl6_template_internal.h"
bool xl6TemplateCompileConfigV1(const xvalue* options,xtemplateconfig* config)
{
    xrtTemplateConfigInit(config);
    if (!xl6TemplateOptionObjectV1(options)) return false;
    const xvalue* open=xl6TemplateOptionV1(options,"open");
    const xvalue* close=xl6TemplateOptionV1(options,"close");
    if ((open && !xrtValueGetString(open,&config->Open)) ||
        (close && !xrtValueGetString(close,&config->Close))) return false;
    if (!xl6TextSelect(&config->Open,-1) || !xl6TextSelect(&config->Close,-1)) return false;
#define LIMIT(Name,Field) if (!xl6TemplateSizeOptionV1(options,Name,&config->Field)) return false
    LIMIT("maxSourceBytes",MaxSourceBytes);LIMIT("maxNodes",MaxNodes);
    LIMIT("maxPathSegments",MaxPathSegments);LIMIT("maxPathDepth",MaxPathDepth);
    LIMIT("maxExpressions",MaxExpressions);LIMIT("maxBlockDepth",MaxBlockDepth);
    LIMIT("maxExpressionDepth",MaxExpressionDepth);LIMIT("maxArguments",MaxArguments);
    LIMIT("maxCallArguments",MaxCallArguments);
#undef LIMIT
    return true;
}
bool xl6TemplateCheckV1(const XL6_Template* self)
{
    if (!self || !self->value) {xrtSetErrorKind(XERR_STATE);return false;}
    return !self->bound || xl6TextThreadCheck(&self->thread);
}
void xl6TemplateDestroyV1(void* raw)
{
    XL6_Template* self=raw;if (!self) return;
    xrtTemplateRelease(self->value);if (self->bound) xl6TextThreadDrop(&self->thread);xrtFree(self);
}
void* xl6TemplateTakeV1(xtemplate* value,uint64_t registry,bool bound)
{
    if (!value) {if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);return NULL;}
    xstrview source=xrtTemplateSource(value);
    if (!xl6TextSelect(&source,-1)) {xrtTemplateRelease(value);return NULL;}
    XL6_Template* self=xrtCalloc(1,sizeof(*self));
    if (!self) {xrtTemplateRelease(value);return NULL;}
    self->value=value;self->registry=registry;self->bound=bound;
    if (bound && !xl6TextThreadInit(&self->thread)) {xl6TemplateDestroyV1(self);return NULL;}
    return self;
}
void* xl6TemplateCreateV1(xstrview source,const xvalue* options,int64_t size)
{
    xtemplateconfig config;
    if (!xl6TextSelect(&source,size) || !xl6TemplateCompileConfigV1(options,&config)) return NULL;
    return xl6TemplateTakeV1(xrtTemplateCompileConfig(source,&config),0,false);
}
xstrview xl6TemplateSourceV1(void* raw)
{XL6_Template* self=raw;return xl6TemplateCheckV1(self)?xl6TextCopy(xrtTemplateSource(self->value)):xl6TextFailure;}
uint64_t xl6TemplateIdentityV1(void* raw)
{XL6_Template* self=raw;return xl6TemplateCheckV1(self)?self->registry:0;}
int64_t xl6TemplateExtensionCountV1(void* raw)
{XL6_Template* self=raw;return xl6TemplateCheckV1(self)?(int64_t)self->extensionCount:0;}
int64_t xl6TemplateNodeCountV1(void* raw)
{
    XL6_Template* self=raw;if (!xl6TemplateCheckV1(self)) return 0;
    size_t count=xrtTemplateNodeCount(self->value);
    if (count>INT64_MAX) {xrtSetErrorKind(XERR_RANGE);return 0;}return (int64_t)count;
}
xvalue* xl6TemplateNodeV1(void* raw,int64_t index)
{
    XL6_Template* self=raw;xtemplatenodeview node;
    if (!xl6TemplateCheckV1(self)) return NULL;
    if (index<0) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    if (!xrtTemplateNode(self->value,(size_t)index,&node)) return NULL;
    xvalue* result=xrtValueObject();if (!result) return NULL;
#define INT(Name,Value) xl6TemplateFieldTakeV1(result,Name,xrtValueUInt(Value))
#define TEXT(Name,Value) xl6TemplateFieldTakeV1(result,Name,xrtValueString(Value))
    bool ok=INT("type",node.Type) && INT("output",node.Output) && INT("offset",node.Location.Offset) &&
        INT("size",node.Location.Size) && INT("line",node.Location.Line) && INT("column",node.Location.Column) &&
        TEXT("source",node.Source) && TEXT("expression",node.Expression) && TEXT("format",node.Format) && TEXT("name",node.Name);
#undef TEXT
#undef INT
    if (!ok) {xl6TemplateDropValueV1(result);return NULL;}return result;
}
int64_t xl6TemplateErrorLocationV1(xerror* error,int64_t field)
{
    xtemplatelocation location;
    if (!error || !xrtTemplateErrorLocation(error,&location)) return -1;
    size_t value;
    switch (field) {case 0:value=location.Offset;break;case 1:value=location.Size;break;
        case 2:value=location.Line;break;case 3:value=location.Column;break;default:return -1;}
    return value<=INT64_MAX?(int64_t)value:-1;
}
/* Registry descriptors own native names/numbers, never callbacks or xvalues. */
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TemplateHandlePolicyV1Get,xl6TemplateDestroyV1)
