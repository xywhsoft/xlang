#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* m,const char* label)
{
    if(xl6IrVerifyModule(m,NULL)){fprintf(stderr,"unexpected valid NULL-compatible IR: %s\n",label);assert(0);}
    for(unsigned header=0;header<2;++header){XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,m,"BAD_NULL_COMPAT",NULL):xl6IrCgenEmitC(&out,m,NULL)));
        char* bytes=xl6OutputTakeMemoryText(&out,&size);assert(bytes && !size);xrtFree(bytes);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* m=&unit.ir;assert(xl6IrVerifyModule(m,NULL));assert(m->foreignFixedInputCount==5);
    for(size_t i=0;i<m->foreignFixedInputCount;++i){XL6_ForeignFixedInputContract* row=&m->foreignFixedInputs[i];XL6_ForeignFixedInputContract saved=*row;
        strcpy(row->pointerCName,"const OtherPod*");refused(m,"unrelated const pointee");*row=saved;
        strcpy(row->pointerCName,"NullPod*");refused(m,"mutable pointer instead of const input");*row=saved;
        row->physicalIndex=63;refused(m,"changed physical alignment");*row=saved;
        row->kind=XL6_FOREIGN_FIXED_MODULE_CODE_OWNER;strcpy(row->pointerCName,"const XL6_CodeOwner*");refused(m,"code owner is not NULL substitution");*row=saved;
        XL6_IrNode* node=&m->nodes[row->declaration-1];uint64 flags=node->flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_PURE;refused(m,"different producer effect");node->flags=flags;
        node->flags|=XL6_AST_FLAG_FOREIGN_NONBLOCKING;refused(m,"different native blocking contract");node->flags=flags;
        uint16 mappings=node->foreignCall.inputMappingFacts;node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_FIXED_PRESENT;refused(m,"lost fixed input presence");node->foreignCall.inputMappingFacts=mappings;
    }
    for(size_t i=0;i<m->foreignPackFieldCount;++i){XL6_ForeignPackFieldContract* row=&m->foreignPackFields[i];XL6_ForeignPackFieldContract saved=*row;
        row->parameter=63;refused(m,"pack column outside explicit input group");*row=saved;
        strcpy(row->field,"source->raw");refused(m,"pack path injection");*row=saved;
        if(row->kind==XL6_FOREIGN_PACK_FIELD_RECORD){
            strcpy(row->physicalCName,"OtherInner");refused(m,"unchanged nested record CType differs");*row=saved;
            row->recordParentIndexPlusOne=1;refused(m,"top record has unrelated parent");*row=saved;
            for(size_t k=0;k<m->foreignPackFieldCount;++k){XL6_ForeignPackFieldContract* child=&m->foreignPackFields[k];
                if(child->recordParentIndexPlusOne!=i+1)continue;
                uint32 parent=child->recordParentIndexPlusOne;child->recordParentIndexPlusOne=0;refused(m,"nested field loses exact parent");child->recordParentIndexPlusOne=parent;
                child->recordParentIndexPlusOne=(uint32)m->foreignPackFieldCount;refused(m,"nested field uses other record directory index");child->recordParentIndexPlusOne=parent;
            }
            XL6_ForeignPackFieldContract* all=xrtMalloc(m->foreignPackFieldCount*sizeof(*all));assert(all);memcpy(all,m->foreignPackFields,m->foreignPackFieldCount*sizeof(*all));
            for(size_t k=0;k<m->foreignPackFieldCount;++k){XL6_ForeignPackFieldContract* part=&m->foreignPackFields[k];
                if(part->declaration!=row->declaration || part->packPhysicalIndex!=row->packPhysicalIndex)continue;
                if(!strcmp(part->field,"inner"))strcpy(part->field,"other");else if(!strncmp(part->field,"inner.",6))memcpy(part->field,"other.",6);
            }
            refused(m,"independently valid second pack nested path differs");memcpy(m->foreignPackFields,all,m->foreignPackFieldCount*sizeof(*all));xrtFree(all);
        }
    }
    for(size_t i=0;i<m->foreignPackCount;++i){XL6_ForeignPackContract* row=&m->foreignPacks[i];XL6_ForeignPackContract saved=*row;
        strcpy(row->recordCName,"OtherDesc");refused(m,"pack record versus NULL pointee");*row=saved;
        strcpy(row->header,"other.h");refused(m,"pack authority differs");*row=saved;
    }
    for(size_t i=0;i<m->callTargetCount;++i){XL6_IrCallTarget* t=&m->callTargets[i];if(t->kind!=XL6_IR_CALL_TARGET_EXTERN_C || !t->cName ||
          (strcmp(t->cName,"null_pod") && strcmp(t->cName,"null_free") && strcmp(t->cName,"null_create_config") && strcmp(t->cName,"apply") && strcmp(t->cName,"paired")))continue;
        XL6_IrCallErrorPhysicalAbi ep=t->errorPhysicalAbi;t->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refused(m,"lost checked adapter errors");t->errorPhysicalAbi=ep;
        int32 success=t->nativeSuccessCode;t->nativeSuccessCode=7;refused(m,"status success disagreement");t->nativeSuccessCode=success;
        for(size_t p=0;p<t->parameterCount;++p){XL6_IrCallParameterConstraintAbi c=t->parameterConstraintAbis[p];t->parameterConstraintAbis[p]=(XL6_IrCallParameterConstraintAbi)UINT32_MAX;refused(m,"unknown constraint in unaffected input");t->parameterConstraintAbis[p]=c;}
    }
    assert(xl6IrVerifyModule(m,NULL));printf("NULL physical compatibility: %u mutations rejected with zero C/header output\n",mutations);
    xl6CompileUnitFree(&unit);return 0;
}
