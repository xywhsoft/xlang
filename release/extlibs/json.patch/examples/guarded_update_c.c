/* 与guarded_update.xl相同；C直接调用公开API，结果/输入引用分别释放。 */
#define XRT_MODULE_JSON_READ
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_UNICODE
#include "../include/json_patch.h"
#include <stdio.h>
#include <string.h>

static xvalue* parse(cstr text) { return xrtJsonParse((xstrview){text,strlen(text)}); }
static bool integer(xvalue* doc,cstr path,int64 expected)
{
    xvalue* value=xjsonpatch_get(doc,path);
    int64 actual;
    bool okay=!xjsonpatch_has_error() && xrtValueGetInt(value,&actual) && actual==expected;
    xrtValueRelease(value); return okay;
}
static bool boolean(xvalue* doc,cstr path,bool expected)
{
    xvalue* value=xjsonpatch_get(doc,path);
    bool actual;
    bool okay=!xjsonpatch_has_error() && xrtValueGetBool(value,&actual) && actual==expected;
    xrtValueRelease(value); return okay;
}
static bool rejects(xvalue* doc,xvalue* patch)
{
    xvalue* value=xjsonpatch_apply(doc,patch);
    bool okay=value && xrtValueType(value)==XVALUE_NULL && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_VALUE;
    xrtValueRelease(value); xrtClearError(); return okay;
}
int main(void)
{
    xvalue* original=parse("{\"revision\":1,\"active\":false,\"limits\":{\"count\":0}}");
    xvalue* operations=parse("[{\"op\":\"test\",\"path\":\"/revision\",\"value\":1.0},"
        "{\"op\":\"replace\",\"path\":\"/revision\",\"value\":2},"
        "{\"op\":\"replace\",\"path\":\"/active\",\"value\":true}]");
    xvalue* invalid=parse("[{\"op\":\"test\",\"path\":\"/active\",\"value\":0}]");
    xvalue* patch=parse("{\"limits\":{\"count\":null,\"burst\":4}}");
    xvalue *updated=NULL,*merged=NULL;
    int status=1;
    if (!original || !operations || !invalid || !patch) goto done;
    // test只校验快照，不替代调用方对共享状态的同步。
    updated=xjsonpatch_apply(original,operations);
    if (!updated || xjsonpatch_has_error() || !integer(updated,"/revision",2) || !integer(original,"/revision",1)) goto done;
    puts("updated revision: 2\noriginal revision: 1");
    if (!rejects(original,invalid) || !boolean(original,"/active",false)) goto done;
    puts("boolean guard: true");
    if (!rejects(updated,operations) || !integer(updated,"/revision",2)) goto done;
    puts("stale guard: true");
    merged=xjsonpatch_merge(updated,patch);
    if (!merged || xjsonpatch_has_error() || xjsonpatch_has(merged,"/limits/count") ||
        !xjsonpatch_has(updated,"/limits/count") || !integer(merged,"/limits/burst",4) || !boolean(updated,"/active",true)) goto done;
    puts("merge delete: true\nmerge burst: 4"); status=0;
done:
    xrtValueRelease(merged); xrtValueRelease(updated); xrtValueRelease(patch);
    xrtValueRelease(invalid); xrtValueRelease(operations); xrtValueRelease(original); return status;
}

#include "../json_patch_native.c"
