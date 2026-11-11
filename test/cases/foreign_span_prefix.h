#ifndef XL6_FOREIGN_SPAN_PREFIX_H
#define XL6_FOREIGN_SPAN_PREFIX_H
#include <stddef.h>
#include <stdint.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct PrefixRoot PrefixRoot;
struct PrefixRoot {int (*measure)(PrefixRoot*,const void*,size_t);};
extern const XL6_ForeignViewDescriptorV1 prefixRootPolicy;
int prefix_create(PrefixRoot**);
void prefix_destroy(PrefixRoot*);
int prefix_sum(const void*,size_t);
int prefix_sum_int(const uint8_t*,int);
int prefix_mix(const char*,int,int*,size_t*);
int prefix_two(const void*,size_t,const void*,size_t);
int prefix_calls(void);
void prefix_counter_begin(void);
void prefix_counter_end(void);
#endif
