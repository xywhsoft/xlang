#include "foreign_terminated_string.h"
#include <assert.h>
#include <string.h>
static int calls;
static int object;
size_t terminated_size(const char* text)
{assert(text);++calls;return strlen(text);}
void* terminated_open(void){return &object;}
size_t terminated_object_size(void* self,const char* text)
{assert(self==&object);return terminated_size(text);}
int terminated_calls(void){return calls;}
void terminated_reset(void){calls=0;}
