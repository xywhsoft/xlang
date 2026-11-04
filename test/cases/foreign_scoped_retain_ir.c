#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* ir,const char* label)
{
    if(xl6IrVerifyModule(ir,NULL)){fprintf(stderr,"invalid scoped-retain IR accepted: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t bytes=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_SCOPED_RETAIN",NULL):xl6IrCgenEmitC(&out,ir,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&bytes);assert(text && !bytes);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2 && sizeof(XL6_ForeignCallFacts)==88);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* ir=&unit.ir;assert(xl6IrVerifyModule(ir,NULL));unsigned scalar=0,outputs=0,scoped=0;
    for(size_t i=0;i<ir->foreignRetainCount;++i){
        XL6_ForeignRetainContract* row=&ir->foreignRetains[i],saved=*row;++scalar;
        row->retainSymbol=0;refused(ir,"scalar missing hold");*row=saved;
        row->rollbackSymbol=saved.retainSymbol;refused(ir,"scalar wrong rollback");*row=saved;
        row->success=0;refused(ir,"scalar success");*row=saved;
        XL6_IrNode* node=&ir->nodes[row->declaration-1];uint64 flags=node->flags;
        node->flags&=~XL6_AST_FLAG_FOREIGN_NO_RAISE;refused(ir,"scalar effect");node->flags=flags;
    }
    for(size_t i=0;i<ir->foreignOutputViewCount;++i){
        XL6_ForeignOutputViewContract* row=&ir->foreignOutputViews[i];if(row->conversion!=XL6_FOREIGN_OUTPUT_CONVERSION_RETAINED_NATIVE)continue;
        XL6_ForeignOutputViewContract saved=*row;++outputs;
        row->retainSymbol=0;refused(ir,"output missing hold");*row=saved;
        row->physicalIndex=63;refused(ir,"output slot");*row=saved;
        row->logicalType=0;refused(ir,"output type");*row=saved;
        row->conversion=XL6_FOREIGN_OUTPUT_CONVERSION_TEXT;refused(ir,"output borrowed conversion");*row=saved;
        XL6_IrNode* node=&ir->nodes[row->declaration-1];uint64 flags=node->flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(ir,"output effect");node->flags=flags;
    }
    for(XL6_IrTypeId id=1;id<=ir->types.count;++id){
        XL6_IrType* type=(XL6_IrType*)xl6IrTypeGet(&ir->types,id);
        if(!type || type->kind!=XL6_IR_TYPE_CLASS || !type->cName[0] || strcmp(type->cName,"ScopedRetainNode*"))continue;
        assert(type->foreignScope && !strcmp(type->foreignScope,"ScopedRetainScope") && !type->foreignWatch);++scoped;
        char* scope=type->foreignScope;type->foreignScope="Scope;injection";refused(ir,"scope identifier");type->foreignScope=scope;
        char* watch=type->foreignWatch;type->foreignWatch="ScopedRetainWatch";refused(ir,"retained watch still rejected");type->foreignWatch=watch;
        XL6_ForeignLifetimeAbi lifetime=type->foreignLifetime;type->foreignLifetime=XL6_FOREIGN_LIFETIME_CALLER;refused(ir,"owned root required");type->foreignLifetime=lifetime;
    }
    assert(scalar>=2 && outputs>=3 && scoped && xl6IrVerifyModule(ir,NULL));
    printf("scoped retained roots: %u scalar/%u outputs, %u mutations refused with zero C/header\n",scalar,outputs,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
