#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <stdio.h>
#include <string.h>

int main(int argc,char** argv)
{
    XL6_ParseResult parsed; XL6_IrModule ir; XL6_Output out;
    if(argc!=3) return 2;
    xl6ParseResultInit(&parsed); xl6IrModuleInit(&ir);
    int ok=xl6ParseFile(argv[1],&parsed) && xl6StdlibExpandModule(parsed.root,&parsed.diags) &&
        xl6SemCheckModule(parsed.root,&parsed.diags,"c_owned") &&
        xl6IrBuildModuleFromAst(&ir,parsed.root,&parsed.diags);
    if(ok) {
        ok=xl6IrLowerModule(&ir,&parsed.diags);
        if(strcmp(argv[2],"check")) {
            char path[4096];snprintf(path,sizeof(path),"%s.json",argv[2]);
            if(xl6OutputOpenFile(&out,path)){xl6IrDumpModuleJson(&out,&ir);xl6OutputClose(&out);}
        }
    }
    if(ok && strcmp(argv[2],"check")) {
        ok=xl6OutputOpenFile(&out,argv[2]);
        if(ok) {
            ok=xl6IrCgenEmitC(&out,&ir,&parsed.diags);
            if(ok) xl6OutputPrintf(&out,"\nint main(void) { return xlir_module_init() ? 0 : 3; }\n");
            xl6OutputClose(&out);
        }
    }
    if(!ok) {
        xl6DiagPrintText(&parsed.diags,argv[1]);
        for(size_t f=0;f<ir.functionCount;++f) {
            const XL6_IrFunction* fn=&ir.functions[f];
            fprintf(stderr,"function %u blocks %zu..%zu\n",fn->declaration,fn->firstBlock+1,fn->firstBlock+fn->blockCount);
            for(size_t b=fn->firstBlock;b<fn->firstBlock+fn->blockCount;++b) {
                const XL6_IrBlock* block=&ir.blocks[b];
                const XL6_IrInstruction* last=block->instructionCount ? &ir.instructions[block->firstInstruction+block->instructionCount-1] : NULL;
                fprintf(stderr," block %zu count %zu last %d origin %u edges %u/%u\n",b+1,block->instructionCount,
                    last ? (int)last->kind : -1,last ? last->origin : 0,last ? last->trueTarget : 0,last ? last->falseTarget : 0);
            }
        }
    }
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parsed);
    return ok ? 0 : 1;
}
