#ifndef FOREIGN_TERMINATED_STRING_H
#define FOREIGN_TERMINATED_STRING_H
#include <stddef.h>
size_t terminated_size(const char*);
void* terminated_open(void);
size_t terminated_object_size(void*,const char*);
int terminated_calls(void);
void terminated_reset(void);
#endif
