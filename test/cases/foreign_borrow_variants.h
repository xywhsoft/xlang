#ifndef XL6_TEST_FOREIGN_BORROW_VARIANTS_H
#define XL6_TEST_FOREIGN_BORROW_VARIANTS_H
#include <xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct BorrowRoot { unsigned depth;bool closing; } BorrowRoot;
typedef struct BorrowScope { BorrowRoot* root; } BorrowScope;
extern const XL6_ForeignViewDescriptorV1 borrowPolicy;
int variant_create(BorrowRoot**);
void variant_destroy(BorrowRoot*);
int variant_take(BorrowRoot*,BorrowRoot*);
int variant_with(BorrowRoot*,int(*)(BorrowRoot*,void*),void*);
int variant_calls(void);
#endif
