#ifndef XL6_TEST_CALLBACK_FACTORY_H
#define XL6_TEST_CALLBACK_FACTORY_H
#include <__xl_runtime/xrt_decl.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct CallbackFactoryNode { int refs,mode,value; } CallbackFactoryNode;
typedef struct CallbackFactoryOwner {
 xrtownershipref tReference;
 bool (*onHold)(const void*);
 bool (*onDrop)(const void*);
} CallbackFactoryOwner;
typedef int (*CallbackFactoryVisit)(CallbackFactoryNode*,void*);
extern const XL6_ForeignViewDescriptorV1 callbackFactoryPolicy;
int callback_factory_share(CallbackFactoryNode*,CallbackFactoryNode**);
int callback_factory_drop(CallbackFactoryNode*);
int callback_factory_set(CallbackFactoryVisit,void*,const CallbackFactoryOwner*);
int callback_factory_dispatch(int);
int callback_factory_live(void);
int callback_factory_calls(void);
void callback_factory_fail_after(int);
int callback_factory_reset(void);
int callback_factory_verify(void);
#endif
