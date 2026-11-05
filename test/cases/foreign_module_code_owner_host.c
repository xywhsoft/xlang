#include "test/cases/foreign_module_code_owner.h"
#include <assert.h>
#include <string.h>
extern int xl_script_bind_code_owner(const XL6_CodeOwner*);
extern int xl_script_module_init(xvalue*);
extern int xl_script_main_checked(xvalue*,int*);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
typedef struct Owner { unsigned refs,acquires,releases;bool closed; } Owner;
static int acquire(void* raw){Owner* owner=raw;if(owner->closed)return 0;++owner->refs;++owner->acquires;return 1;}
static void release(void* raw){Owner* owner=raw;assert(owner->refs);--owner->refs;++owner->releases;}
int main(int argc,char** argv)
{
    assert(argc==2);bool bound=!strcmp(argv[1],"bound");assert(bound || !strcmp(argv[1],"static"));
    Owner owner={0};XL6_CodeOwner code={sizeof(code),&owner,acquire,release};
    module_owner_expect(bound ? &code:NULL);
    if(bound)assert(xl_script_bind_code_owner(&code));
    int result=-1;
    if(!xl_script_module_init(NULL) || !xl_script_main_checked(NULL,&result)){
        xl_script_print_uncaught();return 90;
    }
    assert(!result && xl_script_module_unit_checked());
    assert(owner.refs==(bound ? 1u:0u));owner.closed=true;
    module_owner_release();assert(!owner.refs && owner.acquires==owner.releases);
    assert(!xl6ForeignViewPendingV1(0) && !xrtGetError());return 0;
}
