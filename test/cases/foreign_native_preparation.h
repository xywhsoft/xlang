#ifndef XL6_TEST_PREPARATION_H
#define XL6_TEST_PREPARATION_H
#include <xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct PrepRoot PrepRoot;
typedef struct PrepScope { PrepRoot* root; } PrepScope;
struct PrepRoot { int (*read)(PrepRoot*); unsigned depth; bool closing; };
extern const XL6_ForeignViewDescriptorV1 preparationPolicy;
int preparation_create(PrepRoot**);
void preparation_destroy(PrepRoot*);
int preparation_read(PrepRoot*);
int preparation_pair(PrepRoot*,PrepRoot*);
void preparation_arm(int);
int preparation_calls(void);
int preparation_begins(void);
int preparation_ends(void);
#endif
