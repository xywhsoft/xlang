#include "src/runtime/xl6_array_entry.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_import.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static xvalue* failed(xvalue* owner)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBeginCleanup(&scope,true)) abort();
    xrtValueRelease(owner);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    xl6ErrorScopePropagateFailure(outcome);
    return NULL;
}
xvalue* xl6ArrayEntryAcquireV1(uint64_t identity,uint64_t element,const xvalue* input)
{
    if(!identity || !element) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if(input) {
        XL6_Sequence* sequence=NULL; XL6_SequenceLayoutV1 layout;
        const xvalueconverter* converter=NULL;
        int status=xl6SequenceBeginErasedArrayV1(input,&sequence,&layout,&converter);
        if(status<0) return NULL;
        if(status) {
            bool exact=layout.identity==identity && layout.elementIdentity==element &&
                layout.transfer==3 && layout.size==sizeof(xvalue*) && layout.type==xrtTypeValue() &&
                converter==xl6ArrayReferenceConverterV1() && xl6SequenceReferenceAnyPolicyV1(sequence,element);
            xvalue* retained=exact ? xrtValueRetain(input) : NULL;
            xl6SequenceEndV1(sequence);
            if(exact) return retained;
        }
    }
    xvalue* owner=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,identity,
        element,XL6_VALUE_ADMIT_ANY,false,element);
    if(!owner) return NULL;
    owner=xl6ArrayProjectionTakeV1(owner,identity,element,3,sizeof(xvalue*),xrtTypeValue(),NULL);
    if(!owner) return NULL;
    return input ? xl6ArrayImportTakeV1(owner,input) : owner;
}
xvalue* xl6ArrayArgvCreateV1(uint64_t identity,uint64_t element,int count,char* const* values)
{
    if(count<0 || (count && !values)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    for(int i=0;i<count;++i)
        if(!values[i]) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    xvalue* owner=xl6ArrayEntryAcquireV1(identity,element,NULL);
    if(!owner) return NULL;
    if(!xl6ArrayReserveV1(owner,identity,element,3,sizeof(xvalue*),xrtTypeValue(),count))
        return failed(owner);
    for(int i=0;i<count;++i) {
        xvalue* item=xrtValueString(xrtStrView(values[i]));
        if(!item || !xl6ArrayValueAppendTakeV1(owner,item)) return failed(owner);
    }
    return owner;
}
