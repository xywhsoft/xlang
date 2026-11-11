#ifndef XL6_FOREIGN_SPAN_LIMIT_POSITIONS_H
#define XL6_FOREIGN_SPAN_LIMIT_POSITIONS_H
#include <stdint.h>
#include <stddef.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct LimitRoot LimitRoot;
struct LimitRoot {
    int (*measure)(LimitRoot*,const uint8_t*,int,int);
    int (*render)(LimitRoot*,const uint8_t*,int,int,int,void*,int);
};
extern const XL6_ForeignViewDescriptorV1 limitPolicy;
int limit_create(LimitRoot**);
void limit_destroy(LimitRoot*);
int limit_before(const uint8_t*,int);
int limit_shared(const uint8_t*,int,const uint8_t*,int);
int limit_distinct(const uint8_t*,int,const uint8_t*,int);
int limit_render(const uint8_t*,int,int*,int,int,void*,int);
int limit_calls(void);
void limit_counter_begin(void);
void limit_counter_end(void);
#endif
