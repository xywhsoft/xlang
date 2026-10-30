/* 先按 manifest 生成唯一 XRT，后面的产品源码不能再补全缺失的实现。 */
#include "xrt.h"
#include "../include/json_patch.h"
#include "../json_patch_native.c"
#include <stdio.h>

int main(void)
{
    xvalue *doc=xrtValueObject(), *patch=xrtValueArray(), *op=xrtValueObject();
    bool ok=doc && patch && op &&
        xrtValueObjectSetNew(doc,XRT_STR_LITERAL("a"),xrtValueInt(7)) &&
        xrtValueObjectSetNew(op,XRT_STR_LITERAL("op"),xrtValueString(XRT_STR_LITERAL("move"))) &&
        xrtValueObjectSetNew(op,XRT_STR_LITERAL("from"),xrtValueString(XRT_STR_LITERAL("/a"))) &&
        xrtValueObjectSetNew(op,XRT_STR_LITERAL("path"),xrtValueString(XRT_STR_LITERAL("/b"))) &&
        xrtValueArrayAppendTake(patch,&op);
    xvalue* result=ok ? xjsonpatch_apply(doc,patch) : NULL;
    int64 value=0;
    ok=ok && result && !xjsonpatch_has_error() && !xjsonpatch_has(result,"/a") &&
        xrtValueGetInt(xrtValueObjectGet(result,XRT_STR_LITERAL("b")),&value) && value==7 &&
        xjsonpatch_has(doc,"/a");
    xrtValueRelease(result); xrtValueRelease(op); xrtValueRelease(patch); xrtValueRelease(doc);
    if (ok) puts("json.patch manifest-only: PASS");
    return ok ? 0 : 1;
}
