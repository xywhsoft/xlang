#ifndef XL6_TEST_FOREIGN_MANAGED_VIEW_H
#define XL6_TEST_FOREIGN_MANAGED_VIEW_H
#include "src/runtime/xl6_foreign_view.h"
typedef struct ManagedRoot ManagedRoot;
typedef struct ManagedChild ManagedChild;
typedef struct ManagedWatch {
    struct ManagedWatch* next;
    struct ManagedWatch** previous;
    void (*notify)(void*);
    void* user;
} ManagedWatch;
struct ManagedChild {ManagedWatch* watches;ManagedRoot* root;ManagedChild* next;bool closing;int value;};

typedef struct ManagedScope {ManagedRoot* root;struct ManagedScope* self;} ManagedScope;
extern const XL6_ForeignViewDescriptorV1 managedRootPolicy;
extern const XL6_ForeignViewDescriptorV1 managedChildPolicy;
int managed_root_create(int,ManagedRoot**);
void managed_root_destroy(ManagedRoot*);
int managed_root_read(ManagedRoot*);
int managed_from_bytes(const uint8_t*,size_t,ManagedRoot**);
int managed_from_root(ManagedRoot*,ManagedRoot**);
int managed_root_method_read(ManagedRoot*);
int managed_roots_live(void);
int managed_children_live(void);
int managed_child_create(ManagedRoot*,int,ManagedChild**);
void managed_child_destroy(ManagedChild*);
ManagedChild* managed_child_first(ManagedRoot*);
ManagedRoot* managed_child_owner(ManagedChild*);
int managed_child_read(ManagedChild*);
void managed_reset_failure(void);
void* managed_reader_open(void);
int managed_reader_value(void*,ManagedRoot*);
ManagedChild* managed_reader_child(void*,ManagedRoot*);
#include <xrt_decl.h>
typedef struct ManagedCallbackOwner { xrtownershipref reference; bool (*Hold)(const void*); bool (*Drop)(const void*); } ManagedCallbackOwner;
typedef int (*ManagedCallback)(ManagedChild*,int,void*);
void managed_set_callback(ManagedRoot*,ManagedCallback,void*,const ManagedCallbackOwner*);
int managed_fire(ManagedRoot*,int);
void managed_reader_set(void*,ManagedRoot*,ManagedCallback,void*,const ManagedCallbackOwner*);
int managed_reader_span(void*,ManagedRoot*,const uint8_t*,size_t);
int managed_span_read(ManagedRoot*,const unsigned char*,size_t);
int managed_span_write(ManagedRoot*,unsigned char*,int);
void managed_counter_begin(void);
void managed_counter_end(void);
int managed_span_write_callback(ManagedRoot*,uint8_t*,int);
int managed_span_floats(ManagedRoot*,const float*,int);
#endif
