#include "analysis/ast_clone.h"
#include "analysis/frontend_ir_memo.h"
#include "analysis/frontend_cancel.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_sem.h"
#include "analysis/check.h"
#include "analysis/check_ir.h"
#include "analysis/check_session.h"
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
static xls_ir_memo_work lastWork;
static int ownerPair(xls_semantic_check* source);
#define CHECK(v) do{xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"analysis_ir_memo_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static char* dump(const XL6_IrModule* ir)
{XL6_Output out;if(!xl6OutputOpenMemory(&out))return NULL;xl6IrDumpModuleJson(&out,ir);char* text=xl6OutputHasFailed(&out) ? NULL : xl6OutputTakeMemoryText(&out,NULL);xl6OutputDiscard(&out);return text;}
static xls_semantic_check* baseChecked(const char* path,const char* source)
{
    char error[256];xls_semantic_input in={path,source,strlen(source)};
    xls_semantic_check* result=xlsSemanticCheckCreate(path,&in,1,1,NULL,NULL,error,sizeof(error));
    if(!result)fprintf(stderr,"fresh semantic:%s\n",error);
    else if(!xlsSemanticCheckInfo(result).accepted){char* value=xlsSemanticCheckJson(result,NULL);fprintf(stderr,"fresh rejected:%s\n",value ? value : "no report");xrtFree(value);}
    return result;
}
static int persistentOracle(xls_semantic_check*);
static xls_semantic_check* checked(const char* path,const char* text)
{ xls_semantic_check* value=baseChecked(path,text);if(value && xlsSemanticCheckInfo(value).accepted && !persistentOracle(value)){xlsSemanticCheckDestroy(value);return NULL;}return value;}
static int compare(xls_semantic_check* source,const xls_ir_memo* previous,xls_ir_memo** next,size_t budget,int reuse)
{
    XL6_IrModule ir;xl6IrModuleInit(&ir);XL6_DiagList diags;xl6DiagInit(&diags);xls_ir_memo_work work;
    CHECK(xlsFrontendIrBuildMemo(&ir,xlsSemanticCheckAst(source),&diags,previous,next,&work,budget));
    lastWork=work;
    CHECK(!diags.count);char *expected=dump(xlsSemanticCheckIr(source)),*actual=dump(&ir);
    if(expected && actual && strcmp(expected,actual)){xrtFileWriteAtomic("memo-ir-expected.json",(xbytesview){(const unsigned char*)expected,strlen(expected)});xrtFileWriteAtomic("memo-ir-actual.json",(xbytesview){(const unsigned char*)actual,strlen(actual)});}
    CHECK(expected && actual && !strcmp(expected,actual));
    if(reuse){printf("raw IR functions checked=%llu reused=%llu captured=%llu nodes=%llu bytes=%llu\n",(unsigned long long)work.checked,(unsigned long long)work.reused,(unsigned long long)work.captured,(unsigned long long)work.nodesReused,(unsigned long long)work.bytes);CHECK(work.reused>0 && work.nodesReused>=(uint64_t)reuse);}
    CHECK(work.bytes<=budget);xrtFree(expected);xrtFree(actual);xl6DiagFree(&diags);xl6IrModuleFree(&ir);return 1;
}
static int keys(const XL6_AstNode* a,const XL6_AstNode* b,xls_ir_key_mode mode,int equal)
{
    xstrbuf first={0},second={0};CHECK(xlsFrontendIrAstKey(&first,a,mode));CHECK(xlsFrontendIrAstKey(&second,b,mode));
    CHECK((first.Size==second.Size && !memcmp(first.Data,second.Data,first.Size))==equal);
    xrtStrBufFree(&first);xrtStrBufFree(&second);return 1;
}
static int keyRegions(void)
{
    XL6_AstNode deepA={.kind=XL6_AST_TYPE,.text="int",.semanticRegionId=7},deepB=deepA;
    XL6_AstNode *factChildrenA[]={&deepA},*factChildrenB[]={&deepB};
    XL6_AstNode factA={.kind=XL6_AST_TYPE,.text="int",.children=factChildrenA,.childCount=1,.semanticRegionId=3},factB=factA;
    factB.children=factChildrenB;
    XL6_AstNode leafA={.kind=XL6_AST_RETURN,.semanticRegionId=102,.semanticTargetRegionId=100},leafB=leafA;
    leafB.semanticRegionId=202;leafB.semanticTargetRegionId=200;
    XL6_AstNode *childrenA[]={&leafA},*childrenB[]={&leafB};
    XL6_AstNode a={.kind=XL6_AST_FUNC,.children=childrenA,.childCount=1,.semanticRegionId=100,.inferredType=&factA},b=a;
    b.children=childrenB;b.semanticRegionId=200;b.inferredType=&factB;
    CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,1));CHECK(keys(&a,&b,XLS_IR_KEY_EXACT,0));CHECK(keys(&a,&b,XLS_IR_KEY_ENVIRONMENT,1));
    leafB.semanticTargetRegionId=0;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));leafB.semanticTargetRegionId=200;
    leafB.semanticTargetRegionId=201;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));leafB.semanticTargetRegionId=200;
    deepB.semanticRegionId=8;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));CHECK(keys(&a,&b,XLS_IR_KEY_ENVIRONMENT,0));deepB.semanticRegionId=7;
    factB.semanticTargetRegionId=1;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));CHECK(keys(&a,&b,XLS_IR_KEY_ENVIRONMENT,0));factB.semanticTargetRegionId=0;
    leafB.flags=1;CHECK(keys(&a,&b,XLS_IR_KEY_ENVIRONMENT,0));leafB.flags=0;
    a.semanticDeclaredType=&factA;b.semanticDeclaredType=&factB;b.inferredType=&factA;
    deepB.semanticRegionId=8;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));CHECK(keys(&a,&b,XLS_IR_KEY_ENVIRONMENT,0));deepB.semanticRegionId=7;
    xstrbuf bad={0};leafB.semanticRegionId=199;CHECK(!xlsFrontendIrAstKey(&bad,&b,XLS_IR_KEY_FUNCTION));xrtStrBufFree(&bad);
    bad=(xstrbuf){0};leafB.semanticRegionId=202;leafB.semanticTargetRegionId=199;CHECK(!xlsFrontendIrAstKey(&bad,&b,XLS_IR_KEY_FUNCTION));xrtStrBufFree(&bad);
    bad=(xstrbuf){0};leafB.semanticTargetRegionId=200;b.semanticRegionId=0;CHECK(!xlsFrontendIrAstKey(&bad,&b,XLS_IR_KEY_FUNCTION));xrtStrBufFree(&bad);
    XL6_AstNode high={.kind=XL6_AST_FUNC,.semanticRegionId=UINT32_MAX},low={.kind=XL6_AST_FUNC,.semanticRegionId=1};
    CHECK(keys(&high,&low,XLS_IR_KEY_FUNCTION,1));high.semanticTargetRegionId=UINT32_MAX;low.semanticTargetRegionId=1;CHECK(keys(&high,&low,XLS_IR_KEY_FUNCTION,1));
    low.semanticTargetRegionId=0;CHECK(keys(&high,&low,XLS_IR_KEY_FUNCTION,0));
    return 1;
}
static int keySymbols(void)
{
    XL6_AstSource sourceA={.path="symbol-key.xl"},sourceB={.path="symbol-key.xl"},foreign={.path="foreign.xl"};
    XL6_AstRange inner={.known=1,.startOffset=10,.endOffset=12},outer={.known=1,.startOffset=0,.endOffset=100};
    XL6_AstNode declarationA={.kind=XL6_AST_NAME,.text="value",.source=&sourceA,.range=inner,.semanticSymbolId=21,.semanticDeclaredSymbolId=21};
    XL6_AstNode declarationB=declarationA;declarationB.source=&sourceB;declarationB.semanticSymbolId=1021;declarationB.semanticDeclaredSymbolId=1021;
    XL6_AstNode duplicateA=declarationA,duplicateB=declarationB;
    XL6_AstNode useA={.kind=XL6_AST_NAME,.text="value",.source=&sourceA,.range=inner,.semanticSymbolId=21,.semanticTargetSymbolId=21};
    XL6_AstNode useB=useA;useB.source=&sourceB;useB.semanticSymbolId=useB.semanticTargetSymbolId=1021;
    XL6_AstNode nameA={.kind=XL6_AST_NAME,.text="function",.source=&sourceA,.range=inner,.semanticSymbolId=8,.semanticDeclaredSymbolId=8},nameB=nameA;
    nameB.source=&sourceB;
    XL6_AstNode *childrenA[]={&nameA,&declarationA,&duplicateA,&useA},*childrenB[]={&nameB,&declarationB,&duplicateB,&useB};
    XL6_AstNode a={.kind=XL6_AST_FUNC,.source=&sourceA,.range=outer,.semanticSymbolId=7,.semanticDeclaredSymbolId=7,.semanticRegionId=1,.children=childrenA,.childCount=4},b=a;
    b.source=&sourceB;b.children=childrenB;
    CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,1));CHECK(keys(&a,&b,XLS_IR_KEY_EXACT,0));
    xls_lexical_symbols symbols={0};CHECK(xlsLexicalSymbolsBuild(&symbols,&b,XLS_LEXICAL_SYMBOL_BYTES));
    int local;CHECK(xlsLexicalSymbolKey(&symbols,1021,&local)==1 && local);
    CHECK(xlsLexicalSymbolKey(&symbols,7,&local)==7 && !local);
    CHECK(xlsLexicalSymbolKey(&symbols,8,&local)==8 && !local);
    XL6_SymbolId id;CHECK(xlsLexicalSymbolResolve(&symbols,1,1,&id) && id==1021);
    CHECK(xlsLexicalSymbolResolve(&symbols,UINT32_MAX,0,&id) && id==UINT32_MAX);
    CHECK(!xlsLexicalSymbolResolve(&symbols,0,1,&id) && !xlsLexicalSymbolResolve(&symbols,3,1,&id));
    CHECK(!xlsLexicalSymbolResolve(&symbols,1,2,&id));xlsLexicalSymbolsClear(&symbols);
    CHECK(!xlsLexicalSymbolsBuild(&symbols,&b,1) && !symbols.ready && !symbols.order.Capacity && !symbols.sorted.Capacity);
    useA.semanticTargetSymbolId=1;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));useA.semanticTargetSymbolId=21;
    useA.semanticCompoundOperatorSymbolId=500;useB.semanticCompoundOperatorSymbolId=501;
    CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));useA.semanticCompoundOperatorSymbolId=useB.semanticCompoundOperatorSymbolId=0;
    XL6_AstNode typeA={.kind=XL6_AST_TYPE,.text="int",.semanticSymbolId=21},typeB=typeA;typeB.semanticSymbolId=1021;
    useA.inferredType=&typeA;useB.inferredType=&typeB;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));useA.inferredType=useB.inferredType=NULL;
    useA.semanticTypeId=80;useB.semanticTypeId=81;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));useA.semanticTypeId=useB.semanticTypeId=0;
    declarationB.source=&foreign;duplicateB.source=&foreign;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));
    declarationB.source=duplicateB.source=&sourceB;declarationB.range.known=duplicateB.range.known=0;
    CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));declarationB.range=duplicateB.range=inner;
    b.semanticSymbolId=b.semanticDeclaredSymbolId=8;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));
    b.semanticSymbolId=b.semanticDeclaredSymbolId=7;nameB.semanticSymbolId=nameB.semanticDeclaredSymbolId=9;CHECK(keys(&a,&b,XLS_IR_KEY_FUNCTION,0));
    return 1;
}
static char* body(unsigned edit)
{
    xstrbuf b;xrtStrBufInit(&b);int okay=xrtStrBufAppend(&b,xrtStrView("\xef\xbb\xbf// 中文😀\r\nfunc identity(int value) as int\r\n return value\r\nend\r\n"));
    if(edit==3)okay=okay && xrtStrBufAppend(&b,xrtStrView("// moved declarations 😀\r\n\r\n"));
    for(unsigned i=0;okay && i<16;++i){
        okay=xrtStrBufAppendFormat(&b,"func helper_%u(int value) as int\r\n int local = value + %u\r\n if local > 0 then\r\n",i,i==4 && edit==1 ? 99999 : i);
        if(i==4 && edit==7)okay=okay && xrtStrBufAppend(&b,xrtStrView(" if local > 2\r\n local = local + 5\r\n end\r\n"));
        if(i==4 && edit==2)okay=okay && xrtStrBufAppend(&b,xrtStrView(" local = local + 3\r\n local = local + 4\r\n"));
        if(i==4 && edit==4)okay=okay && xrtStrBufAppend(&b,xrtStrView(" int inserted = 3\r\n local = local + inserted\r\n"));
        if(i==4 && edit==5)okay=okay && xrtStrBufAppend(&b,xrtStrView(" string inserted = \"a\\0😀\"\r\n"));
        if(i==4 && edit==6)okay=okay && xrtStrBufAppend(&b,xrtStrView(" float inserted = 3.5\r\n"));
        for(unsigned j=0;okay && j<12;++j)okay=xrtStrBufAppend(&b,xrtStrView(" local = identity(local)+1\r\n"));
        okay=okay && xrtStrBufAppend(&b,xrtStrView(" end\r\n return local\r\nend\r\n"));
    }
    okay=okay && xrtStrBufAppend(&b,xrtStrView("func main(...) as int\r\n return helper_0(7)\r\nend\r\n"));
    char* text=okay ? xrtStrDup(b.Data) : NULL;xrtStrBufFree(&b);return text;
}
static int incremental(const char* path)
{
    xls_ir_memo* memo=NULL;char* original=body(0);CHECK(original);
    xls_semantic_check* held=checked(path,original);CHECK(held && xlsSemanticCheckInfo(held).accepted);
    CHECK(compare(held,NULL,&memo,32u*1024u*1024u,0) && memo);
    xls_ir_memo* exact=NULL;CHECK(compare(held,memo,&exact,32u*1024u*1024u,100));xlsFrontendIrMemoDestroy(exact);
    for(unsigned edit=1;edit<=6;++edit){
        char* text=body(edit);CHECK(text);xls_semantic_check* current=checked(path,text);CHECK(current && xlsSemanticCheckInfo(current).accepted);
        xls_ir_memo* next=NULL;CHECK(compare(current,memo,&next,32u*1024u*1024u,100));
        if(edit==4)CHECK(lastWork.reused==14); /* local numbering moved;types did not */
        if(edit==6)CHECK(lastWork.reused==3); /* same count,different type-prefix values */
        xlsFrontendIrMemoDestroy(memo);memo=next;xlsSemanticCheckDestroy(current);xrtFree(text);
    }
    xlsFrontendIrMemoDestroy(memo);memo=NULL;CHECK(compare(held,NULL,&memo,32u*1024u*1024u,0));
    char* regions=body(7);CHECK(regions);xls_semantic_check* changed=checked(path,regions);CHECK(changed && xlsSemanticCheckInfo(changed).accepted);
    xls_ir_memo* moved=NULL;CHECK(compare(changed,memo,&moved,32u*1024u*1024u,1));CHECK(lastWork.reused==14);
    xlsFrontendIrMemoDestroy(moved);xlsSemanticCheckDestroy(changed);xrtFree(regions);
    CHECK(xlsSemanticCheckValidate(held));
    for(unsigned budget=0;budget<4;++budget){xls_ir_memo* tiny=NULL;CHECK(compare(held,memo,&tiny,budget ? (size_t)1<<(budget*4) : 0,0));xlsFrontendIrMemoDestroy(tiny);}
    xlsFrontendIrMemoDestroy(memo);xlsSemanticCheckDestroy(held);xrtFree(original);return 1;
}
static int corpus(const char* path)
{
    const char* sources[]={
        "string global = \"global\"\nfunc choose(bool yes) as string\n string outer = \"a\\0😀\"\n if yes\n string branch = \"branch\"\n return branch\n else\n string other = \"other\"\n return other\n end\nend\n",
        "func closure() as func() as int\n array values = [41]\n return func() as int\n return int(values[0])\n end\nend\n",
        "func guarded() as int\n string outer = \"outer\"\n try\n string inner = \"inner\"\n return 7\n catch e\n string message = e.message\n return 8\n finally\n string final = \"final\"\n end\nend\n",
        "func values(int count) as generator<string>\n string suffix = \"owned\"\n for i in range(count)\n string item = suffix + string(i)\n yield item\n end\nend\n",
        "class Box\n string item = \"owned\"\n func value() as string\n string local = self.item\n return local\n end\nend\nfunc read(Box item) as string\n return item.value()\nend\n",
        "protocol Named\n func name() as string\nend\nfunc named(Named item) as int\n return 0\nend\n",
        "typedef Count = int\ntypedef Other = Count\nfunc aliases(Other value) as Count\n return value\nend\n",
        "enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n",
        "func apply(func(int) as int action, int value) as int\n return action(value)\nend\nfunc run() as int\n return apply(func(int value) as int\n return value + 1\n end, 7)\nend\n",
        "func identity<T>(T value) as T\n return value\nend\nfunc run() as int\n return identity::<int>(7)\nend\n",
        "func accept(array items) as int\n return int(items[0])\nend\nfunc erased() as int\n return accept([1, 2, 3])\nend\n",
        "func accept(func() as int callback) as int\n return callback()\nend\nfunc erased(var boxed) as int\n return accept(boxed)\nend\n"
    };
    for(size_t i=0;i<sizeof(sources)/sizeof(*sources);++i){
        /* These fixed corpora end with a named,non-recursive function. Append
         * a differently named copy to prime its real types,then exercise reuse
         * of closures,generators,handlers and nominal/alias/callable cases. */
        const char* last=!strncmp(sources[i],"func ",5) ? sources[i] : NULL;
        for(const char* at=sources[i];(at=strstr(at,"\nfunc "))!=NULL;++at)last=at+1;
        CHECK(last);const char* signature=strchr(last,'(');CHECK(signature);
        xstrbuf input;xrtStrBufInit(&input);CHECK(xrtStrBufAppend(&input,xrtStrView(sources[i])) && xrtStrBufAppend(&input,xrtStrView("\nfunc replica")) && xrtStrBufAppend(&input,xrtStrView(signature)));
        xls_semantic_check* current=checked(path,input.Data);CHECK(current && xlsSemanticCheckInfo(current).accepted);
        xls_ir_memo *first=NULL,*next=NULL;CHECK(compare(current,NULL,&first,32u*1024u*1024u,0));
        CHECK(compare(current,first,&next,32u*1024u*1024u,1));xlsFrontendIrMemoDestroy(first);xlsFrontendIrMemoDestroy(next);CHECK(ownerPair(current));xlsSemanticCheckDestroy(current);xrtStrBufFree(&input);
    }
    return 1;
}
static int bindings(const char* path)
{
    const char* before="func seed(int value) as int\n return value\nend\nfunc earlier(int value) as int\n return seed(value)\nend\nfunc unchanged(int value) as int\n return later(value)\nend\nfunc later(int value) as int\n return value\nend\n";
    const char* after="func seed(int value) as int\n return value\nend\nfunc earlier(int value) as int\n return later(value)\nend\nfunc unchanged(int value) as int\n return later(value)\nend\nfunc later(int value) as int\n return value\nend\n";
    xls_semantic_check *a=checked(path,before),*b=checked(path,after);CHECK(a && b && xlsSemanticCheckInfo(a).accepted && xlsSemanticCheckInfo(b).accepted);
    xls_ir_memo *first=NULL,*next=NULL;CHECK(compare(a,NULL,&first,32u*1024u*1024u,0));
    CHECK(compare(b,first,&next,32u*1024u*1024u,1));CHECK(lastWork.reused==1);
    xlsFrontendIrMemoDestroy(first);xlsFrontendIrMemoDestroy(next);xlsSemanticCheckDestroy(a);xlsSemanticCheckDestroy(b);return 1;
}
static int regionCorpus(const char* path)
{
    const char* tails[]={
        "func loop(int count) as int\n while count > 0\n count = count - 1\n if count > 5\n continue\n end\n while count > 1\n break 2\n end\n break\n end\n return count\nend\n",
        "func guarded() as int\n string outer = \"outer\"\n try\n string inner = \"inner\"\n return 7\n catch e\n string message = e.message\n return 8\n finally\n string final = \"final\"\n end\nend\n",
        "func closure() as func() as int\n array values = [41]\n return func() as int\n return int(values[0])\n end\nend\n",
        "func values(int count) as generator<string>\n string suffix = \"owned\"\n for i in range(count)\n string item = suffix + string(i)\n yield item\n end\nend\n",
        "class Box\n string item = \"owned\"\n func value() as string\n string local = self.item\n return local\n end\nend\nfunc read(Box item) as string\n return item.value()\nend\n",
        "protocol Named\n func name() as string\nend\ntypedef Count = int\nfunc named(Named item) as Count\n return 0\nend\n",
        "func identity<T>(T value) as T\n return value\nend\nfunc run() as int\n return identity::<int>(7)\nend\n",
        "enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n"
    };
    for(size_t i=0;i<sizeof(tails)/sizeof(*tails);++i){
        const char* last=!strncmp(tails[i],"func ",5) ? tails[i] : NULL;
        for(const char* at=tails[i];(at=strstr(at,"\nfunc "))!=NULL;++at)last=at+1;
        CHECK(last);const char* signature=strchr(last,'(');CHECK(signature);
        xstrbuf source[2]={{0}};xls_semantic_check* model[2]={0};
        for(size_t edit=0;edit<2;++edit){
            CHECK(xrtStrBufAppend(&source[edit],xrtStrView("\xef\xbb\xbf// 中文😀\r\nfunc lead(int value) as int\n if value > 0\n")));
            if(edit)CHECK(xrtStrBufAppend(&source[edit],xrtStrView(" if value > 2\n return value\n end\n")));
            CHECK(xrtStrBufAppend(&source[edit],xrtStrView(" return value\n end\n return 0\nend\n")) &&
                xrtStrBufAppend(&source[edit],xrtStrView(tails[i])) && xrtStrBufAppend(&source[edit],xrtStrView("\nfunc replica")) && xrtStrBufAppend(&source[edit],xrtStrView(signature)));
            model[edit]=checked(path,source[edit].Data);CHECK(model[edit] && xlsSemanticCheckInfo(model[edit]).accepted);
        }
        xls_ir_memo *before=NULL,*after=NULL;CHECK(compare(model[0],NULL,&before,32u*1024u*1024u,0));uint64_t captured=lastWork.captured;
        CHECK(captured>0 && compare(model[1],before,&after,32u*1024u*1024u,1));CHECK(lastWork.reused>0 && lastWork.reused+1>=captured);
        uint64_t reused=lastWork.reused;XL6_DiagList diags;xl6DiagInit(&diags);
        xls_ir_result* ownerBefore=xlsFrontendIrResultBuild(xlsSemanticCheckAst(model[0]),&diags,NULL,32u*1024u*1024u);CHECK(ownerBefore && !diags.count);
        xls_ir_result* ownerAfter=xlsFrontendIrResultBuild(xlsSemanticCheckAst(model[1]),&diags,ownerBefore,32u*1024u*1024u);CHECK(ownerAfter && !diags.count);
        CHECK(xlsFrontendIrResultWork(ownerAfter).reused==reused && xlsFrontendIrResultWork(ownerAfter).textNodes);
        char* expected=dump(xlsSemanticCheckIr(model[1]));CHECK(expected);
        /* Changing nominal context still invalidates every function unit.
         * Only unused lexical execution IDs are omitted from that context. */
        if(i==4){
            char* word=strstr(source[1].Data,"owned");CHECK(word);memcpy(word,"bound",5);
            xls_semantic_check* altered=checked(path,source[1].Data);CHECK(altered && xlsSemanticCheckInfo(altered).accepted);
            xls_ir_memo* invalidated=NULL;CHECK(compare(altered,after,&invalidated,32u*1024u*1024u,0));CHECK(!lastWork.reused);
            xlsFrontendIrMemoDestroy(invalidated);xlsSemanticCheckDestroy(altered);
        }
        xlsFrontendIrMemoDestroy(before);xlsFrontendIrMemoDestroy(after);xlsFrontendIrResultDestroy(ownerBefore);
        for(size_t edit=0;edit<2;++edit){xlsSemanticCheckDestroy(model[edit]);xrtStrBufFree(&source[edit]);}
        char* actual=dump(xlsFrontendIrResultModule(ownerAfter));CHECK(actual && !strcmp(actual,expected));
        xrtFree(actual);xrtFree(expected);xlsFrontendIrResultDestroy(ownerAfter);xl6DiagFree(&diags);
    }
    return 1;
}
typedef struct reader {const XL6_AstNode* root;const xls_ir_memo* memo;const char* expected;int passed;} reader;
static int32 readThread(void* user)
{
    reader* r=user;XL6_IrModule ir;xl6IrModuleInit(&ir);XL6_DiagList d;xl6DiagInit(&d);xls_ir_memo* next=NULL;xls_ir_memo_work work;
    int okay=xlsFrontendIrBuildMemo(&ir,r->root,&d,r->memo,&next,&work,32u*1024u*1024u);char* actual=okay ? dump(&ir) : NULL;
    r->passed=okay && actual && !strcmp(actual,r->expected) && work.reused>0 && !d.count;
    xrtFree(actual);xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&ir);xl6DiagFree(&d);return 0;
}
static int stop(void* user)
{xls_frontend_cancel_work w=xlsFrontendCancelWork();return w.kind==XLS_CANCEL_IR_NODE && w.points[XLS_CANCEL_IR_NODE]>=*(unsigned*)user;}
static int lifetime(const char* path)
{
    char* text=body(0);CHECK(text);xls_semantic_check* check=checked(path,text);CHECK(check && xlsSemanticCheckInfo(check).accepted);
    xls_ir_memo* memo=NULL;CHECK(compare(check,NULL,&memo,32u*1024u*1024u,0));char* expected=dump(xlsSemanticCheckIr(check));CHECK(memo && expected);
    for(unsigned gate=64;gate<=512;gate*=2){
        XL6_IrModule ir;xl6IrModuleInit(&ir);XL6_DiagList d;xl6DiagInit(&d);xls_ir_memo* cancelled=NULL;xls_frontend_cancel_scope control;
        xlsFrontendCancelBegin(&control,stop,&gate);CHECK(!xlsFrontendIrBuildMemo(&ir,xlsSemanticCheckAst(check),&d,memo,&cancelled,NULL,32u*1024u*1024u));
        CHECK(!cancelled && xlsFrontendCancelObserved());xlsFrontendCancelEnd(&control,NULL);xl6IrModuleFree(&ir);xl6DiagFree(&d);
        xls_ir_memo* retry=NULL;CHECK(compare(check,memo,&retry,32u*1024u*1024u,1));xlsFrontendIrMemoDestroy(retry);
    }
    reader readers[4];xthread* threads[4];
    for(unsigned i=0;i<4;++i){readers[i]=(reader){xlsSemanticCheckAst(check),memo,expected,0};threads[i]=xrtThreadCreate(readThread,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
    xlsFrontendIrMemoDestroy(memo);xlsSemanticCheckDestroy(check);xrtFree(expected);xrtFree(text);return 1;
}
typedef struct owner_reader {const XL6_AstNode* root;const xls_ir_result* baseline;const char* expected;int passed;} owner_reader;
static int32 ownerThread(void* user)
{
    owner_reader* reader=user;XL6_DiagList diags;xl6DiagInit(&diags);
    xls_ir_result* result=xlsFrontendIrResultBuild(reader->root,&diags,reader->baseline,32u*1024u*1024u);
    char* actual=result ? dump(xlsFrontendIrResultModule(result)) : NULL;
    xls_ir_memo_work work=xlsFrontendIrResultWork(result);
    reader->passed=result && actual && !strcmp(actual,reader->expected) && work.reused && work.textNodes && work.textBytes && !diags.count;
    xrtFree(actual);xlsFrontendIrResultDestroy(result);xl6DiagFree(&diags);return 0;
}
static int ownerPair(xls_semantic_check* source)
{
    const XL6_AstNode* root=xlsSemanticCheckAst(source);XL6_DiagList diags;xl6DiagInit(&diags);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);
    xls_ir_result* first=xlsFrontendIrResultBuild(root,&diags,NULL,32u*1024u*1024u);CHECK(first && !diags.count);
    xls_ir_result* second=xlsFrontendIrResultBuild(root,&diags,first,32u*1024u*1024u);CHECK(second && !diags.count);
    xls_ir_memo_work work=xlsFrontendIrResultWork(second);CHECK(work.reused && work.textNodes && work.textBytes && work.ownerBytes);
    xlsFrontendIrResultDestroy(first);
    char* actual=dump(xlsFrontendIrResultModule(second));CHECK(actual && !strcmp(actual,expected));xrtFree(actual);
    xls_ir_result* third=xlsFrontendIrResultBuild(root,&diags,second,32u*1024u*1024u);CHECK(third && !diags.count);xlsFrontendIrResultDestroy(second);
    actual=dump(xlsFrontendIrResultModule(third));CHECK(actual && !strcmp(actual,expected));xrtFree(actual);
    xlsFrontendIrResultDestroy(third);xrtFree(expected);xl6DiagFree(&diags);return 1;
}
static int ownerLifetime(const char* path)
{
    char* text=body(0);CHECK(text);xls_semantic_check* source=checked(path,text);CHECK(source && xlsSemanticCheckInfo(source).accepted);
    const XL6_AstNode* root=xlsSemanticCheckAst(source);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);
    XL6_DiagList diags;xl6DiagInit(&diags);xls_ir_result* first=xlsFrontendIrResultBuild(root,&diags,NULL,32u*1024u*1024u);CHECK(first);
    xls_ir_result* baseline=xlsFrontendIrResultBuild(root,&diags,first,32u*1024u*1024u);CHECK(baseline && xlsFrontendIrResultWork(baseline).textNodes);xlsFrontendIrResultDestroy(first);
    for(unsigned gate=64;gate<=1024;gate*=2){
        xls_frontend_cancel_scope control;xlsFrontendCancelBegin(&control,stop,&gate);
        xls_ir_result* cancelled=xlsFrontendIrResultBuild(root,&diags,baseline,32u*1024u*1024u);
        CHECK(!cancelled && xlsFrontendCancelObserved());xlsFrontendCancelEnd(&control,NULL);xl6DiagFree(&diags);xl6DiagInit(&diags);
        xls_ir_result* retry=xlsFrontendIrResultBuild(root,&diags,baseline,32u*1024u*1024u);CHECK(retry && !diags.count && xlsFrontendIrResultWork(retry).textNodes);xlsFrontendIrResultDestroy(retry);
    }
    const size_t limits[]={0,1,16,128,4096,128u*1024u};
    for(size_t i=0;i<sizeof(limits)/sizeof(*limits);++i){
        xls_ir_result* bounded=xlsFrontendIrResultBuildWithStorage(root,&diags,baseline,32u*1024u*1024u,limits[i]);
        CHECK(bounded && !diags.count);xls_ir_memo_work work=xlsFrontendIrResultWork(bounded);
        CHECK(work.reused && work.ownerBytes<=limits[i]);
        if(!limits[i])CHECK(!work.textNodes && !work.textBytes && !work.ownerBytes);
        char* value=dump(xlsFrontendIrResultModule(bounded));CHECK(value && !strcmp(value,expected));xrtFree(value);xlsFrontendIrResultDestroy(bounded);
    }
    owner_reader readers[4];xthread* threads[4];
    for(unsigned i=0;i<4;++i){readers[i]=(owner_reader){root,baseline,expected,0};threads[i]=xrtThreadCreate(ownerThread,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
    xls_ir_result* held=xlsFrontendIrResultBuild(root,&diags,baseline,32u*1024u*1024u);CHECK(held && !diags.count);
    xlsFrontendIrResultDestroy(baseline);xlsSemanticCheckDestroy(source);xrtFree(text);
    char* actual=dump(xlsFrontendIrResultModule(held));CHECK(actual && !strcmp(actual,expected));xrtFree(actual);xrtFree(expected);xlsFrontendIrResultDestroy(held);xl6DiagFree(&diags);return 1;
}
static int sessionDefault(const char* path)
{
    xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_semantic_check* held=NULL;char error[256];
    const unsigned edits[]={0,1,7,0};
    for(size_t i=0;i<sizeof(edits)/sizeof(*edits);++i){
        char* text=body(edits[i]);CHECK(text);xls_text* source=xlsTextCreate(text,strlen(text),8u*1024u*1024u,NULL);CHECK(source);
        xls_analysis_document document={"entry",path,source,1,i+1};
        xls_semantic_check* result=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));
        CHECK(result && xlsSemanticCheckInfo(result).accepted);xls_check_ir_work work=xlsSemanticCheckIrWork(result);
        CHECK(work.bytes && work.bytes<=32u*1024u*1024u && work.ownerBytes<=128u*1024u);
        if(i==0)CHECK(work.captured && !work.reused && !work.textNodes);
        else CHECK(work.reused && work.nodesReused && work.textNodes && work.textBytes);
        if(edits[i]==7){
            /* These are independent caches;IR can reuse a function that the
             * semantic memo rechecked. Full current facts prove its key. */
            printf("session region edit:raw reused=%llu;semantic reused=%llu\n",(unsigned long long)work.reused,(unsigned long long)xlsSemanticCheckWork(result).functionsReused);
            CHECK(work.reused==14);
            CHECK(xlsSemanticCheckWork(result).functionsReused==16);
        }
        xls_semantic_check* fresh=checked(path,text);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
        xls_check_ir_work independent=xlsSemanticCheckIrWork(fresh);CHECK(!independent.bytes && !independent.reused && !independent.textNodes);
        char *expected=dump(xlsSemanticCheckIr(fresh)),*actual=dump(xlsSemanticCheckIr(result));CHECK(expected && actual && !strcmp(expected,actual));xrtFree(expected);xrtFree(actual);xlsSemanticCheckDestroy(fresh);
        xls_semantic_check* exact=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(exact==result);
        xls_check_ir_work repeat=xlsSemanticCheckIrWork(exact);CHECK(!memcmp(&work,&repeat,sizeof(work)));xlsSemanticCheckDestroy(exact);
        if(!held)held=xlsSemanticCheckRetain(result);
        xlsSemanticCheckDestroy(result);xlsTextRelease(source);xrtFree(text);
    }
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(held));char* actual=dump(xlsSemanticCheckIr(held));CHECK(actual);xrtFree(actual);xlsSemanticCheckDestroy(held);return 1;
}
static int symbolRebinding(const char* path)
{
    xls_semantic_check* models[4]={0};char* expected[4]={0};xls_ir_memo* base=NULL;
    const char* tail="func identity(int value) as int\n return value\nend\nfunc unchanged(int value) as int\n int local=value\n if local>0 then\n int value=local+1\n local=value\n end\n return identity(local)\nend\nfunc forward(int value) as int\n return later(value)\nend\nfunc later(int value) as int\n return value\nend\n";
    for(unsigned i=0;i<4;++i){
        xstrbuf body={0};CHECK(xrtStrBufAppend(&body,xrtStrView("\xef\xbb\xbf// 中文😀\r\nfunc lead(int value) as int\n")));
        for(unsigned j=0;j<i;++j)CHECK(xrtStrBufAppendFormat(&body," int local_%u=value+%u\n",j,j));
        CHECK(xrtStrBufAppend(&body,xrtStrView(" return value\nend\n")) && xrtStrBufAppend(&body,xrtStrView(tail)));
        models[i]=checked(path,body.Data);CHECK(models[i] && xlsSemanticCheckInfo(models[i]).accepted);xrtStrBufFree(&body);
        expected[i]=dump(xlsSemanticCheckIr(models[i]));CHECK(expected[i]);
    }
    CHECK(compare(models[0],NULL,&base,32u*1024u*1024u,0));
    reader readers[4];xthread* threads[4];
    for(unsigned i=0;i<4;++i){readers[i]=(reader){xlsSemanticCheckAst(models[i]),base,expected[i],0};threads[i]=xrtThreadCreate(readThread,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
    const unsigned versions[]={1,3,2,0,3,0};
    for(size_t i=0;i<sizeof(versions)/sizeof(*versions);++i){
        xls_ir_memo* next=NULL;CHECK(compare(models[versions[i]],base,&next,32u*1024u*1024u,1));
        CHECK(lastWork.reused>=2);xlsFrontendIrMemoDestroy(base);base=next;
    }
    xlsFrontendIrMemoDestroy(base);
    for(unsigned i=0;i<4;++i){CHECK(xlsSemanticCheckValidate(models[i]));char* actual=dump(xlsSemanticCheckIr(models[i]));CHECK(actual && !strcmp(actual,expected[i]));xrtFree(actual);xrtFree(expected[i]);xlsSemanticCheckDestroy(models[i]);}
    return 1;
}
static int run(void)
{char* path=xrtPathAbs("ir-memo-中文.xl");CHECK(path);CHECK(keyRegions() && keySymbols() && incremental(path) && corpus(path) && bindings(path) && regionCorpus(path) && lifetime(path) && ownerLifetime(path) && sessionDefault(path) && symbolRebinding(path));xrtFree(path);return 1;}
int legacyIrMemoMain(void)
{xrtAtomic32Init(&checks,0);if(!run())return 1;printf("analysis_ir_memo_test passed: %u checks;fresh complete XLIR parity,changed bodies,node/child rebasing,Unicode spans,type/binding fences,budget fallback,cancellation,held fragments and4threads.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}

static unsigned persistentCases;
static int shiftedNode(const XL6_AstNode* input,XL6_AstNode* output,void* user)
{
    int delta=*(const int*)user;
    if(input->line>0)output->line=input->line+delta;
    if(input->column>0)output->column=input->column+3;
    return 1;
}
static int persistentEqual(const xls_ir_result* owner,const XL6_AstNode* ast)
{
    XL6_IrModule ir;xl6IrModuleInit(&ir);XL6_DiagList diags;xl6DiagInit(&diags);
    CHECK(xl6IrBuildModuleFromAst(&ir,ast,&diags));CHECK(!diags.count);
    char *actual=dump(xlsFrontendIrResultModule(owner)),*expected=dump(&ir);
    if(actual && expected && strcmp(actual,expected)){xrtFileWriteAtomic("persistent-expected.json",(xbytesview){(const unsigned char*)expected,strlen(expected)});xrtFileWriteAtomic("persistent-actual.json",(xbytesview){(const unsigned char*)actual,strlen(actual)});}
    CHECK(actual && expected && !strcmp(actual,expected));
    xrtFree(actual);xrtFree(expected);xl6IrModuleFree(&ir);xl6DiagFree(&diags);return 1;
}
static xls_ir_result* persistentBuild(const XL6_AstNode* root,const xls_ir_result* before,size_t budget)
{
    XL6_DiagList diags;xl6DiagInit(&diags);
    xls_ir_result* result=xlsFrontendIrResultBuildPersistent(root,&diags,before,32u*1024u*1024u,128u*1024u,budget);
    if(diags.count){xlsFrontendIrResultDestroy(result);result=NULL;}
    xl6DiagFree(&diags);return result;
}
static int persistentOracle(xls_semantic_check* source)
{
    const XL6_AstNode* root=xlsSemanticCheckAst(source);CHECK(root);
    xls_ir_result* first=persistentBuild(root,NULL,32u*1024u*1024u);CHECK(first);CHECK(persistentEqual(first,root));
    xls_ir_persistent_work work=xlsFrontendIrResultPersistentWork(first);
    if(!work.captured)fprintf(stderr,"capture declined: stage=%d ready=%d nodes=%zu instructions=%zu key=%llu\n",(int)xlsFrontendIrResultModule(first)->stage,xlsFrontendIrResultModule(first)->ready,xlsFrontendIrResultModule(first)->nodeCount,xlsFrontendIrResultModule(first)->instructionCount,(unsigned long long)work.keyBytes);
    CHECK(work.captured==1 && work.built==1 && !work.reused);
    int delta=17;xls_ast_clone_options options={.transform=shiftedNode,.user=&delta,.bytes=64u*1024u*1024u};
    XL6_AstNode* moved=xlsAstClone(root,&options,NULL);CHECK(moved);
    char* retained=dump(xlsFrontendIrResultModule(first));CHECK(retained);
    xls_ir_result* second=persistentBuild(moved,first,32u*1024u*1024u);CHECK(second);
    work=xlsFrontendIrResultPersistentWork(second);
    CHECK(work.reused==1 && !work.built && work.verified==1 && work.tablesShared==1 && work.nodesCopied>0);
    CHECK(xlsFrontendIrResultModule(second)->nodes!=xlsFrontendIrResultModule(first)->nodes);
    CHECK(xlsFrontendIrResultModule(second)->types.items==xlsFrontendIrResultModule(first)->types.items);
    CHECK(xlsFrontendIrResultModule(second)->children==xlsFrontendIrResultModule(first)->children);
    CHECK(persistentEqual(second,moved));
    char* unchanged=dump(xlsFrontendIrResultModule(first));CHECK(unchanged && !strcmp(unchanged,retained));xrtFree(unchanged);xrtFree(retained);
    xlsFrontendIrResultDestroy(first);CHECK(persistentEqual(second,moved));
    const size_t budgets[]={0,1,256,4096,32u*1024u*1024u,SIZE_MAX};
    for(size_t i=0;i<sizeof(budgets)/sizeof(*budgets);++i){
        xls_ir_result* next=persistentBuild(moved,second,budgets[i]);CHECK(next);CHECK(persistentEqual(next,moved));
        work=xlsFrontendIrResultPersistentWork(next);CHECK(work.metadataBytes<=(budgets[i]<32u*1024u*1024u ? budgets[i] : 32u*1024u*1024u));
        if(!budgets[i])CHECK(!work.reused && !work.captured && work.built==1);
        xlsFrontendIrResultDestroy(next);CHECK(persistentEqual(second,moved));
    }
    XL6_AstNode* again=xlsAstClone(moved,&options,NULL);CHECK(again);
    xls_ir_result* third=persistentBuild(again,second,32u*1024u*1024u);CHECK(third);CHECK(xlsFrontendIrResultPersistentWork(third).reused==1);
    xlsFrontendIrResultDestroy(second);xl6AstFree(moved);CHECK(persistentEqual(third,again));
    xl6AstFree(again);char* detached=dump(xlsFrontendIrResultModule(third));CHECK(detached);xrtFree(detached);xlsFrontendIrResultDestroy(third);
    ++persistentCases;return 1;
}
static int realWhitespace(void)
{
    const char* first="func identity(int value) as int\n return value\nend\nfunc left(int value) as int\n return value+1\nend\nfunc right(int value) as int\n return value+2\nend\nfunc changed(int value) as int\n int local=value+7\n return local\nend\n";
    char second[4096],third[4096];CHECK(snprintf(second,sizeof(second),"\n\n%s",first)>0);memcpy(third,second,strlen(second)+1);char* literal=strstr(third,"value+7");CHECK(literal);literal[6]='9';
    char* path=xrtPathAbs("persistent-real-中文.xl");CHECK(path);
    xls_semantic_check *a=baseChecked(path,first),*b=baseChecked(path,second),*c=baseChecked(path,third);CHECK(a && b && c && xlsSemanticCheckInfo(a).accepted && xlsSemanticCheckInfo(b).accepted && xlsSemanticCheckInfo(c).accepted);
    xls_ir_result* before=persistentBuild(xlsSemanticCheckAst(a),NULL,32u*1024u*1024u);CHECK(before);
    xls_ir_result* after=persistentBuild(xlsSemanticCheckAst(b),before,32u*1024u*1024u);CHECK(after);CHECK(xlsFrontendIrResultPersistentWork(after).reused==1);CHECK(persistentEqual(after,xlsSemanticCheckAst(b)));
    xlsFrontendIrResultDestroy(before);
    xls_ir_result* changed=persistentBuild(xlsSemanticCheckAst(c),after,32u*1024u*1024u);CHECK(changed);
    CHECK(xlsFrontendIrResultPersistentWork(changed).built==1 && !xlsFrontendIrResultPersistentWork(changed).reused);CHECK(xlsFrontendIrResultWork(changed).reused>0);CHECK(persistentEqual(changed,xlsSemanticCheckAst(c)));
    xlsFrontendIrResultDestroy(after);CHECK(persistentEqual(changed,xlsSemanticCheckAst(c)));xlsFrontendIrResultDestroy(changed);
    xlsSemanticCheckDestroy(a);xlsSemanticCheckDestroy(b);xlsSemanticCheckDestroy(c);xrtFree(path);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!run() || !realWhitespace())return 1;
    printf("analysis_ir_persistent_test passed: %u checks;cases=%u;complete originalIR parity,independent readonly tables/text,current spans,held result/AST retirement,budgets andraw-cache continuation.\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),persistentCases);return 0;
}
