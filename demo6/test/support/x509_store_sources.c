/* Ordinary fixed certificates, owned local files/VFS and allocation rollback.
 * OS roots are read-only and explicit; no network or malformed crypto input. */
#include "src/runtime/xl6_x509_store_internal.h"
#include "src/runtime/xl6_x509_store_abi.h"
#include "src/runtime/xl6_vfs_private.h"
#include "x509_path_vectors.h"
#include <assert.h>
#include <stdio.h>
static size_t budgets,failures;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
int main(int argc,char** argv)
{
    assert(argc==2 && strlen(argv[1])<3900);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    char path[4096];assert(snprintf(path,sizeof(path),"%s.der",argv[1])>0);
    const PathVector* vector=&pathVectors[1];assert(xrtFileWriteAll(path,vector->mid));
    xstrview file={path,strlen(path)};str pem=xrtPemEncodeNew("CERTIFICATE",vector->mid.Data,vector->mid.Size);assert(pem);
    xbuffer buffer={0};assert(xrtBufferAppend(&buffer,vector->mid));
    XL6_VfsOwner fs={xrtVfsCreate()};xvfsmemory memory=xrtVfsMemoryCreate();assert(fs.native && memory);
    assert(xrtVfsMemoryPutCopy(memory,"mid.der",vector->mid.Data,vector->mid.Size) && xrtVfsMemorySeal(memory));
    xvfsmount mount=xrtVfsMemoryMount(fs.native,"/",0,XVFS_CASE_SENSITIVE,memory,0);assert(mount);
    for(unsigned operation=0;operation<5;++operation){
        bool done=false;
        for(size_t budget=0;budget<128;++budget){
            xmemdebugsnapshot prior;xrtMemDebugSnapshot(&prior);
            void* store=xl6X509StoreCreateV1();assert(store);
            assert(xl6X509StoreAddEncodedBytesV1(store,vector->root,-1)==1);
            void* old=xl6X509StoreSnapshotV1(store);assert(old);
            assert(xrtMemDebugFailAfter(budget));int64_t added=0;
            if(operation==0)added=xl6X509StoreAddEncodedV1(store,(xstrview){pem,strlen(pem)},-1);
            else if(operation==1)added=xl6X509StoreAddEncodedBytesV1(store,vector->mid,-1);
            else if(operation==2)added=xl6X509StoreAddEncodedBufferV1(store,&buffer,-1);
            else if(operation==3)added=xl6X509StoreAddFileV1(store,file,(int64_t)vector->mid.Size);
            else added=xl6X509StoreAddVfsV1(store,&fs,(xstrview){"/mid.der",8},(int64_t)vector->mid.Size);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
            assert(hit?(added==0 && xrtErrorKind(xrtGetError())==XERR_MEMORY):(added==1 && !xrtGetError()));
            if(hit)++failures;else done=true;xrtClearError();
            assert(xl6X509StoreCountV1(store)==(hit?1:2) && xl6X509StoreSnapshotCountV1(old)==1);
            assert(xl6X509StoreBytesV1(store)==(int64_t)(vector->root.Size+(hit?0:vector->mid.Size)));
            xl6X509StoreSnapshotDestroyV1(old);xl6X509StoreDestroyV1(store);balanced(prior);if(done)break;
        }assert(done);
    }
    void* store=xl6X509StoreCreateV1();assert(store);
    assert(xl6X509StoreAddFileV1(store,file,0)==0 && xrtGetError());xrtClearError();
    assert(xl6X509StoreAddVfsV1(store,&fs,(xstrview){"/mid.der",8},0)==0 && xrtGetError());xrtClearError();
    assert(xl6X509StoreCountV1(store)==0);xl6X509StoreDestroyV1(store);
    /* Read native OS sources once per binary, without writing host stores. */
    void* system=xl6X509StoreSnapshotSystemV1();assert(system && xl6X509StoreSnapshotCountV1(system)>0 && xl6X509StoreSnapshotBytesV1(system)>0);
    xl6X509StoreSnapshotDestroyV1(system);
    xrtVfsMountDestroy(mount);xrtVfsDestroy(fs.native);xrtVfsMemoryDestroy(memory);
    xrtBufferUnit(&buffer);xrtFree(pem);assert(!xrtGetError());balanced(before);assert(xrtMemDebugReset());
    printf("X509 trust sources: %zu allocation budgets; %zu failures; data/file/VFS rollback; OS snapshot; balanced\n",budgets,failures);return 0;
}
