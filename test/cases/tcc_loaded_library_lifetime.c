/* Native loader references belong to the TCCState that keeps code alive. */
#define _GNU_SOURCE 1
#include "libtcc.h"
#include <assert.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>

static int loaded(const char* path)
{
#ifdef _WIN32
    return GetModuleHandleA(path)!=NULL;
#else
    void* handle=dlopen(path,RTLD_NOW|RTLD_NOLOAD);
    if(!handle)return 0;
    dlclose(handle);
    return 1;
#endif
}
static int replacement(void){return 88;}
static TCCState* create(const char* library,const char* tccdir,int registered)
{
    TCCState* state=tcc_new();assert(state);
    tcc_set_lib_path(state,tccdir);
    assert(!tcc_set_output_type(state,TCC_OUTPUT_MEMORY));
    assert(!tcc_add_file(state,library));
    assert(!tcc_add_file(state,library));
    if(registered)assert(!tcc_add_symbol(state,"tcc_lifetime_value",(const void*)replacement));
    assert(!tcc_compile_string(state,"extern int tcc_lifetime_value(void); int value(void){return tcc_lifetime_value();}"));
    assert(!tcc_relocate(state));
    return state;
}
static void check(TCCState* state,int expected)
{
    int(*value)(void)=(int(*)(void))tcc_get_symbol(state,"value");
    assert(value && value()==expected);
}
int main(int argc,char** argv)
{
    assert(argc==3 && !loaded(argv[1]));
    TCCState* first=create(argv[1],argv[2],0);
    assert(loaded(argv[1]));check(first,37);
    TCCState* second=create(argv[1],argv[2],0);
    check(second,37);tcc_delete(first);
    assert(loaded(argv[1]));check(second,37);
    tcc_delete(second);assert(!loaded(argv[1]));
    TCCState* registered=create(argv[1],argv[2],1);
    check(registered,88);tcc_delete(registered);
    assert(!loaded(argv[1]));
    puts("loaded DLL duplicate/cohost/registered-symbol lifetime: PASS");
    return 0;
}
