/* Offline public API tests, with link-time failures of actual compiler stage
 * dependencies. Never include xl_api.c or replace a production source object.
 * These controlled failures prove diagnostics/rollback, not the cause of any
 * historical intermittent failure. No TCC execution or network activity. */
#include "include/xlc.h"
#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { NO_FAULT, LOWER, VERIFY, CGEN_VERIFY, CGEN_EMIT, PRECISE_EMIT };
static int fault;
static unsigned hits;
extern int __real_xl6IrLowerModule(XL6_IrModule*,XL6_DiagList*);
extern int __real_xl6IrVerifyModule(const XL6_IrModule*,XL6_DiagList*);
extern int __real_xl6IrCgenVerifyEmittable(const XL6_IrModule*,XL6_DiagList*);
extern int __real_xl6IrCgenEmitCEx(XL6_Output*,const XL6_IrModule*,XL6_DiagList*,const XL6_IrCgenOptions*);
int __wrap_xl6IrLowerModule(XL6_IrModule* m,XL6_DiagList* d)
{if(fault==LOWER){++hits;return 0;}return __real_xl6IrLowerModule(m,d);}
int __wrap_xl6IrVerifyModule(const XL6_IrModule* m,XL6_DiagList* d)
{if(fault==VERIFY){++hits;return 0;}return __real_xl6IrVerifyModule(m,d);}
int __wrap_xl6IrCgenVerifyEmittable(const XL6_IrModule* m,XL6_DiagList* d)
{if(fault==CGEN_VERIFY){++hits;return 0;}return __real_xl6IrCgenVerifyEmittable(m,d);}
int __wrap_xl6IrCgenEmitCEx(XL6_Output* o,const XL6_IrModule* m,XL6_DiagList* d,const XL6_IrCgenOptions* c)
{
    if(fault==CGEN_EMIT||fault==PRECISE_EMIT){
        ++hits;xl6OutputPuts("PARTIAL_C_MUST_NOT_REPLACE_OLD_OUTPUT",o);
        if(fault==PRECISE_EMIT)xl6DiagAddFile(d,"precise-stage.xl",7,3,"specific stage diagnostic retained");
        return 0;
    }
    return __real_xl6IrCgenEmitCEx(o,m,d,c);
}
static void put_guard(const char* path)
{FILE* f=fopen(path,"wb");assert(f&&fwrite("KEEP",1,4,f)==4&&fclose(f)==0);}
static void check_guard(const char* path)
{char s[16]={0};FILE* f=fopen(path,"rb");assert(f&&fread(s,1,sizeof(s),f)==4&&fclose(f)==0&&!memcmp(s,"KEEP",4));}
static void load(XLC_Context* c)
{assert(xlcAddSourceString(c,"__stage_failure_test.xl","assert 2 + 3 == 5\n")&&xlcCompile(c));}
static const char* stage(int mode)
{return mode==LOWER?"[ir-lower]":mode==VERIFY?"[ir-verify]":mode==CGEN_VERIFY?"[cgen-verify]":"[cgen-emit]";}
int main(int argc,char** argv)
{
    assert(argc==3);unsigned checks=0;
    for(int mode=LOWER;mode<=PRECISE_EMIT;++mode)for(unsigned module=0;module<2;++module){
        XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=argv[1];options.tempDir=argv[2];
        XLC_Context* c=xlcCreateContext(&options);assert(c);fault=NO_FAULT;load(c);
        char path[2400];int size=snprintf(path,sizeof(path),"%s/stage-%d-%u.c",argv[2],mode,module);
        assert(size>0&&(size_t)size<sizeof(path));
        /* Put verification/emission probes strictly AFTER successful lowering.
         * Lowering itself legitimately calls the independent IR verifier. */
        if(mode!=LOWER)assert(xlcEmitC(c,path));
        put_guard(path);unsigned before=hits;fault=mode;
        if(module)assert(!xlcRelocateModule(c));
        else assert(!xlcEmitC(c,path));
        fault=NO_FAULT;assert(hits==before+1);check_guard(path);
        const char* error=xlcGetLastError(c);assert(error&&*error);
        if(mode==PRECISE_EMIT){
            assert(strstr(error,"precise-stage.xl:7:3:")&&strstr(error,"specific stage diagnostic retained")&&
                !strstr(error,"failed without a diagnostic"));
        }else assert(strstr(error,"__stage_failure_test.xl")&&strstr(error,stage(mode))&&
            strstr(error,"failed without a diagnostic"));
        load(c);assert(xlcEmitC(c,path)&&(!xlcGetLastError(c)||!*xlcGetLastError(c)));
        xlcDestroyContext(c);
        ++checks;
    }
    printf("Compiler stage failures: %u exact file/module diagnostics, prior outputs protected, specific errors retained and contexts recover\n",checks);
    return 0;
}
