#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* m,const char* label)
{
    if(xl6IrVerifyModule(m,NULL)){fprintf(stderr,"unexpected valid nullable-span IR: %s\n",label);assert(0);}
    for(unsigned h=0;h<2;++h){XL6_Output out;size_t n=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(h ? xl6IrCgenEmitHeader(&out,m,"BAD_SPAN",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&n);assert(text && !n);xrtFree(text);xl6OutputDiscard(&out);}
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit u;xl6CompileUnitInit(&u);assert(xl6CompileUnitLoadFile(&u,argv[1],1));assert(xl6IrLowerModule(&u.ir,&u.parse.diags));XL6_IrModule* m=&u.ir;assert(xl6IrVerifyModule(m,NULL));
    XL6_IrTypeId stringType=0,bytesType=0;for(size_t i=0;i<m->types.count;++i){const XL6_IrType* t=&m->types.items[i];if(t->kind==XL6_IR_TYPE_BUILTIN && t->cAbi==XL6_IR_C_ABI_MANAGED_STRING)stringType=(XL6_IrTypeId)i+1;if(t->kind==XL6_IR_TYPE_BUILTIN && t->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)bytesType=(XL6_IrTypeId)i+1;}
    assert(stringType && bytesType);
    for(size_t i=0;i<m->foreignParameterCount;++i){XL6_ForeignParameterContract* row=&m->foreignParameters[i];if(row->kind!=XL6_FOREIGN_PARAMETER_SPAN)continue;XL6_ForeignParameterContract saved=*row;
        row->countAbi=0;refused(m,"span count ABI missing");*row=saved;
        strcpy(row->elementCName,"char");refused(m,"pointer and element type disagree");*row=saved;
        row->parameter=63;refused(m,"span logical input outside signature");*row=saved;
        const XL6_IrCallTarget* t=xl6IrCallTargetGet(m,m->nodes[row->declaration-1].symbol);assert(t);size_t p=row->parameter+1;
        const XL6_IrType* type=xl6IrTypeGet(&m->types,t->parameterTypes[p]);assert(type);
        if(type->cAbi==XL6_IR_C_ABI_MANAGED_STRING || type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES){row->readOnly=0;strcpy(row->pointerCName,"void*");refused(m,"immutable source becomes writable span");*row=saved;}
        if(!strcmp(t->cName,"nullable_span_read") || !strcmp(t->cName,"nullable_span_pack")){
            row->countAbi=XL6_FOREIGN_SPAN_COUNT_INT;refused(m,"same Native count type differs");*row=saved;
            strcpy(row->header,"other.h");refused(m,"same Native span header differs");*row=saved;
            strcpy(row->pointerCName,"const char*");strcpy(row->elementCName,"char");refused(m,"same Native readonly pointer type differs");*row=saved;
        }
    }
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* t=&m->callTargets[i];for(size_t p=0;p<t->parameterCount;++p){if(t->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_SPAN_BORROW)continue;
        XL6_IrTypeId original=t->parameterTypes[p];const XL6_IrType* type=xl6IrTypeGet(&m->types,original);assert(type);
        if(type->kind==XL6_IR_TYPE_NULLABLE){
            t->parameterTypes[p]=bytesType;refused(m,"nullable Buffer scope reinterpreted as bytes");t->parameterTypes[p]=original;
            t->parameterTypes[p]=stringType;refused(m,"nullable Buffer scope reinterpreted as string");t->parameterTypes[p]=original;
            XL6_IrNodeId field=t->parameterPhysicalFieldDeclarations[p];t->parameterPhysicalFieldDeclarations[p]=0;refused(m,"nullable Buffer loses protected native field");t->parameterPhysicalFieldDeclarations[p]=field;
        }
        XL6_IrParameterAbi abi=t->parameterAbis[p];t->parameterAbis[p]=XL6_IR_PARAMETER_ABI_OWNED;refused(m,"span borrows do not transfer source owner");t->parameterAbis[p]=abi;
    }}
    assert(xl6IrVerifyModule(m,NULL));printf("nullable/read-only spans: %u mutations rejected with zero C/header output\n",mutations);xl6CompileUnitFree(&u);return 0;
}
