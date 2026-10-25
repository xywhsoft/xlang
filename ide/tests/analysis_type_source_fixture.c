/* Diagnostic-only TU: use the exact generated semantic compiler for both
 * the accepted baseline and candidate. Production has no test switch. */
#include <xrt.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include XLS_TYPE_SOURCE_FIXTURE
#pragma GCC diagnostic pop
#include "analysis/check.h"
#include <stdio.h>
static unsigned sourceChecks;
#define SOURCE_CHECK(v) do{++sourceChecks;if(!(v)){fprintf(stderr,"type source fixture:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int sourceRange(XL6_AstRange a,XL6_AstRange b)
{return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.endLine==b.endLine && a.startColumn==b.startColumn && a.endColumn==b.endColumn;}
static int sourceRoot(const XL6_AstNode* node,const XL6_AstNode* at)
{
    SOURCE_CHECK(node && node->kind==XL6_AST_TYPE);
    SOURCE_CHECK(node->line==(at ? at->line : 0) && node->column==(at ? at->column : 0));
    if(XLS_TYPE_SOURCE_EXPECT && at){
        SOURCE_CHECK(node->source==at->source);
        SOURCE_CHECK(sourceRange(node->range,at->range) && sourceRange(node->tokenRange,at->tokenRange));
    }else SOURCE_CHECK(!node->source && !node->range.known && !node->tokenRange.known);
    return 1;
}
static int sourceFactories(XL6_AstNode* at,const char* definitions)
{
    XL6_AstNode *params=xl6AstNew(XL6_AST_LIST,NULL),*returns=xl6AstNew(XL6_AST_RETURN_TYPES,NULL),*param=xl6AstNew(XL6_AST_PARAM,NULL);
    SOURCE_CHECK(params && returns && param);
    xl6AstAddChild(param,xl6AstNewText(XL6_AST_TYPE,"int",3,NULL));param->flags=XL6_AST_FLAG_BYREF;xl6AstAddChild(params,param);
    xl6AstAddChild(returns,xl6AstNewText(XL6_AST_TYPE,"int",3,NULL));xl6AstAddChild(returns,xl6AstNewText(XL6_AST_TYPE,"string",6,NULL));
    SOURCE_CHECK(xl6AstSetSourceTree(params,definitions) && xl6AstSetSourceTree(returns,definitions));
    unsigned paramRefs=params->source->references,returnRefs=returns->source->references,originRefs=at && at->source ? at->source->references : 0;
    const XL6_AstNode* input=param->children[0];XL6_AstNode classNode={.kind=XL6_AST_CLASS,.text="Box",.semanticTypeId=91,.semanticTypeAbiId=73};
    XL6_SemContext context={.currentClass=&classNode};
    for(unsigned factory=0;factory<8;++factory){
        XL6_AstNode* node=NULL;
        switch(factory){
        case 0:node=xl6SemNewNullableType(xl6SemCloneTypeNode(input),at);break;
        case 1:node=xl6SemTupleTypeFromReturns(returns,at);break;
        case 2:node=xl6SemNewGenericUnaryType("future",xl6SemCloneTypeNode(input),at);break;
        case 3:node=xl6SemFunctionTypeFromCallableParts(params,returns,at);break;
        case 4:node=xl6SemNewFutureCallbackType(input,returns->children[0],at);break;
        case 5:node=xl6SemNewGenericContainerType("array",xl6SemCloneTypeNode(input),at);break;
        case 6:node=xl6SemOwnedSimpleType(&context,"int",at);break;
        case 7:node=xl6SemOwnedCurrentClassType(&context,at);break;
        }
        SOURCE_CHECK(sourceRoot(node,at));
        if(factory<6){
            SOURCE_CHECK(node->childCount && node->children[0]->childCount);
            const char* path=xl6AstSourcePath(node->children[0]->children[0]);SOURCE_CHECK(path && !strcmp(path,definitions));
            if(factory==3)SOURCE_CHECK(node->children[0]->children[0]->flags&XL6_AST_FLAG_BYREF);
            xl6AstFree(node);
        }else xl6SemFreeOwnedTypeNodes(&context);
        SOURCE_CHECK(params->source->references==paramRefs && returns->source->references==returnRefs);
        SOURCE_CHECK(!at || !at->source || at->source->references==originRefs);
    }
    xl6SemFreeOwnedTypeNodes(&context);xl6AstFree(params);xl6AstFree(returns);return 1;
}
static int sourceLifetime(const char* path)
{
    XL6_AstNode* at=xl6AstNew(XL6_AST_NAME,NULL);SOURCE_CHECK(at && xl6AstSetSourceTree(at,path));
    at->line=7;at->column=9;at->range=(XL6_AstRange){.known=1,.startOffset=100,.endOffset=105,.startLine=7,.endLine=7,.startColumn=9,.endColumn=14};at->tokenRange=at->range;
    SOURCE_CHECK(sourceFactories(at,"type-origin-definitions.xl"));
    at->source->completionQuery=1;at->source->completionStart=100;at->source->completionEnd=105;
    SOURCE_CHECK(sourceFactories(at,"type-origin-definitions.xl"));
    XL6_AstNode *params=xl6AstNew(XL6_AST_LIST,NULL),*returns=xl6AstNew(XL6_AST_RETURN_TYPES,NULL);SOURCE_CHECK(params && returns);
    XL6_AstNode* held=xl6SemFunctionTypeFromCallableParts(params,returns,at);SOURCE_CHECK(sourceRoot(held,at));
    xl6AstFree(at);xl6AstFree(params);xl6AstFree(returns);
    if(XLS_TYPE_SOURCE_EXPECT){SOURCE_CHECK(held->source && held->source->references==1 && !strcmp(xl6AstSourcePath(held),path));SOURCE_CHECK(held->source->completionQuery && held->source->completionStart==100 && held->range.startOffset==100);}
    xl6AstFree(held);
    XL6_AstNode unknown={.kind=XL6_AST_NAME,.line=7,.column=9};SOURCE_CHECK(sourceFactories(&unknown,"type-origin-definitions.xl"));
    SOURCE_CHECK(sourceFactories(NULL,"type-origin-definitions.xl"));return 1;
}
static int sourceWrite(const char* folder,const char* name,const char* text)
{
    xstrbuf path={0};int okay=text && xrtStrBufAppendFormat(&path,"%s/%s",folder,name) && xrtFileWriteAtomic(path.Data,(xbytesview){(const unsigned char*)text,strlen(text)});
    xrtStrBufFree(&path);SOURCE_CHECK(okay);return 1;
}
static int sourceCorpus(const char* folder,const char* entry,const char* dependency)
{
    const char* bodies[]={
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
        "func accept(func() as int callback) as int\n return callback()\nend\nfunc erased(var boxed) as int\n return accept(boxed)\nend\n",
        "func main(...) as int\n return \"bad\"\nend\n",
        "import type_origin_defs\nfunc main(...) as int\n return type_origin_defs.value()\nend\n"
    };
    const char* definitions="export func value() as int\n return 7\nend\n";
    for(size_t i=0;i<sizeof(bodies)/sizeof(*bodies);++i){
        xls_semantic_input inputs[]={{entry,bodies[i],strlen(bodies[i])},{dependency,definitions,strlen(definitions)}};char error[256];
        xls_semantic_check* result=xlsSemanticCheckCreate(entry,inputs,i==13 ? 2 : 1,1,NULL,NULL,error,sizeof(error));
        if(!result)fprintf(stderr,"type source corpus%zu:%s\n",i,error);
        SOURCE_CHECK(result && xlsSemanticCheckInfo(result).accepted==(i!=12));
        char name[64];snprintf(name,sizeof(name),"corpus-%zu.json",i);char* text=xlsSemanticCheckJson(result,NULL);SOURCE_CHECK(sourceWrite(folder,name,text));xlsSemanticStringFree(text);
        snprintf(name,sizeof(name),"ir-%zu.json",i);
        if(i==12){SOURCE_CHECK(sourceWrite(folder,name,"null"));}
        else{XL6_Output output;SOURCE_CHECK(xl6OutputOpenMemory(&output));xl6IrDumpModuleJson(&output,xlsSemanticCheckIr(result));text=xl6OutputTakeMemoryText(&output,NULL);xl6OutputDiscard(&output);SOURCE_CHECK(sourceWrite(folder,name,text));xrtFree(text);}
        xlsSemanticCheckDestroy(result);
    }
    return 1;
}
int main(int argc,char** argv)
{
    if(argc!=4 || !sourceLifetime("type-origin-中文😀.xl") || !sourceCorpus(argv[1],argv[2],argv[3]))return 1;
    printf("analysis_type_source_fixture passed: %u checks;eight exact constructors,four origin modes,source references/held origins and14 complete semantic/IR corpus results;expect=%d\n",sourceChecks,XLS_TYPE_SOURCE_EXPECT);return 0;
}
