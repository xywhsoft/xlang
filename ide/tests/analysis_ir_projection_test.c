#include "analysis/frontend_internal.h"
#include "analysis/check.h"
#include <stdio.h>
#include <string.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
#include "xl6_ir.c"
#endif
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ir_projection_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
#ifdef XLS_NATIVE_IR_RECORD_PROJECTION
static const void* watchedSource;static unsigned watchedFrees,deniedSource;static int failSource;
void __real_xrtFree(void*);char* __real_xrtStrDup(const char*);
void __wrap_xrtFree(void* p){if(p && p==watchedSource)++watchedFrees;__real_xrtFree(p);}
char* __wrap_xrtStrDup(const char* text){if(failSource && text && !strcmp(text,"NativeRecord")){failSource=0;++deniedSource;return NULL;}return __real_xrtStrDup(text);}
static uint64_t blocks(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int different(const XL6_IrTypePool* pool,const xstrbuf* first,int expected)
{
 xstrbuf other={0};CHECK(xlsIrMemoTypes(pool,&other,1024u*1024u));
 CHECK(xlsIrMemoEqual(first,&other)==!expected);xrtStrBufFree(&other);return 1;
}
static int typeKeys(void)
{
 char a[]="NativeRecord",b[]="NativeRecord";
 XL6_IrType type={.kind=XL6_IR_TYPE_C_RECORD,.foreignRecordAbi=XL6_IR_FOREIGN_RECORD_PROJECTED,.foreignRecordSource=a};
 XL6_IrTypePool pool={.items=&type,.count=1,.capacity=1};xstrbuf key={0};uint64_t live=blocks();
 CHECK(xlsIrMemoTypes(&pool,&key,1024u*1024u));
 type.foreignRecordSource=b;CHECK(different(&pool,&key,0));
 type.foreignRecordSource=NULL;CHECK(different(&pool,&key,1));
 type.foreignRecordSource="";CHECK(different(&pool,&key,1));
 type.foreignRecordSource=b;b[0]='n';CHECK(different(&pool,&key,1));b[0]='N';
 for(unsigned abi=0;abi<=255;++abi){type.foreignRecordAbi=(uint8)abi;CHECK(different(&pool,&key,abi!=XL6_IR_FOREIGN_RECORD_PROJECTED));}
 type.foreignRecordAbi=XL6_IR_FOREIGN_RECORD_PROJECTED;
 XL6_IrModule module={.types=pool};xls_ir_derived_context* before=xlsIrDerivedContextBuild(&module,1024u*1024u);CHECK(before);
 type.foreignRecordAbi=XL6_IR_FOREIGN_RECORD_EXACT;xls_ir_derived_context* after=xlsIrDerivedContextBuild(&module,1024u*1024u);CHECK(after && !xlsIrMemoEqual(&before->key,&after->key));xlsIrDerivedContextRelease(after);
 type.foreignRecordAbi=XL6_IR_FOREIGN_RECORD_PROJECTED;type.foreignRecordSource=NULL;after=xlsIrDerivedContextBuild(&module,1024u*1024u);CHECK(after && !xlsIrMemoEqual(&before->key,&after->key));xlsIrDerivedContextRelease(after);xlsIrDerivedContextRelease(before);
 xrtStrBufFree(&key);CHECK(blocks()==live);return 1;
}
static char* dump(const XL6_IrModule* module)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,module);char* text=xl6OutputHasFailed(&out)?NULL:xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return text;}
static const XL6_IrType* record(const XL6_IrModule* m,int projection)
{for(size_t i=0;i<m->types.count;++i){const XL6_IrType* t=m->types.items+i;if(t->kind==XL6_IR_TYPE_C_RECORD && t->foreignRecordAbi==(projection?XL6_IR_FOREIGN_RECORD_PROJECTED:XL6_IR_FOREIGN_RECORD_EXACT))return t;}return NULL;}
static int nativeCase(int projection)
{
 char program[1024];snprintf(program,sizeof(program),"extern \"NativeRecord\" struct Row @header(\"record.h\") %s\n c_int value\nend\nfunc echo(Row value) as Row\n return value\nend\nfunc main(...) as int\n return 0\nend\n",projection?"@c_projection()":"");
 char* path=xrtPathAbs("projection.xl");CHECK(path);xls_semantic_input in={path,program,strlen(program)};char error[512]={0};
 xls_semantic_check* q=xlsSemanticCheckCreate(path,&in,1,1,NULL,NULL,error,sizeof(error));if(q&&!xlsSemanticCheckInfo(q).accepted){char* report=xlsSemanticCheckJson(q,NULL);fprintf(stderr,"%s\n",report?report:error);xlsSemanticStringFree(report);}CHECK(q && xlsSemanticCheckInfo(q).accepted);
 const XL6_IrType* t=record(xlsSemanticCheckIr(q),projection);CHECK(t && (projection ? t->foreignRecordSource && !strcmp(t->foreignRecordSource,"NativeRecord") && strstr(t->cName,"XL_Projection_") : !t->foreignRecordSource && !strcmp(t->cName,"NativeRecord")));
 XL6_IrModule fresh;xl6IrModuleInit(&fresh);XL6_DiagList diags;xl6DiagInit(&diags);CHECK(xlsIrBuildModuleOriginal(&fresh,xlsSemanticCheckAst(q),&diags) && !diags.count);
 char *expected=dump(xlsSemanticCheckIr(q)),*actual=dump(&fresh);CHECK(expected && actual && !strcmp(expected,actual));xrtFree(actual);xl6IrModuleFree(&fresh);
 xls_ir_result* held=xlsFrontendIrResultBuildPersistent(xlsSemanticCheckAst(q),&diags,NULL,32u*1024u*1024u,32u*1024u*1024u,32u*1024u*1024u);CHECK(held && !diags.count && !held->persistent && held->persistentWork.declined);
 actual=dump(&held->module);CHECK(actual && !strcmp(expected,actual));xrtFree(actual);
 if(projection){
  t=record(&held->module,projection);CHECK(t && t->foreignRecordSource && t->foreignRecordSource!=record(xlsSemanticCheckIr(q),projection)->foreignRecordSource);
  xl6IrModuleInit(&fresh);failSource=1;deniedSource=0;CHECK(!xlsIrBuildModuleOriginal(&fresh,xlsSemanticCheckAst(q),&diags) && !failSource && deniedSource==1);xl6IrModuleFree(&fresh);
  watchedSource=t->foreignRecordSource;watchedFrees=0;
 }
 xlsSemanticCheckDestroy(q);CHECK(!projection || !watchedFrees);
 t=record(&held->module,projection);CHECK(t && (projection ? t->foreignRecordSource && !strcmp(t->foreignRecordSource,"NativeRecord") : !t->foreignRecordSource));actual=dump(&held->module);CHECK(actual && !strcmp(expected,actual));xrtFree(actual);xrtFree(expected);xlsFrontendIrResultDestroy(held);CHECK(!projection || watchedFrees==1);watchedSource=NULL;xl6DiagFree(&diags);xrtFree(path);return 1;
}
static int ownedString(void)
{
 uint64_t live=blocks();XL6_IrModule m={0};m.types.items=xrtCalloc(1,sizeof(*m.types.items));CHECK(m.types.items);m.types.count=m.types.capacity=1;
 m.types.items[0].foreignRecordSource=xrtStrDup("NativeRecord");CHECK(m.types.items[0].foreignRecordSource);watchedSource=m.types.items[0].foreignRecordSource;watchedFrees=0;xl6IrModuleFree(&m);CHECK(watchedFrees==1 && blocks()==live);watchedSource=NULL;return 1;
}
#endif
int main(void)
{
#ifdef XLS_NATIVE_IR_RECORD_PROJECTION
 xrtMemStatsEnable(true);if(!xrtMemStatsEnabled())return 1;
 if(!typeKeys() || !ownedString() || !nativeCase(0) || !nativeCase(1))return 1;
 uint64_t live=blocks();for(size_t i=0;i<8;++i)if(!nativeCase((int)(i&1)))return 1;
 if(blocks()!=live){fprintf(stderr,"analysis_ir_projection_test: native owner blocks leaked\n");return 1;}
 printf("analysis_ir_projection_test passed: %u checks; all256 ABI discriminator values, owned backing string content/nullness/address independence, derived context isolation, actual exact/projected native full IR parity, foreign persistent decline and detached owner lifetime\n",checks);
#else
 printf("analysis_ir_projection_test passed: legacy cohort retains its original record ABI (%u checks)\n",checks);
#endif
 return 0;
}
