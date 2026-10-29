/* Compiler-private frame leaf. No xvalue kind or language type registration. */
#include "xl6_c_owned_capture.h"
#include <stdlib.h>
bool xl6COwnedCopyCapturedV1(XL6_COwnedV1 input,XL6_COwnedV1* output)
{
    *output=xl6COwnedCopyV1(input);
    return !input.Data || output->Data!=NULL;
}
void xl6COwnedDropCapturedV1(XL6_COwnedV1 value) { xl6COwnedDropValueV1(value); }
static bool init(ptr output,const xrttype* type)
{ (void)type;*(XL6_COwnedV1*)output=(XL6_COwnedV1){0};return true; }
static bool copy(ptr output,const void* input,const xrttype* type)
{
    (void)type;XL6_COwnedV1 owned={0};
    if(!xl6COwnedCopyCapturedV1(*(const XL6_COwnedV1*)input,&owned))return false;
    XL6_COwnedV1 prior=*(XL6_COwnedV1*)output;*(XL6_COwnedV1*)output=owned;
    xl6COwnedDropValueV1(prior);return true;
}
static void drop(ptr output,const xrttype* type)
{ (void)type;xl6COwnedDropV1(output); }
static void detach(void* raw,unsigned char* live)
{
    XL6_COwnedV1* slot=raw;XL6_COwnedV1 owned;
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    owned=*slot;*slot=(XL6_COwnedV1){0};if(live)*live=0;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6COwnedDropValueV1(owned);
}
static const xrttype* storageType(void)
{
    static const xrttypeops ops={.Init=init,.Copy=copy,.Drop=drop,.Clone=copy};
    static const xrttype type={.Id=UINT64_C(0xb40b3e14b1a917be),.Kind=XRT_TYPE_RECORD,
        .Flags=XRT_TYPE_FLAG_COPYABLE|XRT_TYPE_FLAG_FINAL|XRT_TYPE_FLAG_RELOCATABLE,
        .Name=XRT_STR_INIT("compiler C owner"),.AbiName=XRT_STR_INIT("xlang.compiler.c-owner.v1"),
        .Size=sizeof(XL6_COwnedV1),.Align=_Alignof(XL6_COwnedV1),.InstanceAlign=1,.Ops=&ops};
    return &type;
}
const XL6_NativeCaptureProfileV1* xl6COwnedCaptureProfileV1Get(void)
{
    static const XL6_NativeCaptureProfileV1 profile={sizeof(profile),UINT64_C(0xb40b3e14b1a917be),storageType,detach};
    return &profile;
}
