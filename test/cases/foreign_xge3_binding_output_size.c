#include "test/cases/foreign_xge3_binding_output_size.h"
#include <assert.h>
int binding_sized_free(BindingSizedInfo* output,int value)
{if(!output || output->size!=sizeof(*output))return -7;output->value=value;return 0;}
void binding_sized_void(BindingSizedInfo* output)
{assert(output && output->size==sizeof(*output));output->value=9;}
int binding_sized_method(ManagedRoot* root,BindingSizedInfo* output,int value)
{assert(root);return binding_sized_free(output,value);}
int binding_sized_multi(unsigned* count,BindingSizedInfo* first,const uint8_t* data,size_t length,BindingSizedInfo* last)
{
    if(!count || !first || !last || !data || !length || first->size!=sizeof(*first) || last->size!=sizeof(*last))return -7;
    *count=(unsigned)length;first->value=data[0];last->value=data[length-1];return 0;
}
