#include "src/runtime/xl6_value_format.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/runtime/xl6_list_graph.h"
#include "src/runtime/xl6_set_graph.h"
#include "src/runtime/xl6_dict_graph.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

bool xl6_xrt_print(xvalue* values, bool newline)
{
    /* This exported erased-pack entry is genuinely open. Ordinary direct
     * language calls carry their narrower IR plan through CPrintNative. */
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1(),xl6SetGraphProviderV1(),xl6DictGraphProviderV1()};
    const XL6_ValueGraphPlanV1 graph={sizeof(graph),providers,3,NULL,0};
    xstrbuf output;
    bool ready = true;
    xl6_error_scope operation={0};
    if (!xl6ErrorScopeBegin(&operation)) abort();
    xrtStrBufInit(&output);
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(values,&owner,&layout,&converter);
    if (status<0) ready=false;
    else if (owner || (values && xrtValueType(values) == XVALUE_ARRAY)) {
        xtypedarray* storage=owner ? xl6SequenceStorageV1(owner) : NULL;
        size_t count=owner ? xrtTypedArrayCount(storage) : xrtValueCount(values);
        for (size_t i = 0; ready && i < count; ++i) {
            if (i) ready = xrtStrBufAppend(&output, xrtStrView(" "));
            if (!ready) break;
            /* ANY/reference slots are borrowed only under the operation pin.
             * Scalar projections own their temporary box. Formatting cannot
             * mutate the pack or let a raw slot borrow escape this loop. */
            bool owned=owner && layout.transfer!=3;
            const xvalue* item=owned ? xl6SequenceProjectionTakeV1(owner,
                xrtTypedArrayGetValue(storage,i,converter)) : owner ?
                *(xvalue* const*)xrtTypedArrayConstGet(storage,i) : xrtValueArrayGet(values,i);
            ready=(!owned || item) && xl6ValueTextAppendNativeV1(&output,item,&graph);
            if (!ready) xl6ErrorScopeCaptureFailure(&operation);
            if (owned) xrtValueRelease((xvalue*)item);
            if (operation.failed || xrtGetError()) ready=false;
        }
    } else ready = xl6ValueTextAppendNativeV1(&output, values,&graph);
    if (!ready) xl6ErrorScopeCaptureFailure(&operation);
    if (owner) xl6SequenceEndV1(owner);
    if (operation.failed || xrtGetError()) ready=false;
    if (ready) ready = newline ? xrtConsoleWriteLine(XCONSOLE_STDOUT, xrtStrBufView(&output)) :
        xrtConsoleWrite(XCONSOLE_STDOUT, xrtStrBufView(&output));
    if (!ready) xl6ErrorScopeCaptureFailure(&operation);
    xrtStrBufFree(&output);
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return ready && outcome.ok;
}
