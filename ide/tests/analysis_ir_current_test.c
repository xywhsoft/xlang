#include "analysis/frontend_internal.h"
#include "analysis/frontend_ir_memo.h"
#include "src/sem/xl6_sem.h"
#include "analysis_ast_same.h"
#include "analysis/ast_clone.h"
#include <stdio.h>
#include <string.h>
static unsigned irChecks;
int xlsTestCurrentIrCodec(unsigned*);
int xlsReferenceIrBuildModuleFromAst(XL6_IrModule*,const XL6_AstNode*,XL6_DiagList*);
int xlsReferenceIrVerifyModule(const XL6_IrModule*,XL6_DiagList*);
void xlsReferenceIrModuleFree(XL6_IrModule*);
void xlsReferenceIrDumpModuleJson(XL6_Output*,const XL6_IrModule*);
#define IR_CHECK(v) do{++irChecks;if(!(v)){fprintf(stderr,"analysis_ir_current_probe:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static char* irDump(const XL6_IrModule* module,int original)
{
    XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;
    if(original)xlsReferenceIrDumpModuleJson(&out,module);else xl6IrDumpModuleJson(&out,module);
    char* text=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return text;
}
static int irOrdinary(void)
{
    char* path=xrtPathAbs("current-ir.xl");IR_CHECK(path);XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    IR_CHECK(xl6ParseSource("func first(int value) as int\n return value+1\nend\nfunc second(int value) as int\n return value+2\nend\n",path,&parsed));
    IR_CHECK(xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"ir",1));
    XL6_IrModule actual={0},original={0};XL6_DiagList a,b;xl6DiagInit(&a);xl6DiagInit(&b);xls_ir_memo *memo=NULL,*next=NULL;xls_ir_memo_work work={0};
    IR_CHECK(xlsFrontendIrBuildMemo(&actual,parsed.root,&a,NULL,&memo,&work,32u*1024u*1024u));
    IR_CHECK(xlsReferenceIrBuildModuleFromAst(&original,parsed.root,&b));
    IR_CHECK(!a.count && !b.count && xl6IrVerifyModule(&actual,&a) && xlsReferenceIrVerifyModule(&original,&b));
    char *left=irDump(&actual,0),*right=irDump(&original,1);IR_CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);xl6IrModuleFree(&actual);
    IR_CHECK(xlsFrontendIrBuildMemo(&actual,parsed.root,&a,memo,&next,&work,32u*1024u*1024u));
    IR_CHECK(work.reused>0);left=irDump(&actual,0);right=irDump(&original,1);IR_CHECK(left && right && !strcmp(left,right));
    xrtFree(left);xrtFree(right);xlsFrontendIrMemoDestroy(memo);xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&actual);xlsReferenceIrModuleFree(&original);xl6DiagFree(&a);xl6DiagFree(&b);xl6ParseResultFree(&parsed);xrtFree(path);return 1;
}
static int irProgram(unsigned scenario)
{
    const char* definitions[]={"", "class Box\n int value\n create(int seed)\n self.value=seed\n end\n func read() as int\n return self.value\n end\nend\n",
        "extern \"text\" @no_raise func nativeText() as point\nextern \"free\" @no_raise func nativeFree(point p)\nfunc make() as cstring\n return cstring.take(nativeText(),nativeFree)\nend\nfunc read(string value) as int\n return value.length\nend\n",
        "func identity<T>(T value) as T\n return value\nend\n"};
    const char* bodies[]={" string outer=\"outer\"\n try\n string inner=\"inner\"\n return %u\n catch e\n string message=e.message\n return 8\n finally\n string final=\"final\"\n end\n", " auto box=Box(3)\n return box.read()+%u\n", " cstring value=make()\n return read(value)+%u\n", " return identity::<int>(%u)\n"};
    char* path=xrtPathAbs("current-ir-matrix.xl");IR_CHECK(path);xls_ir_result* previous=NULL;
    for(unsigned version=0;version<3;++version){
        xstrbuf source={0};if(version==2)IR_CHECK(xrtStrBufAppend(&source,xrtStrView("// moved coordinates\n")));
        for(unsigned f=0;f<2;++f){IR_CHECK(xrtStrBufAppendFormat(&source,"func stable_%u() as int\n",f));for(unsigned n=0;n<20;++n)IR_CHECK(xrtStrBufAppendFormat(&source," int value_%u=%u\n",n,n));IR_CHECK(xrtStrBufAppend(&source,xrtStrView(" return value_19\nend\n")));}
        IR_CHECK(xrtStrBufAppend(&source,xrtStrView(definitions[scenario])) && xrtStrBufAppend(&source,xrtStrView("func edited() as int\n")) && xrtStrBufAppendFormat(&source,bodies[scenario],version ? 8 : 7) && xrtStrBufAppend(&source,xrtStrView("end\n")));
        XL6_ParseResult parsed;xl6ParseResultInit(&parsed);IR_CHECK(xl6ParseSource(source.Data,path,&parsed));
        int checked=xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"matrix",1);if(!checked)for(size_t i=0;i<parsed.diags.count;++i)fprintf(stderr,"matrix semantic %u: %s\n",scenario,parsed.diags.items[i].message);IR_CHECK(checked);
        XL6_AstNode* unchanged=xlsAstClone(parsed.root,NULL,NULL);IR_CHECK(unchanged);
        char* before=previous ? irDump(xlsFrontendIrResultModule(previous),0) : NULL;
        XL6_DiagList a,b;xl6DiagInit(&a);xl6DiagInit(&b);
        xls_ir_result* result=xlsFrontendIrResultBuildPersistent(parsed.root,&a,previous,32u*1024u*1024u,128u*1024u,32u*1024u*1024u);
        if(!result){for(size_t i=0;i<a.count;++i)fprintf(stderr,"matrix IR %u: %s\n",scenario,a.items[i].message);}
        IR_CHECK(result && !a.count);
        if(previous){char* after=irDump(xlsFrontendIrResultModule(previous),0);IR_CHECK(before && after && !strcmp(before,after));xrtFree(before);xrtFree(after);xlsFrontendIrResultDestroy(previous);}
        XL6_IrModule original={0};IR_CHECK(xlsReferenceIrBuildModuleFromAst(&original,parsed.root,&b) && !b.count && xlsReferenceIrVerifyModule(&original,&b));
        const XL6_IrModule* actual=xlsFrontendIrResultModule(result);IR_CHECK(xl6IrVerifyModule(actual,&a) && !a.count && sameAst(unchanged,parsed.root));
        char *left=irDump(actual,0),*right=irDump(&original,1);
        if(left && right && strcmp(left,right)){size_t at=0;while(left[at] && right[at] && left[at]==right[at])++at;fprintf(stderr,"matrix mismatch %u/%u at %zu actual=%.120s original=%.120s\n",scenario,version,at,left+at,right+at);}
        IR_CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);
        xls_ir_memo_work work=xlsFrontendIrResultWork(result);xls_ir_persistent_work persistent=xlsFrontendIrResultPersistentWork(result);
        if(version)IR_CHECK(work.reused>0 || persistent.reused);
        printf("current IR scenario %u version %u: raw=%llu persistent=%llu\n",scenario,version,(unsigned long long)work.reused,(unsigned long long)persistent.reused);
        xl6AstFree(unchanged);xl6ParseResultFree(&parsed);xrtStrBufFree(&source);xlsReferenceIrModuleFree(&original);xl6DiagFree(&a);xl6DiagFree(&b);previous=result;
    }
    xlsFrontendIrResultDestroy(previous);xrtFree(path);return 1;
}
int main(void)
{xrtMemStatsEnable(true);if(!xlsTestCurrentIrCodec(&irChecks) || !irOrdinary())return 1;for(unsigned i=0;i<4;++i)if(!irProgram(i))return 1;printf("analysis_ir_current_probe passed: %u checks; actual current memo and unmodified IR oracle\n",irChecks);return 0;}
