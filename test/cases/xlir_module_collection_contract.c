#define main xl6ExistingArtifactContractMain
#include "test/cases/xlir_artifact_runtime_contract.c"
#undef main
#include <assert.h>
static unsigned corrupt,roots,observations;
static void refuse(XL6_IrModule* ir)
{
    assert(!xl6IrVerifyModule(ir,NULL));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_READ_GRAPH",NULL) : xl6IrCgenEmitC(&out,ir,NULL)));
        char* bytes=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(bytes); xl6OutputDiscard(&out);
    }
    ++corrupt;
}
int main(int argc,char** argv)
{
    assert(argc==4);
    for (int lane=1;lane<4;++lane) {
        assert(fixture_contract(argv[lane])); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[lane],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* ir=&unit.ir;
        for (size_t i=0;i<ir->persistenceRestoreRootCount;++i) {
            XL6_IrPersistenceRestoreRoot* root=&ir->persistenceRestoreRoots[i];
            unsigned saved=root->ownershipGraphProviders;
            assert(saved==(lane==1 ? 7u : 4u));
            root->ownershipGraphProviders=0; refuse(ir); root->ownershipGraphProviders=saved;
            for (unsigned bit=1;bit<=8;bit<<=1) {
                root->ownershipGraphProviders=saved^bit; refuse(ir); root->ownershipGraphProviders=saved;
            }
            ++roots;
        }
        for (size_t i=0;i<ir->nodeCount;++i) {
            XL6_IrNode* loop=&ir->nodes[i];
            if (loop->loopAbi!=XL6_IR_LOOP_ABI_XVALUE_ITERATOR_VALUE &&
                loop->loopAbi!=XL6_IR_LOOP_ABI_XVALUE_ITERATOR_KEY_VALUE) continue;
            XL6_IrNode* source=&ir->nodes[ir->children[loop->firstChild+1]-1];
            if (xl6IrLoopSourceAcquisition(ir,source)!=XL6_IR_LOOP_SOURCE_OBSERVED_COLLECTION) continue;
            assert(loop->loopResourceAbi==XL6_IR_LOOP_RESOURCE_XVALUE_ITERATOR_DESTROY);
            XL6_IrNode saved=*source;
            source->ownership=XL6_IR_OWNERSHIP_NONE; refuse(ir); *source=saved;
            XL6_IrLoopResourceAbi resource=loop->loopResourceAbi;
            loop->loopResourceAbi=XL6_IR_LOOP_RESOURCE_NONE; refuse(ir); loop->loopResourceAbi=resource;
            ++observations;
        }
        xl6CompileUnitFree(&unit);
    }
    assert(roots>=4 && observations>=5 && corrupt>=30);
    printf("Module collection contracts: roots=%u observations=%u corrupt=%u\n",roots,observations,corrupt);
    return 0;
}
