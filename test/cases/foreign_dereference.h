#ifndef XL6_TEST_FOREIGN_DEREFERENCE_H
#define XL6_TEST_FOREIGN_DEREFERENCE_H
#include <xrt_decl.h>
#include "src/runtime/xl6_foreign_view.h"
typedef struct DerefRoot DerefRoot;
typedef struct DerefWatch {struct DerefWatch* next;struct DerefWatch** previous;void(*notify)(void*);void* user;} DerefWatch;
typedef struct DerefScope {DerefRoot* root;struct DerefScope* self;} DerefScope;
typedef struct DerefOwner {xrtownershipref reference;bool(*Hold)(const void*);bool(*Drop)(const void*);} DerefOwner;
typedef int(*DerefCallback)(DerefRoot*,int*,const char*,void*);
extern const XL6_ForeignViewDescriptorV1 derefRootPolicy;
int deref_create(DerefRoot**);
void deref_destroy(DerefRoot*);
void deref_on(DerefRoot*,DerefCallback,void*,const DerefOwner*);
int deref_fire(DerefRoot*);
void deref_counter_begin(void);
void deref_counter_end(void);
#endif
