#ifndef XLS_TEXT_SPAN_H
#define XLS_TEXT_SPAN_H
#include <stddef.h>
/* Shared byte-diff core for already validated immutable UTF8. Keep scalar and
 * CRLF boundaries, including equal leading/trailing bytes inside a scalar.
 * The internal caller supplies valid data/output; this is not a validator.
 * A cancelled scan leaves output untouched. No allocation or text ownership. */
typedef struct xls_text_span {size_t start,beforeEnd,afterEnd;} xls_text_span;
static inline int xlsTextByteBoundary(const char* bytes,size_t size,size_t offset)
{
 return offset==size || (!((unsigned char)bytes[offset]>=0x80 && (unsigned char)bytes[offset]<0xc0) &&
     (!offset || bytes[offset-1]!='\r' || bytes[offset]!='\n'));
}
static inline int xlsTextSpanDiff(const char* left,size_t before,const char* right,size_t after,
 int (*cancelled)(void*),void* user,xls_text_span* output)
{
 size_t prefix=0,suffix=0;
 while(prefix<before && prefix<after && left[prefix]==right[prefix]){
  if(!(prefix%65536) && cancelled && cancelled(user))return -1;
  ++prefix;
 }
 if(prefix==before && prefix==after){*output=(xls_text_span){before,before,after};return 0;}
 while(prefix && (!xlsTextByteBoundary(left,before,prefix) || !xlsTextByteBoundary(right,after,prefix)))--prefix;
 while(suffix<before-prefix && suffix<after-prefix && left[before-suffix-1]==right[after-suffix-1]){
  if(!(suffix%65536) && cancelled && cancelled(user))return -1;
  ++suffix;
 }
 while(suffix && (!xlsTextByteBoundary(left,before,before-suffix) || !xlsTextByteBoundary(right,after,after-suffix)))--suffix;
 *output=(xls_text_span){prefix,before-suffix,after-suffix};return 1;
}
#endif
