#include "test/cases/foreign_callable_field.h"
#include <assert.h>
static int add(BindingFieldTable* table,int value)
{assert(table && table->depth);return value+7;}
static size_t count(BindingFieldTable* table,const uint8_t* bytes,size_t size)
{assert(table && table->depth && (bytes || !size));return size;}
static void pair(BindingFieldTable* table,int* first,float* second)
{assert(table && table->depth);*first=19;*second=3.5f;}
static int checked(BindingFieldTable* table,int status)
{assert(table && table->depth);return status;}
static xstrview echo(BindingFieldTable* table,xstrview text)
{assert(table && table->depth);return text;}
static int apply(BindingFieldTable* table,int value,int(*callback)(int,void*),void* user)
{assert(table && table->depth && callback);return callback(value,user);}
static const xvalue* read(const BindingFieldTable* table)
{assert(table && table->depth);return table->stored;}
static int create(BindingFieldTable* table,BindingFieldTable** output)
{assert(table && table->depth);return binding_field_create(output);}
int binding_field_create(BindingFieldTable** output)
{
 if(!output)return -1;
 *output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;
 **output=(BindingFieldTable){.add=add,.xrtPathCwd=add,.count=count,.pair=pair,.checked=checked,
  .echo=echo,.apply=apply,.read=read,.create=create};
 (*output)->stored=xrtValueUInt(UINT64_MAX);
 if(!(*output)->stored){xrtFree(*output);*output=NULL;return -1;}
 return 0;
}
void binding_field_destroy(BindingFieldTable* table)
{
 if(!table)return;
 table->closing=true;
 if(!table->depth){xrtValueRelease(table->stored);xrtFree(table);}
}
BindingFieldTable* binding_field_null(void){return NULL;}
const xvalue* binding_field_identity(const xvalue* value){return value;}
static bool begin(void* raw,void* token)
{BindingFieldTable* t=raw;BindingFieldScope* s=token;if(t->closing || s->table)return false;s->table=t;++t->depth;return true;}
static bool end(void* token)
{BindingFieldScope* s=token;BindingFieldTable* t=s->table;assert(t && t->depth);s->table=NULL;--t->depth;if(t->closing)binding_field_destroy(t);return true;}
static bool destroy(void* raw){binding_field_destroy(raw);return true;}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xrtValueOwnership(((const BindingFieldTable*)raw)->stored),user);}
static bool busy(const void* raw){return ((const BindingFieldTable*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 bindingFieldPolicy={sizeof(bindingFieldPolicy),XL6_FOREIGN_VIEW_V1_TAG,
 "binding.callable.field",0,1,NULL,NULL,destroy,sizeof(BindingFieldScope),_Alignof(BindingFieldScope),begin,end,trace,busy,0,NULL,NULL};
