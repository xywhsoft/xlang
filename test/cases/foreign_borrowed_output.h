#ifndef XL6_TEST_FOREIGN_BORROWED_OUTPUT_H
#define XL6_TEST_FOREIGN_BORROWED_OUTPUT_H
#include "test/cases/foreign_managed_view.h"
extern const XL6_ForeignViewDescriptorV1 borrowedChildPolicy;
int borrowed_seed(ManagedRoot*);
int borrowed_status(ManagedRoot*,int,ManagedChild**);
int borrowed_optional(ManagedRoot*,int,ManagedChild**);
int borrowed_get(ManagedRoot*,ManagedChild**);
void borrowed_void(ManagedRoot*,ManagedChild**);
int borrowed_owner(ManagedChild*,ManagedRoot**);
int borrowed_free(int,ManagedRoot*,ManagedChild**);
int borrowed_span(ManagedRoot*,const uint8_t*,int,ManagedChild**,int);
void borrowed_child_destroy(ManagedChild*);
unsigned borrowed_calls(void);
unsigned borrowed_watches(void);
unsigned borrowed_destroys(void);
void borrowed_fail_after(int);
typedef struct BorrowedReader BorrowedReader;
struct BorrowedReader {int (*read)(BorrowedReader*,ManagedRoot*,ManagedChild**);};
BorrowedReader* borrowed_reader(void);
#endif
