#ifndef XL6_TEST_FOREIGN_SCOPED_RETAIN_H
#define XL6_TEST_FOREIGN_SCOPED_RETAIN_H
#include <__xl_runtime/xl6_foreign_view_abi.h>
#include <xrt_decl.h>
typedef struct ScopedRetainNode {int refs,depth,value,mode;} ScopedRetainNode;
typedef struct ScopedRetainScope {ScopedRetainNode* node;uint64_t cookie;} ScopedRetainScope;
typedef struct ScopedRetainWatch {void* raw;} ScopedRetainWatch;
extern const XL6_ForeignViewDescriptorV1 scopedRetainPolicy;
int scoped_retain_create(int,ScopedRetainNode**);
int scoped_retain_hold(ScopedRetainNode*);
int scoped_retain_drop(ScopedRetainNode*);
ScopedRetainNode* scoped_retain_borrow(ScopedRetainNode*);
ScopedRetainNode* scoped_retain_maybe(ScopedRetainNode*,int);
int scoped_retain_single(ScopedRetainNode*,ScopedRetainNode**);
int scoped_retain_pair(ScopedRetainNode*,ScopedRetainNode**,ScopedRetainNode**);
int scoped_retain_value(ScopedRetainNode*);
int scoped_retain_refs(ScopedRetainNode*);
int scoped_retain_live(void);
int scoped_retain_calls(void);
int scoped_retain_producers(void);
void scoped_retain_fail_after(int);
void scoped_retain_reset(void);
#endif
