/* Test-only catalog and resolved-call census. Declarations/name occurrences
 * never count as call sites. This does not claim branch execution coverage. */
#include "src/compile/xl6_compile_unit.h"
#include "src/stdlib/xl6_stdlib_data.h"
#include "src/ast/xl6_ast_dump.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void quoted(const char* text)
{
    putchar('"');if(text)for(const unsigned char*p=(const unsigned char*)text;*p;++p){
        if(*p=='"'||*p=='\\'){putchar('\\');putchar(*p);}
        else if(*p<32)printf("\\u%04x",*p);else putchar(*p);
    }putchar('"');
}
static int scoped(const char* ns)
{
    static const char* names[]={"time","env","os.env","signal","process","number","text","string","unicode","html","encoding","pattern","regex","queue","array","list","buffer","map","dict","set","orderedMap","slotMap"};
    if(!ns)return 0;
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)if(!strcmp(ns,names[i]))return 1;
    return 0;
}
static void type_name(const XL6_IrTypePool* pool,XL6_IrTypeId id)
{
    const XL6_IrType*t=xl6IrTypeGet(pool,id);assert(t);
    if(t->kind!=XL6_IR_TYPE_FUNCTION){fputs(t->displayName?t->displayName:"",stdout);return;}
    fputs("func(",stdout);
    for(size_t i=0;i<t->paramCount;++i){if(i)putchar(',');type_name(pool,t->params[i]);}putchar(')');
    if(t->returnCount){fputs(" as ",stdout);if(t->returnCount>1)putchar('(');
        for(size_t i=0;i<t->returnCount;++i){if(i)putchar(',');type_name(pool,t->returns[i]);}
        if(t->returnCount>1)putchar(')');}
}
static int source_calls(const XL6_AstNode* node,XL6_SymbolId symbol,const char* name,
    const XL6_IrType* owner,XL6_IrSpan span)
{
    if(!node)return 0;
    if(node->semanticTargetSymbolId==symbol&&
        (node->kind==XL6_AST_CALL||node->kind==XL6_AST_MEMBER||node->kind==XL6_AST_SAFE_MEMBER))return 1;
    /* Generic lowering assigns an independent specialization symbol. The
     * original, already expanded callee still names the template. Count that
     * source binding only against the concrete IR parameter vector. */
    if(node->kind==XL6_AST_CALL&&node->childCount){const XL6_AstNode*callee=node->children[0];
        if(callee&&callee->kind==XL6_AST_GENERIC_INSTANT&&callee->childCount){const XL6_AstNode*base=callee->children[0];
            if(base&&base->kind==XL6_AST_NAME&&base->text&&name&&!strcmp(base->text,name))return 1;}}
    if(owner&&node->kind==XL6_AST_CALL&&node->childCount&&node->line==span.line){
        const XL6_AstNode*callee=node->children[0];
        if(callee&&callee->kind==XL6_AST_GENERIC_INSTANT&&callee->childCount)callee=callee->children[0];
        int samePosition=node->column==span.column||(callee&&callee->line==span.line&&callee->column==span.column);
        if(!samePosition)goto children;
        if(callee&&callee->kind==XL6_AST_NAME&&!strcmp(name,"create")&&callee->text&&owner->name&&!strcmp(callee->text,owner->name))return 1;
        if(callee&&callee->kind==XL6_AST_MEMBER&&callee->text&&!strcmp(callee->text,name)&&callee->childCount){
            const XL6_AstNode*receiver=callee->children[0];
            const XL6_AstNode*type=receiver?receiver->inferredType:NULL;
            if(type&&((owner->sourceTypeId&&type->semanticTypeId==owner->sourceTypeId)||
                (owner->abiId&&type->semanticTypeAbiId==owner->abiId)))return 1;
        }
    }
children:
    for(size_t i=0;i<node->childCount;++i)if(source_calls(node->children[i],symbol,name,owner,span))return 1;
    return 0;
}
static void catalog(void)
{
    XL6_Output out;xl6OutputInitStdFile(&out,stdout);puts("{\"Registrations\":[");int comma=0;
    for(size_t i=0;i<xl6StdlibFuncCount;++i){const XL6_StdlibFunc*f=&xl6StdlibFuncs[i];if(!scoped(f->ns))continue;
        if(comma++)putchar(',');
        printf("{\"Index\":%zu,\"Namespace\":",i);quoted(f->ns);fputs(",\"Name\":",stdout);quoted(f->name);
        fputs(",\"Internal\":",stdout);quoted(f->internalName);fputs(",\"CSymbol\":",stdout);quoted(f->cSymbol);
        fputs(",\"ParameterTypes\":[",stdout);for(size_t p=0;p<f->paramCount;++p){if(p)putchar(',');quoted(f->params[p].type);}fputs("],\"Ast\":",stdout);
        if(f->sourceBody){XL6_ParseResult parsed;xl6ParseResultInit(&parsed);assert(xl6ParseSource(f->sourceBody,"<catalog>",&parsed));xl6AstDumpJson(&out,parsed.root);xl6ParseResultFree(&parsed);}else fputs("null",stdout);
        putchar('}');
    }puts("]}");
}
#ifdef XL6_COVERAGE_ENUM_NAMES_HEADER
#include XL6_COVERAGE_ENUM_NAMES_HEADER
#include "stdlib_intrinsic_coverage.h"
#endif
int main(int argc,char**argv)
{
    if(argc==3&&!strcmp(argv[1],"--class-catalog")){
        XL6_ParseResult parsed;XL6_Output out;xl6ParseResultInit(&parsed);
        if(!xl6ParseFile(argv[2],&parsed)||!xl6StdlibExpandModule(parsed.root,&parsed.diags)){
            xl6DiagPrintText(&parsed.diags,argv[2]);xl6ParseResultFree(&parsed);return 2;
        }
        xl6OutputInitStdFile(&out,stdout);xl6AstDumpJson(&out,parsed.root);xl6ParseResultFree(&parsed);return 0;
    }
    assert(argc==2);if(!strcmp(argv[1],"--catalog")){catalog();return 0;}
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6ParseFile(argv[1],&unit.parse)){xl6DiagPrintText(&unit.parse.diags,argv[1]);xl6CompileUnitFree(&unit);return 2;}
    size_t originalCount=unit.parse.root->childCount;
