#include "foreign_xge3_binding_outputs.h"
int binding_span_outputs(const unsigned char* data,size_t length,size_t* size,unsigned int* first)
{
 *size=length;*first=length ? data[0] : 0;
 return length && data[0]==255 ? -7 : 0;
}
