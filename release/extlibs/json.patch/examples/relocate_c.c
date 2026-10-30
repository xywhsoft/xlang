/* 与 relocate.xl 读取相同 JSON 配置，演示 copy 独立性、move 与失败原子性。 */
#define XRT_MODULE_JSON_READ
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_UNICODE
#include "../include/json_patch.h"
#include <stdio.h>
#include <string.h>

static xvalue* parse(const char* text)
{
    return xrtJsonParse((xstrview){text,strlen(text)});
}

static bool label(xvalue* doc,const char* path,const char* name,const char* expected)
{
    xvalue* value=xjsonpatch_get(doc,path);
    xstrview text;
    bool ok=!xjsonpatch_has_error() && xrtValueGetString(value,&text) &&
        text.Size==strlen(expected) && memcmp(text.Data,expected,text.Size)==0;
    if (ok) printf("%s=%.*s\n",name,(int)text.Size,text.Data);
    xrtValueRelease(value);
    return ok;
}

int main(void)
{
    xvalue* config=parse("{\"draft\":{\"title\":\"Ready\",\"tags\":[\"xlang\",\"C\"]}}");
    xvalue* ops=parse("["
        "{\"op\":\"copy\",\"from\":\"/draft\",\"path\":\"/backup\"},"
        "{\"op\":\"move\",\"from\":\"/draft\",\"path\":\"/published\"},"
        "{\"op\":\"replace\",\"path\":\"/published/title\",\"value\":\"Live\"}]");
    xvalue* failing=parse("["
        "{\"op\":\"move\",\"from\":\"/draft\",\"path\":\"/published\"},"
        "{\"op\":\"remove\",\"path\":\"/missing\"}]");
    xvalue* published=config && ops ? xjsonpatch_apply(config,ops) : NULL;
    bool ok=published && !xjsonpatch_has_error() && xjsonpatch_has(config,"/draft") &&
        !xjsonpatch_has(published,"/draft") &&
        label(published,"/published/title","title","Live") &&
        label(published,"/backup/title","backup","Ready") && label(config,"/draft/title","original","Ready");
    xvalue* bad=config && failing ? xjsonpatch_apply(config,failing) : NULL;
    ok=ok && xrtValueType(bad)==XVALUE_NULL && xrtErrorKind(xrtGetError())==XERR_VALUE &&
        xjsonpatch_has(config,"/draft") && !xjsonpatch_has(config,"/published");
    xvalue* tags=ok ? xjsonpatch_get(published,"/published/tags") : NULL;
    ok=ok && !xjsonpatch_has_error() && xrtValueCount(tags)==2;
    if (ok) puts("atomic=true\ntags=2");
    xrtValueRelease(tags); xrtValueRelease(bad); xrtValueRelease(published);
    xrtValueRelease(failing); xrtValueRelease(ops); xrtValueRelease(config);
    return ok ? 0 : 1;
}

#include "../json_patch_native.c"
