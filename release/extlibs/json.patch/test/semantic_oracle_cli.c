/* 测试输出完整JSON，由独立参照判断；不使用xrtValueEqual自证test的行为。 */
#define XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_WRITE
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/json_patch.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../json_patch_native.c"
#endif

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}

static bool inspect(cstr line,size_t size)
{
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    xvalue *input=xrtJsonParse((xstrview){line,size}),*result=NULL;
    str output=NULL,document=NULL,patch=NULL;
    bool okay=false;
    xstrview mode;
    if (!input || xrtValueType(input)!=XVALUE_ARRAY || xrtValueCount(input)!=3u ||
        !xrtValueGetString(xrtValueArrayAt(input,0),&mode) || mode.Size!=1u) goto done;
    xvalue *doc=xrtValueArrayAt(input,1),*ops=xrtValueArrayAt(input,2);
    if (mode.Data[0]=='T') result=xjsonpatch_apply(doc,ops);
    else if (mode.Data[0]=='M') result=xjsonpatch_merge(doc,ops);
    else goto done;
    const xerror* error=xrtGetError();
    cstr status=error ? (xrtErrorKind(error)==XERR_VALUE ? "value" : "other") : "ok";
    xrtClearError();
    if (!result) goto done;
    output=xrtJsonStringify(result,false,NULL);
    document=xrtJsonStringify(doc,false,NULL);
    patch=xrtJsonStringify(ops,false,NULL);
    if (!output || !document || !patch || xrtGetError()!=NULL) goto done;
    /* JSON控制字符已经转义，因此tab可以无歧义分隔四个原始结果。 */
    okay=printf("%s\t%s\t%s\t%s\n",status,output,document,patch)>=0;
done:
    xrtFree(output); xrtFree(document); xrtFree(patch);
    xrtValueRelease(result); xrtValueRelease(input);
    return balanced(&before) && okay;
}

int main(void)
{
    char line[65536];
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE,"test.json.patch",0,"warm diagnostics"); xrtClearError();
    str warm=xrtJsonStringify(xrtValueNull(),false,NULL);
    if (!warm) return 2;
    xrtFree(warm);
    while (fgets(line,sizeof(line),stdin)) {
        size_t size=strlen(line);
        if (size==0u || line[size-1u]!='\n') return 2;
        if (!inspect(line,size)) return 3;
    }
    return ferror(stdin) || ferror(stdout) ? 2 : 0;
}
