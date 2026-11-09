#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* Compile the actual generated IR TU so static native classifiers remain an
 * independent oracle,including all optional outputs and early rejection. */
#include "xl6_ir.c"
#ifdef XLS_NATIVE_IR_CLASSIFIER_PREFIXES
static unsigned checks,loopNonDefault,implicitNonDefault,memberNonDefault;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"classifier-test:%d:%s\n",__LINE__,#x);return 0;}}while(0)
typedef struct loop_outputs {
    XL6_IrErrorAbi errorAbi;
    XL6_IrLoopKeyAbi keyAbi;
    XL6_IrLoopItemAbi itemAbi;
    XL6_IrLoopResourceAbi resourceAbi;
    XL6_IrTypeId resourceType;
    XL6_SymbolId protocolIterTarget;
    XL6_SymbolId protocolNextTarget;
    XL6_IrTypeId protocolIterableReceiverType;
    XL6_IrTypeId protocolIteratorReceiverType;
    XL6_IrTypeId protocolIteratorType;
    XL6_IrTypeId protocolNextResultType;
    XL6_IrTypeId protocolItemType;
    XL6_IrTypeId generatorItemType;
} loop_outputs;
static int compareLoop(const XL6_IrModule* module,const XL6_IrNode* node){for(unsigned mode=0;mode<15;++mode){unsigned mask=mode==0?0:mode==1?8191:1u<<(mode-2);loop_outputs a,b;memset(&a,0xa5,sizeof(a));memset(&b,0xa5,sizeof(b));XL6_IrLoopAbi x=xl6IrClassifyLoopAbi(module,node,
        (mask&1?NULL:&a.errorAbi),
        (mask&2?NULL:&a.keyAbi),
        (mask&4?NULL:&a.itemAbi),
        (mask&8?NULL:&a.resourceAbi),
        (mask&16?NULL:&a.resourceType),
        (mask&32?NULL:&a.protocolIterTarget),
        (mask&64?NULL:&a.protocolNextTarget),
        (mask&128?NULL:&a.protocolIterableReceiverType),
        (mask&256?NULL:&a.protocolIteratorReceiverType),
        (mask&512?NULL:&a.protocolIteratorType),
        (mask&1024?NULL:&a.protocolNextResultType),
        (mask&2048?NULL:&a.protocolItemType),
        (mask&4096?NULL:&a.generatorItemType));XL6_IrLoopAbi y=xlsClassifyOriginal_xl6IrClassifyLoopAbi(module,node,
        (mask&1?NULL:&b.errorAbi),
        (mask&2?NULL:&b.keyAbi),
        (mask&4?NULL:&b.itemAbi),
        (mask&8?NULL:&b.resourceAbi),
        (mask&16?NULL:&b.resourceType),
        (mask&32?NULL:&b.protocolIterTarget),
        (mask&64?NULL:&b.protocolNextTarget),
        (mask&128?NULL:&b.protocolIterableReceiverType),
        (mask&256?NULL:&b.protocolIteratorReceiverType),
        (mask&512?NULL:&b.protocolIteratorType),
        (mask&1024?NULL:&b.protocolNextResultType),
        (mask&2048?NULL:&b.protocolItemType),
        (mask&4096?NULL:&b.generatorItemType));if(!mode && x!=XL6_IR_LOOP_ABI_NONE)++loopNonDefault;CHECK(x==y && !memcmp(&a,&b,sizeof(a)));}return 1;}
static int compareImplicit(const XL6_IrModule* module,XL6_IrNodeId id){for(unsigned mask=0;mask<4;++mask){XL6_IrTypeId a=UINT32_MAX,b=UINT32_MAX;XL6_IrNodeId c=UINT32_MAX,d=UINT32_MAX;XL6_IrImplicitNameAbi x=xl6IrClassifyImplicitNameAbi(module,id,
        (mask&1)?NULL:&a,
        (mask&2)?NULL:&c),y=xlsClassifyOriginal_xl6IrClassifyImplicitNameAbi(module,id,
        (mask&1)?NULL:&b,
        (mask&2)?NULL:&d);if(!mask && x!=XL6_IR_IMPLICIT_NAME_NONE)++implicitNonDefault;CHECK(x==y && a==b && c==d);}return 1;}
