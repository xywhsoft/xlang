#ifndef FOREIGN_GLOBAL_CALLBACK_H
#define FOREIGN_GLOBAL_CALLBACK_H
#include <xrt_decl.h>
typedef struct GlobalEvent { int value; } GlobalEvent;
typedef void (*GlobalHandler)(const GlobalEvent*,void*);
typedef struct GlobalOwner {
    xrtownershipref Ref;
    bool (*Hold)(const void*);
    bool (*Drop)(const void*);
} GlobalOwner;
int global_callback_set(GlobalHandler,void*,const GlobalOwner*,GlobalHandler,void*,const GlobalOwner*);
void global_callback_fire(int count,int threaded);
int global_callback_pending(void);
int global_callback_total(void);
int global_callback_live(void);
#endif
