#ifndef XL6_TEMPLATE_INTERNAL_H
#define XL6_TEMPLATE_INTERNAL_H
#include "xl6_text_thread.h"
#include "xl6_closure.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
/* Registry data contains only copied names, numeric dispatch identities and
 * resident functions. Language callbacks remain ordinary managed fields. */
typedef struct XL6_Template {
    xtemplate* value;
    uint64_t registry;
    size_t extensionCount;
    XL6_TextThreadOwner thread;
    bool bound;
} XL6_Template;
typedef struct XL6_TemplateToken {
    xatomic64 references;
    XL6_TextThreadOwner thread;
    bool active, include;
    uint64_t registry;
    size_t index;
    xstrview name;
    xtemplatecall* call;
    xtemplate* resolved;
} XL6_TemplateToken;
bool xl6TemplateCompileConfigV1(const xvalue*,xtemplateconfig*);
void* xl6TemplateTakeV1(xtemplate*,uint64_t,bool);
bool xl6TemplateCheckV1(const XL6_Template*);
bool xl6TemplateInvokeV1(xtemplatecall*,uint64_t,size_t,xstrview,xtemplate**);
XL6_TemplateToken* xl6TemplateTokenCheckV1(void*);
typedef bool (*XL6_TemplateWriter)(ptr,xstrview);
/* Diagnostic-only (FOREIGN_SCOPE) entries have no unambiguous failed scalar
 * carrier. Preserve a separate failure bit even when OOM cannot allocate an
 * error object; ordinary NULL/false checked entries must keep their carrier. */
static inline void xl6TemplateForeignFailureV1(void)
{xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});}
bool xl6TemplateWriteOutputV1(void*,const xvalue*,const xvalue*,XL6_Closure*,XL6_TemplateWriter,ptr);
xvalue* xl6TemplateDataV1(const xvalue*);
/* Scoped cleanup preserves the operation failure, including a diagnostic-less
 * language failure, before dropping any independently owned graph. */
static inline void xl6TemplateDropValueV1(xvalue* value)
{
    if (!value) return;
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBeginCleanup(&scope,false)) abort();
    xrtValueRelease(value);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if (!result.ok) xl6ErrorScopePropagateFailure(result);
}
static inline bool xl6TemplateOptionObjectV1(const xvalue* options)
{
    if (!options || xrtValueType(options)==XVALUE_NULL || xrtValueType(options)==XVALUE_OBJECT) return true;
    xrtSetErrorKind(XERR_TYPE);return false;
}
static inline const xvalue* xl6TemplateOptionV1(const xvalue* options,const char* name)
{
    return options && xrtValueType(options)!=XVALUE_NULL ?
        xrtValueObjectGet(options,(xstrview){name,strlen(name)}):NULL;
}
static inline bool xl6TemplateSizeOptionV1(const xvalue* options,const char* name,size_t* output)
{
    const xvalue* value=xl6TemplateOptionV1(options,name);if (!value) return !xrtGetError();
    int64_t number;
    if (!xrtValueGetInt(value,&number)) return false;
    if (number<0 || (uint64_t)number>SIZE_MAX) {xrtSetErrorKind(XERR_RANGE);return false;}
    *output=(size_t)number;return true;
}
static inline bool xl6TemplateBoolOptionV1(const xvalue* options,const char* name,uint32_t bit,uint32_t* flags)
{
    const xvalue* value=xl6TemplateOptionV1(options,name);if (!value) return !xrtGetError();
    bool enabled;if (!xrtValueGetBool(value,&enabled)) return false;
    if (enabled) *flags|=bit;else *flags&=~bit;return true;
}
static inline bool xl6TemplateFieldTakeV1(xvalue* object,const char* name,xvalue* value)
{
    bool ok=value && xrtValueObjectSetTake(object,(xstrview){name,strlen(name)},&value);
    xl6TemplateDropValueV1(value);return ok;
}
#endif
