/* 正常文件/锁/关闭均用真实实现；只在指定边界发布可追踪的失败。 */
#define XRT_MODULE_FILE_LOCK
#define XRT_MODULE_DIR
#define XRT_MODULE_PATH
#define XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_UNICODE
#define XRT_MODULE_ONCE
#define XRT_MODULE_MUTEX
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#ifndef _WIN32
#include <errno.h>
#include <sys/file.h>
#endif

enum { CONTENDED_CLOSE, LOCK_CLOSE, RELEASE_CLOSE, UNLOCK_CLOSE, NATIVE_CLOSE, OPEN_NATIVE_CLOSE };
static struct { bool enabled, guarded; int mode, osCode; unsigned closes; xerror *first, *last; } fault;
static unsigned cases, checks, failures;
static unsigned oomBudgets, oomFailures;
static void require(bool ok,const char* label)
{
    ++checks;
    if(!ok){if(failures<12)fprintf(stderr,"singleinstance cleanup: %s\n",label);++failures;}
}
static void publish(xerror* error)
{
    xrtSetError(error);
    if(!fault.guarded){
        require(xrtMemDebugFailAfter(0),"guard allocations after cause publication");fault.guarded=true;
    }
}
static bool close_file(xfile file)
{
    bool result=xrtClose(file);
    if(fault.enabled){++fault.closes;publish(fault.last);return false;}
    return result;
}
#ifdef _WIN32
static bool lock_file(xfile file,xfilelock mode,bool wait)
{
    if(fault.enabled && (fault.mode==CONTENDED_CLOSE || fault.mode==LOCK_CLOSE)){
        /* 正常争用由 AGAIN 表示；错误对象在测试开始前创建。 */
        publish(fault.first);return false;
    }
    return xrtFileLock(file,mode,wait);
}
static bool unlock_file(xfile file)
{
    if(fault.enabled && fault.mode==UNLOCK_CLOSE){publish(fault.first);return false;}
    return xrtFileUnlock(file);
}
#define xrtFileLock lock_file
#define xrtFileUnlock unlock_file
#else
static int lock_native(int descriptor,int operation)
{
    if(fault.enabled && ((operation==(LOCK_EX|LOCK_NB) &&
        (fault.mode==CONTENDED_CLOSE || fault.mode==LOCK_CLOSE)) ||
        (operation==LOCK_UN && fault.mode==UNLOCK_CLOSE))){
        errno=fault.mode==CONTENDED_CLOSE ? EWOULDBLOCK : fault.osCode;
        return -1;
    }
    return flock(descriptor,operation);
}
static intptr_t native_file(xfile file)
{
    if(fault.enabled && (fault.mode==NATIVE_CLOSE || fault.mode==OPEN_NATIVE_CLOSE)){publish(fault.first);return -1;}
    return xrtFileNative(file);
}
#define flock lock_native
#define xrtFileNative native_file
#endif
#define xrtClose close_file
#include "../singleinstance_native.c"
#undef xrtClose

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    return before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->OverflowCount==after.OverflowCount && before->UnderflowCount==after.UnderflowCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount;
}
static void one(int mode,xerrkind kind,int code,bool longMessage)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    char message[301];memset(message,'m',longMessage ? 300 : 0);message[longMessage ? 300 : 0]=0;
    xrtSetErrorInfo(mode==CONTENDED_CLOSE ? XERR_AGAIN : kind,"test.singleinstance.first",code,message);
    fault.first=xrtTakeError();
    xrtSetErrorInfo(XERR_IO,"test.singleinstance.close",-17,"close failed after real retirement");
    fault.last=xrtTakeError();
    require(fault.first && fault.last,"create independent cause owners before failure");
    fault.mode=mode;fault.closes=0;fault.osCode=EIO;fault.guarded=false;
    bool release=mode>=RELEASE_CLOSE && mode!=OPEN_NATIVE_CLOSE;
    if(release)require(xsi_acquire("cleanup-contract")==1,"prepare real held lock");
    fault.enabled=true;
    int result=release ? xsi_release() : xsi_acquire("cleanup-contract");
    bool extraAllocation=xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();fault.enabled=false;
    require(!extraAllocation,"cause retirement did not attempt another allocation");
    require(result==(release ? 0 : -1),"cleanup failure must be an error, not normal contention");
    require(fault.closes==1,"retire real file exactly once even after earlier failure");
    xerror* expected=mode==CONTENDED_CLOSE || mode==RELEASE_CLOSE ? fault.last : fault.first;
