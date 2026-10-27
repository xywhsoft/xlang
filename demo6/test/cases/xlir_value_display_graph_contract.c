#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void refuse(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_DISPLAY",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(text); xl6OutputDiscard(&out);
    }
}
int main(int argc,char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t mutations=0,covered=0;
    const struct { const char* name; unsigned providers; } expected[]={
        {"displayArray",0},{"displayDict",4},{"displayList",1},
        {"displaySet",2},{"displayOpen",7},{"displayInteger",0},{"displayClass",4}};
    assert(argc==2); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,"display_contract") ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) ||
        !xl6IrLowerModule(&ir,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags)) {
        for(size_t i=0;i<parse.diags.count;++i)puts(parse.diags.items[i].message);
        return 1;
    }
    for(size_t e=0;e<sizeof(expected)/sizeof(*expected);++e) {
        size_t found=0;
        for(size_t f=0;f<ir.functionCount;++f) {
            XL6_IrFunction* fn=&ir.functions[f];
            if(!fn->debugName || strcmp(fn->debugName,expected[e].name))continue;
            for(size_t i=fn->firstInstruction;i<fn->firstInstruction+fn->instructionCount;++i) {
                XL6_IrInstruction* ins=&ir.instructions[i]; unsigned recipe=0;
                if(!xl6IrValueDisplayGraphRecipe(&ir,ins,&recipe))continue;
                assert(recipe==expected[e].providers && ins->displayGraphProviders==recipe);
                for(unsigned bit=1;bit<=8;bit<<=1) {
                    ins->displayGraphProviders=recipe^bit;refuse(&ir);
                    ins->displayGraphProviders=recipe;++mutations;
                }
                ++found;
            }
        }
        printf("display domain %s: %zu operations\n",expected[e].name,found);
        assert(found==1);++covered;
    }
    assert(xl6IrVerifyModule(&ir,NULL));
    printf("value display graph: %zu independent provider domains, %zu rejected metadata mutations\n",covered,mutations);
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