#ifdef XL6_COVERAGE_ENUM_NAMES_HEADER
    CoverageIntrinsicSources sources={0};
    for(size_t i=0;i<originalCount;++i)
        coverage_capture_members(&sources,unit.parse.root->children[i],(uintptr_t)unit.parse.root->children[i]);
#endif
    /* Keep pointer identities only. Imports may retire an original node;
     * dereference it only if it remains a current module child. */
    XL6_AstNode**original=xrtMalloc(originalCount*sizeof(*original));assert(original||!originalCount);
    if(originalCount)memcpy(original,unit.parse.root->children,originalCount*sizeof(*original));
    if(!xl6ImportsExpandModule(unit.parse.root,argv[1],&unit.parse.diags,&unit.importDeps)||
       !xl6StdlibExpandModule(unit.parse.root,&unit.parse.diags)||
       !xl6SemCheckModule(unit.parse.root,&unit.parse.diags,"coverage")||
       !xl6IrBuildModuleFromAst(&unit.ir,unit.parse.root,&unit.parse.diags)){
#ifdef XL6_COVERAGE_ENUM_NAMES_HEADER
        coverage_free_sources(&sources);
#endif
        xrtFree(original);
        xl6DiagPrintText(&unit.parse.diags,argv[1]);xl6CompileUnitFree(&unit);return 2;
    }
#ifdef XL6_COVERAGE_ENUM_NAMES_HEADER
    coverage_bind_module(&sources,&unit.ir,unit.parse.root);
#endif
    if(!xl6IrLowerModule(&unit.ir,&unit.parse.diags)){
#ifdef XL6_COVERAGE_ENUM_NAMES_HEADER
        coverage_free_sources(&sources);
#endif
        xrtFree(original);xl6DiagPrintText(&unit.parse.diags,argv[1]);xl6CompileUnitFree(&unit);return 2;
    }
    puts("{\"Calls\":[");int comma=0;
    for(size_t i=0;i<unit.ir.callSiteCount;++i){const XL6_IrCallSite*site=&unit.ir.callSites[i];const XL6_IrCallTarget*t=NULL;
        for(size_t j=0;j<unit.ir.callTargetCount;++j)if(unit.ir.callTargets[j].symbol==site->targetSymbol){t=&unit.ir.callTargets[j];break;}
        if(!t||!t->declaration||t->declaration>unit.ir.nodeCount)continue;
        const XL6_IrNode*n=&unit.ir.nodes[t->declaration-1];
        const char* name=n->text;
        if(n->childCount){XL6_IrNodeId child=unit.ir.children[n->firstChild];assert(child&&child<=unit.ir.nodeCount);
            const XL6_IrNode*first=&unit.ir.nodes[child-1];if(first->sourceKind==XL6_AST_NAME)name=first->text;}
        const XL6_IrType*owner=xl6IrTypeGet(&unit.ir.types,n->declarationOwnerType);
        assert(site->origin&&site->origin<=unit.ir.nodeCount);XL6_IrSpan span=unit.ir.nodes[site->origin-1].span;
        int called=0;
        for(size_t c=0;c<unit.parse.root->childCount&&!called;++c)
            for(size_t p=0;p<originalCount&&!called;++p)
                if(unit.parse.root->children[c]==original[p])called=source_calls(unit.parse.root->children[c],t->symbol,name,owner,span);
        if(comma++)putchar(',');
        printf("{\"Site\":%zu,\"Symbol\":%u,\"Declaration\":%u,\"SourceCall\":%s,\"DeclarationFlags\":%u,\"DeclarationKind\":",i,t->symbol,t->declaration,called?"true":"false",n->flags);quoted(xl6AstKindName(n->sourceKind));
        fputs(",\"Internal\":",stdout);quoted(name);
        fputs(",\"CSymbol\":",stdout);quoted(t->cName);fputs(",\"Owner\":",stdout);
        quoted(owner?owner->displayName:NULL);
        fputs(",\"ParameterTypes\":[",stdout);for(size_t p=0;p<t->parameterCount;++p){if(p)putchar(',');putchar('"');type_name(&unit.ir.types,t->parameterTypes[p]);putchar('"');}fputs("]}",stdout);
    }putchar(']');
#ifdef XL6_COVERAGE_ENUM_NAMES_HEADER
    coverage_dump_intrinsics(&sources,&unit.ir);coverage_dump_properties(&sources,&unit.ir);coverage_free_sources(&sources);
#endif
    puts("}");xrtFree(original);xl6CompileUnitFree(&unit);return 0;
}