#include "src/parser/xl6_parser_api.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
static int sameDiags(const XL6_DiagList* a,const XL6_DiagList* b)
{if(a->count!=b->count)return 0;for(size_t i=0;i<a->count;++i)if(a->items[i].line!=b->items[i].line || a->items[i].column!=b->items[i].column || strcmp(a->items[i].message,b->items[i].message))return 0;return 1;}
static char* dumpModule(const XL6_IrModule* module)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,module);char* text=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return text;}
static int verifyPair(const XL6_IrModule* module,int accepted)
{XL6_DiagList a,b;xl6DiagInit(&a);xl6DiagInit(&b);int x=xl6IrVerifyModule(module,&a),y=xlsFrontendIrReferenceVerify(module,&b);CHECK(x==y && x==accepted && sameDiags(&a,&b));CHECK((!xlsActiveIrScan || !xlsActiveIrScan->originalClassifiers));xl6DiagFree(&a);xl6DiagFree(&b);return 1;}
static int nestedReference(const XL6_IrModule* module)
{
 xls_ir_scan* previous=xlsActiveIrScan;
 xls_ir_scan outer={.previous=previous,.originalClassifiers=1},nested;
 xlsActiveIrScan=&outer;
 xlsIrScanInit(&nested,NULL);
 CHECK(nested.previous==&outer && nested.originalClassifiers==1);
 XL6_DiagList diags;xl6DiagInit(&diags);
 CHECK(xlsFrontendIrReferenceVerify(module,&diags) && !diags.count);
 CHECK(xlsActiveIrScan==&outer && outer.originalClassifiers==1);
 xl6DiagFree(&diags);xlsActiveIrScan=previous;return 1;
}
static int corpus(const char* source)
{
 XL6_ParseResult parsed;xl6ParseResultInit(&parsed);XL6_IrModule current,original;xl6IrModuleInit(&current);xl6IrModuleInit(&original);
 int okay=xl6ParseSource(source,"classifier-prefix.xl",&parsed) && xl6StdlibExpandModule(parsed.root,&parsed.diags) && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"prefix",1) && xl6IrBuildModuleFromAst(&current,parsed.root,&parsed.diags) && xlsIrClassifierReferenceBuild(&original,parsed.root,&parsed.diags);
 if(!okay)xl6DiagPrintText(&parsed.diags,"classifier-prefix.xl");
 CHECK(okay && !parsed.diags.count && (!xlsActiveIrScan || !xlsActiveIrScan->originalClassifiers));char *a=dumpModule(&current),*b=dumpModule(&original);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);CHECK(verifyPair(&current,1));
 CHECK(compareLoop(NULL,NULL) && compareImplicit(NULL,0));
 for(size_t i=0;i<current.nodeCount;++i){
  const XL6_IrNode* node=&current.nodes[i];CHECK(compareLoop(&current,node) && compareLoop(NULL,node) && compareImplicit(&current,(XL6_IrNodeId)(i+1)));
  if(xl6IrClassifyMemberAbi(&current,node)!=XL6_IR_MEMBER_ABI_NONE)++memberNonDefault;
  CHECK(xl6IrClassifyMemberAbi(&current,node)==xlsClassifyOriginal_xl6IrClassifyMemberAbi(&current,node));
  XL6_IrNode saved=current.nodes[i];current.nodes[i].loopAbi=(XL6_IrLoopAbi)INT_MAX;CHECK(verifyPair(&current,0));current.nodes[i]=saved;
  current.nodes[i].implicitNameAbi=(XL6_IrImplicitNameAbi)INT_MAX;CHECK(verifyPair(&current,0));current.nodes[i]=saved;
  current.nodes[i].memberAbi=(XL6_IrMemberAbi)INT_MAX;CHECK(verifyPair(&current,0));current.nodes[i]=saved;
 }
 CHECK(compareLoop(&current,NULL) && compareImplicit(&current,0) && compareImplicit(&current,UINT32_MAX));
 CHECK(xl6IrClassifyMemberAbi(NULL,NULL)==xlsClassifyOriginal_xl6IrClassifyMemberAbi(NULL,NULL));
 CHECK(verifyPair(&current,1) && nestedReference(&current));xl6IrModuleFree(&current);xl6IrModuleFree(&original);xl6ParseResultFree(&parsed);return 1;
}
static int run(void)
{
 const char* sources[]={
  "class ImplicitBox\n string item = \"owned\"\n func value() as string\n return item\n end\nend\n",
  "func leaf(int value) as int\n int result=value+1\n return result\nend\nfunc main(...) as int\n return leaf(7)\nend\n",
  "func values(int count) as generator<string>\n string suffix = \"owned\"\n for i in range(count)\n string item = suffix + string(i)\n yield item\n end\nend\n",
  "class Box\n string item = \"owned\"\n func value() as string\n string local = self.item\n return local\n end\nend\nfunc read(Box item) as string\n return item.value()\nend\n",
  "func guarded() as int\n string outer = \"outer\"\n try\n string inner = \"inner\"\n return 7\n catch e\n string message = e.message\n return 8\n finally\n string final = \"final\"\n end\nend\n"
 };
 for(size_t i=0;i<sizeof(sources)/sizeof(*sources);++i){CHECK(corpus(sources[i]));}
 return 1;
}
static int test(void){if(!compareLoop(NULL,NULL) || !compareImplicit(NULL,0))return 1;if(!run())return 1;if(!loopNonDefault || !implicitNonDefault || !memberNonDefault){fprintf(stderr,"positive admissions missing:%u/%u/%u\n",loopNonDefault,implicitNonDefault,memberNonDefault);return 1;}printf("classifier-admission-test passed: %u checks;all loop output pointers/null masks,implicit outputs,original whole IR dumps,invalid metadata and exact diagnostics.\n",checks);return 0;}

#endif
#endif
int main(void)
{
#if defined(XLS_NATIVE_AST_PACKAGE_FIELDS) && defined(XLS_NATIVE_IR_CLASSIFIER_PREFIXES)
 return test();
#else
 puts("analysis_ir_classifier_test passed: original native cohort retained");return 0;
#endif
}
