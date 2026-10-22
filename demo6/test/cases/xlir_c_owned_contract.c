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
        xl6IrBuildModuleFromAst(&ir,parsed.root,&parsed.diags) && xl6IrLowerModule(&ir,&parsed.diags);
    if(ok && strcmp(argv[2],"check")) {
        ok=xl6OutputOpenFile(&out,argv[2]);
        if(ok) {
            ok=xl6IrCgenEmitC(&out,&ir,&parsed.diags);
            if(ok) xl6OutputPrintf(&out,"\nint main(void) { return xlir_module_init() ? 0 : 3; }\n");
            xl6OutputClose(&out);
        }
    }
    if(!ok) xl6DiagPrintText(&parsed.diags,argv[1]);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parsed);
    return ok ? 0 : 1;
}
