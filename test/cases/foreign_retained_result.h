#ifndef XL6_TEST_FOREIGN_RETAINED_RESULT_H
#define XL6_TEST_FOREIGN_RETAINED_RESULT_H
#include "src/runtime/xl6_foreign_view.h"
typedef struct RetainedNode {int refs,mode,value;struct RetainedNode* child;} RetainedNode;
extern const XL6_ForeignViewDescriptorV1 retainedPolicy;
int retained_create(int,RetainedNode**);
int retained_addref(RetainedNode*);
int retained_addref_zero(RetainedNode*);
void retained_release(RetainedNode*);
RetainedNode* retained_borrow(RetainedNode*);
RetainedNode* retained_borrow_zero(RetainedNode*);
RetainedNode* retained_maybe(RetainedNode*);
int retained_set_child(RetainedNode*,RetainedNode*);
int retained_value(RetainedNode*);
int retained_live(void);
int retained_refs(RetainedNode*);
int retained_calls(void);
void retained_fail_after(int);
void retained_reset(void);
#endif