#ifndef _WIN32
    if(mode==LOCK_CLOSE || mode==UNLOCK_CLOSE){
        require(xrtErrorKind(xrtGetError())==XERR_IO && xrtErrorCode(xrtGetError())==EIO &&
            strcmp(xrtErrorDomain(xrtGetError()),"xlang.extlib.singleinstance")==0,
            "native errno diagnosis survives later close failure");
    }else
#endif
    require(xrtGetError()==expected && xrtErrorKind(expected)==(expected==fault.last ? XERR_IO : kind) &&
        xrtErrorCode(expected)==(expected==fault.last ? -17 : code) &&
        strcmp(xrtErrorMessage(expected),expected==fault.last ? "close failed after real retirement" : message)==0,
        "first cause identity/kind/code/full message preserved");
    xrtClearError();
    require(!xsi_is_acquired(),"failed retirement consumes old package handle");
    require(xsi_acquire("cleanup-contract")==1 && xsi_release(),"same name recovers after closing fault");
    require(xsi_release() && !xrtGetError(),"release remains idempotent and clears prior errors");
    xrtErrorFree(fault.first);xrtErrorFree(fault.last);fault.first=fault.last=NULL;
    require(balanced(&before),"seven memory metrics return to case baseline");++cases;
}
static void allocations(void)
{
    bool completed=false;
    for(unsigned budget=0;budget<64;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        require(xrtMemDebugFailAfter(budget),"arm real acquire allocation failure");
        int result=xsi_acquire("cleanup-contract");
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++oomBudgets;
        if(hit){
            require(result==-1 && xrtErrorKind(xrtGetError())==XERR_MEMORY,"real allocation failure remains Memory, not contention");
            require(!xsi_is_acquired(),"failed acquire publishes no package owner");++oomFailures;
        }else{
            require(result==1 && xsi_release(),"allocation sweep reaches successful ownership");completed=true;
        }
        require(balanced(&before),"real OOM releases all temporary paths and file owners");
        if(completed)break;
    }
    require(completed && oomFailures>0,"sweep includes genuine failures and final success");
}
int main(void)
{
    const char* home=getenv("XLANG_SI_TEST_HOME");
    const char* actual=getenv(
#ifdef _WIN32
        "USERPROFILE"
#else
        "HOME"
#endif
    );
    if(!home || !actual || strcmp(home,actual) || !xrtMemDebugEnabled())return 2;
    xrtSetErrorInfo(XERR_STATE,"test.warm",0,"warm");xrtClearError();
    xmemdebugsnapshot cold;xrtMemDebugSnapshot(&cold);
    require(xsi_acquire("cleanup-contract")==1 && xsi_release(),"warm private directory and one-time mutex");
    static const xerrkind kinds[]={XERR_IO,XERR_MEMORY,XERR_VALUE};
    static const int codes[]={INT_MIN,-17,INT_MAX};
    int modes=4;
#ifndef _WIN32
    modes=6;
#endif
    for(int mode=0;mode<modes;++mode)for(unsigned k=0;k<3;++k)for(unsigned c=0;c<3;++c)
        for(unsigned length=0;length<2;++length)one(mode,kinds[k],codes[c],length!=0);
    allocations();
    require(xrtMutexUnit(&xsi_guard),"retire inline mutex without freeing its static storage");
    require(balanced(&cold),"test-owned once mutex and all temporary owners retire");
    printf("singleinstance cleanup: %u cases, %u checks, %u actual OOM/%u budgets, %u failures\n",cases,checks,oomFailures,oomBudgets,failures);
    return failures ? 1 : 0;
}
