/* 用确定性的环境/元数据提供者覆盖查找顺序；字体加载仍用真实测试字体。 */
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_ENVIRONMENT
#define XRT_MODULE_PATH
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

enum { ENV_FIRST, ENV_SECOND, JOIN_FIRST, STAT_FIRST=5, DUP_RESULT=11,
    FONT_OBJECT, FONT_BYTES, BITMAP_OBJECT };
static const char* const absolute[]={
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf"
};
static const char* const names[]={"Fonts/arial.ttf","Fonts/segoeui.ttf","Fonts/tahoma.ttf"};
static char* paths[3];
static const char root[]="/xlang-test-fonts/字体";
static struct { bool active,failed,directories;int env,winner,stage,scope;unsigned after; xerror* cause; } model;
static unsigned cases,checks,failures,realBudgets,realFailures;
static void require(bool ok,const char* label)
{
    ++checks;
    if(!ok){if(failures<12)fprintf(stderr,"stbtt discovery: %s\n",label);++failures;}
}
static bool fail(int stage)
{
    if(!model.active)return false;
    if(model.failed){++model.after;return false;}
    if(model.stage!=stage)return false;
    model.failed=true;xrtSetError(model.cause);
    require(xrtMemDebugFailAfter(0),"guard propagation after original publication");
    return true;
}
static bool lookup(const char* name,str* value)
{
    if(!model.active)return xrtEnvLookup(name,value);
    int second=strcmp(name,"SystemRoot")==0;
    *value=NULL;if(fail(second ? ENV_SECOND : ENV_FIRST))return false;
    if(model.env==2 || (model.env==1 && !second))return true;
    *value=xrtStrDup(root);return *value!=NULL;
}
static str get(const char* name)
{str value=NULL;return lookup(name,&value) ? value : NULL;}
static str join(const char* base,const char* name)
{
    if(model.active){
        unsigned index=0;while(index<3 && strcmp(name,names[index]))++index;
        require(index<3,"known relative font candidate");
        if(fail(JOIN_FIRST+(int)index))return NULL;
    }
    return xrtPathJoin(base,name);
}
static int path_index(const char* path)
{
    for(int i=0;i<3;++i)if(!strcmp(path,paths[i]))return i;
    for(int i=0;i<3;++i)if(!strcmp(path,absolute[i]))return i+3;
    require(false,"known candidate path");return 6;
}
static bool metadata(const char* path,bool follow,xfileinfo* info)
{
    if(!model.active)return xrtPathStat(path,follow,info);
    int index=path_index(path);if(fail(STAT_FIRST+index))return false;
    memset(info,0,sizeof(*info));
    if(index==model.winner){info->Type=XFILE_TYPE_FILE;return true;}
    if(model.directories){info->Type=XFILE_TYPE_DIRECTORY;return true;}
    xrtSetErrorInfo(XERR_NOT_FOUND,"test.font.stat",0,"candidate missing");return false;
}
static bool exists(const char* path)
{
    if(!model.active)return xrtFileExists(path);
    int index=path_index(path);if(fail(STAT_FIRST+index))return false;
    return index==model.winner;
}
static str duplicate(const char* text)
{return fail(DUP_RESULT) ? NULL : xrtStrDup(text);}
static void* allocate_object(size_t count,size_t size)
{
    int stage=model.scope==1 ? FONT_OBJECT : BITMAP_OBJECT;
    return model.active && model.scope && fail(stage) ? NULL : xrtCalloc(count,size);
}
static void* allocate_bytes(size_t size)
{return model.active && model.scope==1 && fail(FONT_BYTES) ? NULL : xrtMalloc(size);}
#undef xrtCalloc
#undef xrtMalloc
#define xrtEnvLookup lookup
#define xrtEnvGet get
#define xrtPathJoin join
#define xrtPathStat metadata
#define xrtFileExists exists
#define xrtStrDup duplicate
#define xrtCalloc allocate_object
#define xrtMalloc allocate_bytes
#include "../stb_truetype_native.c"
#undef xrtEnvLookup
#undef xrtEnvGet
#undef xrtPathJoin
#undef xrtPathStat
#undef xrtFileExists
#undef xrtStrDup
#undef xrtCalloc
#undef xrtMalloc

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}
static void normal(int env,int winner,bool directories,bool stale)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    model.env=env;model.winner=winner;model.directories=directories;model.stage=-1;
    model.failed=false;model.after=0;model.scope=0;model.active=true;
    if(stale)xrtSetErrorInfo(XERR_IO,"test.previous",17,"previous call");
    char* result=xstbtt_default_font_path();model.active=false;
    const char* expected=winner<3 ? paths[winner] : winner<6 ? absolute[winner-3] : "";
    require(result && strcmp(result,expected)==0,"priority, absent variables and directory candidates");
    require(xrtGetError()==NULL,"successful or empty result clears previous error");
    xrtFree(result);require(balanced(&before),"normal lookup seven memory metrics balanced");++cases;
}
static void failed(int stage,xerrkind kind,int code,bool longMessage,const void* fontData,size_t fontSize,void* font)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    char message[301];memset(message,'m',longMessage ? 300 : 0);message[longMessage ? 300 : 0]=0;
    xrtSetErrorInfo(kind,"test.font.provider",code,message);model.cause=xrtTakeError();
    require(model.cause!=NULL,"own precreated provider cause");
    model.env=stage==ENV_SECOND ? 1 : 0;model.stage=stage;model.directories=false;
    model.winner=stage>=STAT_FIRST && stage<DUP_RESULT ? stage-STAT_FIRST+1 :
        stage>=JOIN_FIRST && stage<STAT_FIRST ? stage-JOIN_FIRST+1 : 3;
    model.scope=stage>=FONT_OBJECT ? (stage==BITMAP_OBJECT ? 2 : 1) : 0;
    model.failed=false;model.after=0;model.active=true;
    void* result=stage==BITMAP_OBJECT ? xstbtt_font_bitmap(font,65,24.0) :
        stage>=FONT_OBJECT ? xstbtt_load_memory((void*)fontData,fontSize,0) : xstbtt_default_font_path();
    bool extra=xrtMemDebugFailTriggered();xrtMemDebugFailClear();model.active=false;
    require(model.failed && result==NULL,"selected provider failure terminates operation");
    require(model.after==0 && !extra,"no later candidate or diagnostic allocation after first failure");
    require(xrtGetError()==model.cause && xrtErrorKind(xrtGetError())==kind &&
        xrtErrorCode(xrtGetError())==code && !strcmp(xrtErrorDomain(xrtGetError()),"test.font.provider") &&
        !strcmp(xrtErrorMessage(xrtGetError()),message),"full original cause identity and fields retained");
    if(result){if(stage==BITMAP_OBJECT)xstbtt_bitmap_release(result);else if(stage>=FONT_OBJECT)xstbtt_font_release(result);else xrtFree(result);}
    xrtClearError();xrtErrorFree(model.cause);model.cause=NULL;
    require(balanced(&before),"failed lookup/load/bitmap owners and seven metrics balanced");++cases;
}
static void real_allocations(void)
{
    /* 真正分配故障，但环境和候选文件元数据仍由确定性提供者给出。 */
    bool completed=false;
    for(unsigned budget=0;budget<64;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        model.env=0;model.winner=0;model.stage=-1;model.scope=0;model.failed=false;model.active=true;
        require(xrtMemDebugFailAfter(budget),"arm actual allocation failure");
        char* result=xstbtt_default_font_path();
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();model.active=false;++realBudgets;
        if(hit){require(result==NULL && xrtErrorKind(xrtGetError())==XERR_MEMORY,"actual OOM cannot become a fallback result");++realFailures;}
        else{require(result && !strcmp(result,paths[0]) && !xrtGetError(),"actual allocation sweep reaches success");completed=true;}
        xrtFree(result);require(balanced(&before),"actual allocation budget balanced");
        if(completed)break;
    }
    require(completed && realFailures>0,"actual allocator failures and success observed");
}
int main(void)
{
    /* 同一夹具同时审查旧Get/Exists与现行Lookup/Stat调用路径。 */
    (void)get;(void)exists;(void)lookup;(void)metadata;
    if(!xrtMemDebugEnabled())return 2;
    for(int i=0;i<3;++i){paths[i]=xrtPathJoin(root,names[i]);if(!paths[i])return 2;}
    size_t size=0;bytes data=xrtFileReadAllLimit("release/extlibs/stb_truetype/test/assets/DejaVuSans-test.ttf",32u*1024u*1024u,&size);
    void* font=data ? xstbtt_load_memory(data,size,0) : NULL;if(!font)return 2;
    xrtSetErrorInfo(XERR_STATE,"test.warm",0,"warm");xrtClearError();
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(int env=0;env<3;++env)for(int winner=env==2 ? 3 : 0;winner<=6;++winner)
        for(unsigned directories=0;directories<2;++directories)for(unsigned stale=0;stale<2;++stale)normal(env,winner,directories!=0,stale!=0);
    const xerrkind kinds[]={XERR_IO,XERR_MEMORY,XERR_PERMISSION};const int codes[]={INT_MIN,-17,INT_MAX};
    for(int stage=0;stage<=BITMAP_OBJECT;++stage)for(unsigned k=0;k<(stage>=FONT_OBJECT ? 1u : 3u);++k)
        for(unsigned c=0;c<3;++c)for(unsigned length=0;length<2;++length)
            failed(stage,stage>=FONT_OBJECT ? XERR_MEMORY : kinds[k],codes[c],length!=0,data,size,font);
    real_allocations();require(balanced(&before),"entire model returns to retained input/font baseline");
    xstbtt_font_release(font);xrtFree(data);for(int i=0;i<3;++i)xrtFree(paths[i]);
    printf("stbtt discovery: %u cases, %u checks, %u actual OOM/%u budgets, %u failures\n",cases,checks,realFailures,realBudgets,failures);
    return failures ? 1 : 0;
}
