#include "src/cgen/xl6_ir_cgen.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/compile/xl6_artifact_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_callable_bridge_mutations.h"
static unsigned mutations;
static void reject(const XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; assert(!xl6IrVerifyModule(ir,&diags));
    for (unsigned header=0;header<2;++header) {
        XL6_Output out; size_t size=SIZE_MAX; assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,ir,"BAD_SCALAR",&diags) : xl6IrCgenEmitC(&out,ir,&diags)));
        char* text=xl6OutputTakeMemoryText(&out,&size); assert(size==0); xrtFree(text);
    }
    xl6DiagFree(&diags); ++mutations;
}
static XL6_NativeScalarProfileV1 expectedNative(XL6_IrCAbi abi)
{
    switch (abi) {
    case XL6_IR_C_ABI_C_CHAR: return XL6_NATIVE_SCALAR_CHAR;
    case XL6_IR_C_ABI_C_SCHAR: return XL6_NATIVE_SCALAR_SCHAR;
    case XL6_IR_C_ABI_C_UCHAR: return XL6_NATIVE_SCALAR_UCHAR;
    case XL6_IR_C_ABI_C_SHORT: return XL6_NATIVE_SCALAR_SHORT;
    case XL6_IR_C_ABI_C_USHORT: return XL6_NATIVE_SCALAR_USHORT;
    case XL6_IR_C_ABI_C_INT: return XL6_NATIVE_SCALAR_INT;
    case XL6_IR_C_ABI_C_UINT: return XL6_NATIVE_SCALAR_UINT;
    case XL6_IR_C_ABI_C_LONG: return XL6_NATIVE_SCALAR_LONG;
    case XL6_IR_C_ABI_C_ULONG: return XL6_NATIVE_SCALAR_ULONG;
    case XL6_IR_C_ABI_C_LONGLONG: return XL6_NATIVE_SCALAR_LONGLONG;
    case XL6_IR_C_ABI_C_ULONGLONG: return XL6_NATIVE_SCALAR_ULONGLONG;
    case XL6_IR_C_ABI_C_SIZE: return XL6_NATIVE_SCALAR_SIZE;
    case XL6_IR_C_ABI_C_SSIZE: return XL6_NATIVE_SCALAR_SSIZE;
    case XL6_IR_C_ABI_C_PTRDIFF: return XL6_NATIVE_SCALAR_PTRDIFF;
    case XL6_IR_C_ABI_C_INTPTR: return XL6_NATIVE_SCALAR_INTPTR;
    case XL6_IR_C_ABI_C_UINTPTR: return XL6_NATIVE_SCALAR_UINTPTR;
    case XL6_IR_C_ABI_C_BOOL: return XL6_NATIVE_SCALAR_BOOL;
    case XL6_IR_C_ABI_C_FLOAT: return XL6_NATIVE_SCALAR_FLOAT;
    case XL6_IR_C_ABI_C_DOUBLE: return XL6_NATIVE_SCALAR_DOUBLE;
    case XL6_IR_C_ABI_C_WCHAR: return XL6_NATIVE_SCALAR_WCHAR;
    case XL6_IR_C_ABI_TYPE_ID: return XL6_NATIVE_SCALAR_TYPE_ID;
    default: return XL6_NATIVE_SCALAR_NONE;
    }
}
int main(int argc,char** argv)
{
    assert(argc==5); XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    if (!xl6ParseFile(argv[1],&parse) || !xl6StdlibExpandModule(parse.root,&parse.diags) ||
        !xl6SemCheckModule(parse.root,&parse.diags,argv[1]) ||
        !xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags) ||
        !xl6IrLowerModule(&ir,&parse.diags) || !xl6IrVerifyModule(&ir,&parse.diags)) goto failed;
    XL6_ArtifactRuntimePlan plan;
    assert(xl6ArtifactRuntimePlanBuild(&ir,XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,&plan,&parse.diags));
    bool unused=strstr(argv[1],"_unused")!=NULL;
    bool integers=strstr(argv[1],"_integer_domains")!=NULL;
    bool native=strstr(argv[1],"_native_scalars")!=NULL;
    assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_NATIVE_SCALAR_VALUE)==native);
    if (!native) assert(xl6ArtifactRuntimeSetHas(&plan.components,XL6_ARTIFACT_VALUE_ADMISSION)==!unused);
    xlirCallableBridgeMutations(&ir,reject);
    unsigned f32=0,time=0,bindings=0,integer=0,nominal=0,nativeSlots=0; uint32_t profiles=0;
    for (size_t i=0;i<ir.callableBridgeCount;++i) {
        const XL6_IrCallableBridge* b=&ir.callableBridges[i];
        for (size_t p=0;p<b->parameterCount+b->resultCount;++p) {
            XL6_IrCallableBridgeValueAbi decode,encode; XL6_IrTypeId id;
            XL6_IrCallableBridgeIntegerDomain domain;
            XL6_NativeScalarProfileV1 profile;
            if (p<b->parameterCount) {
                const XL6_IrCallableBridgeParameter* s=&ir.callableBridgeParameters[b->firstParameter+p];
                decode=s->decodeAbi; encode=s->encodeAbi; id=s->type;
                domain=s->integerDomain;
                profile=s->nativeScalarProfile;
                assert(s->decodedDestroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NONE || decode!=XL6_IR_CALLABLE_BRIDGE_F32);
            } else {
                const XL6_IrCallableBridgeResult* s=&ir.callableBridgeResults[b->firstResult+p-b->parameterCount];
                decode=s->decodeAbi; encode=s->encodeAbi; id=s->type;
                domain=s->integerDomain;
                profile=s->nativeScalarProfile;
            }
            const XL6_IrType* t=xl6IrTypeGet(&ir.types,id); assert(t);
            assert(profile==expectedNative(t->cAbi));
            if (profile) {
                assert(decode==XL6_IR_CALLABLE_BRIDGE_NATIVE_SCALAR && encode==decode && !domain.rule && t->typeValueId);
                ++nativeSlots; profiles|=UINT32_C(1)<<profile;
            }
            if (t->cAbi==XL6_IR_C_ABI_F32) { assert(decode==XL6_IR_CALLABLE_BRIDGE_F32 && encode==decode); ++f32; }
            if (t->cAbi==XL6_IR_C_ABI_TIME_I64) { assert(decode==XL6_IR_CALLABLE_BRIDGE_TIME && encode==decode); ++time; }
            if (domain.rule!=XL6_VALUE_ADMIT_NONE) {
                assert(domain.rule>=XL6_VALUE_ADMIT_I8 && domain.rule<=XL6_VALUE_ADMIT_U64 &&
                    domain.identity==t->typeValueId && domain.nominal==(t->kind==XL6_IR_TYPE_NUMERIC_ENUM));
                assert(decode==(domain.rule==XL6_VALUE_ADMIT_U64 ? XL6_IR_CALLABLE_BRIDGE_U64 : XL6_IR_CALLABLE_BRIDGE_I64));
                assert(encode==decode); ++integer; nominal+=domain.nominal;
            }
        }
    }
    assert(native ? nativeSlots>=44 && profiles==((UINT32_C(1)<<22)-2) : integers ? integer>=32 && nominal>=16 : f32 && (unused || time));
    assert(xl6OutputOpenFile(&out,argv[2])); assert(xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3]));
    for (size_t i=0;i<ir.storageCount;++i) {
        const XL6_IrStorage* s=&ir.storages[i];
        const XL6_IrNode* n=xl6IrNodeGet(&ir,s->declaration); const char* name=NULL;
        if (s->kind!=XL6_IR_STORAGE_GLOBAL || !n || !n->text) continue;
        if (!strcmp(n->text,"scalarNativeF32")) name="F32";
        else if (!strcmp(n->text,"scalarNativeTime")) name="TIME";
        else if (!strcmp(n->text,"scalarNativeMixed")) name="MIXED";
        else if (integers && !strncmp(n->text,"domainNative",12)) name=n->text+12;
        else if (native && !strncmp(n->text,"nativeSlot",10)) name=n->text+10;
        if (!name) continue;
        const XL6_IrCallableBridge* b=xl6IrCallableBridgeGet(&ir,s->type); assert(b && b->reverseSupported);
        if (integers) {
            const XL6_IrCallableBridgeParameter* p=&ir.callableBridgeParameters[b->firstParameter];
            const XL6_IrCallableBridgeResult* r=&ir.callableBridgeResults[b->firstResult];
            assert(p->integerDomain.rule==r->integerDomain.rule && p->integerDomain.identity==r->integerDomain.identity);
            xl6OutputPrintf(&out,"#define SCALAR_%s_IDENTITY UINT64_C(0x%016llX)\n",name,(unsigned long long)p->integerDomain.identity);
        }
        if (native) {
            const XL6_IrCallableBridgeParameter* p=&ir.callableBridgeParameters[b->firstParameter];
            const XL6_IrType* t=xl6IrTypeGet(&ir.types,p->type); assert(t && p->nativeScalarProfile);
            xl6OutputPrintf(&out,"#define SCALAR_%s_IDENTITY UINT64_C(0x%016llX)\n#define SCALAR_%s_PROFILE %u\n",
                name,(unsigned long long)t->typeValueId,name,(unsigned)p->nativeScalarProfile);
            if (strcmp(name,"MIXED")) {
                const XL6_IrType* array=NULL;
                const XL6_IrType* signature=xl6IrTypeGet(&ir.types,s->type); assert(signature);
                for (size_t j=1;j<=ir.types.count;++j) {
                    const XL6_IrType* candidate=xl6IrTypeGet(&ir.types,(XL6_IrTypeId)j);
                    if (candidate && candidate->collectionAbi==XL6_IR_COLLECTION_ARRAY && candidate->paramCount==1 && candidate->params[0]==s->type)
                        array=candidate;
                }
                assert(array && array->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_CALLABLE_REFERENCE_V1 &&
                    array->sequenceLayout.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 &&
                    array->sequenceLayout.baseElementType==s->type && !array->sequenceLayout.nullable);
                xl6OutputPrintf(&out,"#define SCALAR_%s_SIGNATURE UINT64_C(0x%016llX)\n#define SCALAR_%s_ARRAY UINT64_C(0x%016llX)\n",
                    name,(unsigned long long)signature->typeValueId,name,(unsigned long long)array->typeValueId);
            }
            if (!strcmp(name,"MIXED")) {
                const XL6_IrType* code=xl6IrTypeGet(&ir.types,ir.callableBridgeParameters[b->firstParameter+1].type);
                assert(code); xl6OutputPrintf(&out,"#define SCALAR_MIXED_TYPE_IDENTITY UINT64_C(0x%016llX)\n",(unsigned long long)code->typeValueId);
            }
        }
        xl6OutputPrintf(&out,"#define SCALAR_%s_NATIVE s%u\n#define SCALAR_%s_FORWARD xlir_xcall_entry_t%u\n"
            "#define SCALAR_%s_REVERSE xlir_xvalue_closure_entry_t%u\n",name,(unsigned)s->symbol,
            name,(unsigned)s->type,name,(unsigned)s->type);
        if (!strcmp(name,"MIXED")) xl6OutputPrintf(&out,"#define SCALAR_MIXED_RESULT xlir_product_t%u\n"
            "#define SCALAR_MIXED_DROP xlir_product_drop_t%u\n",(unsigned)b->resultType,(unsigned)b->resultType);
        ++bindings;
    }
    assert(unused ? !bindings : bindings==(native ? 22u : integers ? 17u : 3u)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[4])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
    printf("scalar bridge: f32=%u time=%u mutations=%u resident-reader=%d integers=%u nominal=%u native=%u profiles=%u\n",f32,time,mutations,!unused,integer,nominal,nativeSlots,profiles);
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
failed:
    for (size_t i=0;i<parse.diags.count;++i) fprintf(stderr,"%s\n",parse.diags.items[i].message);
    return 1;
}
