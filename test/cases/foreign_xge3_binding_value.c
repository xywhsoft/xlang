#include "test/cases/foreign_xge3_binding_value.h"
#include <assert.h>
struct BindingValueBox { xvalue* value; unsigned depth; bool clearOnEnd,closing; size_t beforeRead; };
static unsigned checkedCopies;
static size_t refs(const xvalue* value)
{xrtownershipref ref=xrtValueOwnership(value);size_t count=0;assert(ref.Data && ref.Ops && ref.Ops->Count(ref.Data,&count));return count;}
int binding_value_create(BindingValueBox** out)
{if(!out)return -1;*out=xrtCalloc(1,sizeof(**out));return *out ? 0:-1;}
void binding_value_destroy(BindingValueBox* box)
{if(!box)return;box->closing=true;if(!box->depth){xrtValueRelease(box->value);xrtFree(box);}}
void binding_value_set(BindingValueBox* box,const xvalue* value)
{xvalue* held=xrtValueRetain(value);assert(held);xrtValueRelease(box->value);box->value=held;}
const xvalue* binding_value_read(BindingValueBox* box)
{assert(box->depth);box->clearOnEnd=true;box->beforeRead=box->value ? refs(box->value):0;return box->value;}
const xvalue* binding_value_null(BindingValueBox* box){(void)box;return NULL;}
const xvalue* binding_value_identity(const xvalue* value){return value;}
int binding_value_empty(BindingValueBox* box){return box->value==NULL;}
int binding_value_checks(void){return (int)checkedCopies;}
static bool begin(void* raw,void* storage)
{BindingValueBox* box=raw;BindingValueScope* scope=storage;if(scope->box || box->closing)return false;scope->box=box;++box->depth;return true;}
static bool end(void* storage)
{BindingValueScope* scope=storage;BindingValueBox* box=scope->box;assert(box && box->depth);scope->box=NULL;--box->depth;if(!box->depth && box->clearOnEnd){if(box->value){assert(refs(box->value)>box->beforeRead);++checkedCopies;}xrtValueRelease(box->value);box->value=NULL;box->clearOnEnd=false;}if(!box->depth && box->closing)binding_value_destroy(box);return true;}
static bool destroy(void* raw){binding_value_destroy(raw);return true;}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{const BindingValueBox* box=raw;return !box->value || visit(xrtValueOwnership(box->value),user);}
static bool busy(const void* raw){return ((const BindingValueBox*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 bindingValueBoxPolicy={
 sizeof(bindingValueBoxPolicy),XL6_FOREIGN_VIEW_V1_TAG,"binding.value.box",0,1,NULL,NULL,destroy,
 sizeof(BindingValueScope),_Alignof(BindingValueScope),begin,end,trace,busy};
