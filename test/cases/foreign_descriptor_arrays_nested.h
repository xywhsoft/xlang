#ifndef FOREIGN_DESCRIPTOR_ARRAYS_NESTED_H
#define FOREIGN_DESCRIPTOR_ARRAYS_NESTED_H
#include <stdint.h>
#include <string.h>
typedef struct NestedArrayPort {const char* name;int value;} NestedArrayPort;
typedef struct NestedArrayDesc {const NestedArrayPort* inputs;int inputCount;const NestedArrayPort* outputs;int outputCount;} NestedArrayDesc;
static inline int nestedArrayConsume(const NestedArrayDesc* desc)
{
 int sum=0;
 for(int i=0;i<desc->inputCount;++i)sum+=(int)strlen(desc->inputs[i].name)+desc->inputs[i].value;
 for(int i=0;i<desc->outputCount;++i)sum+=(int)strlen(desc->outputs[i].name)+desc->outputs[i].value;
 return sum;
}
#endif
