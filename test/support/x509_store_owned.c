/* Normal fixed certificates, managed versions and ordinary lifecycle only. */
#include "src/runtime/xl6_x509_store_internal.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include "x509_path_vectors.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif
static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void state(void* raw,int64_t count,int64_t bytes)
{assert(xl6X509StoreSnapshotCountV1(raw)==count && xl6X509StoreSnapshotBytesV1(raw)==bytes && !xrtGetError());}
static bool equal(xbytesview a,xbytesview b)
{return a.Size==b.Size && !memcmp(a.Data,b.Data,a.Size);}
#define FAULTS(Call,Drop) do { \
    bool completed=false; \
    for(size_t prefix=0;prefix<128;++prefix){ \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix)); \
        void* result=(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets; \
        assert(hit?(!result && xrtErrorKind(xrtGetError())==XERR_MEMORY):(result && !xrtGetError())); \
        if(hit)++failures;else completed=true;xrtClearError();Drop(result);balanced(before);if(completed)break; \
    }assert(completed); \
}while(0)
static void publish(void* raw,const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain)
{
    assert(raw);xvalue* value=xl6NativeHandleTakeV1(&raw,policy);assert(value && !raw);
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(&domain,1);assert(catalog);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope frozen={0};size_t count=0;
    assert(xrtOwnershipFreezeTryBegin(&frozen) && ref.Ops->Count(ref.Data,&count) && count==1);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&prepare,&finish) && adapter && !prepare && !finish);
    int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&frozen));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);xrtValueRelease(value);
}
static void publication(void* store)
{
    for(unsigned kind=0;kind<2;++kind){
        bool completed=false;
        for(size_t prefix=0;prefix<16;++prefix){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            void* raw=kind?xl6X509StoreSnapshotV1(store):xl6X509StoreCreateV1();assert(raw);
            assert(xrtMemDebugFailAfter(prefix));xvalue* value=xl6NativeHandleTakeV1(&raw,kind?xl6X509StoreSnapshotHandlePolicyV1Get():xl6X509StoreHandlePolicyV1Get());
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publications;
            assert(!raw && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();xrtValueRelease(value);balanced(before);
            if(!hit){completed=true;break;}
        }assert(completed);
    }
}
static void mutations(void* root,void* mid,xstrview pem)
{
    void* source=xl6X509StoreCreateV1();assert(source && xl6X509StoreAddV1(source,root) && xl6X509StoreAddV1(source,mid));
    void* input=xl6X509StoreSnapshotV1(source);assert(input);
    int64_t rootBytes=(int64_t)((XL6_X509CertificateV1*)root)->view.Raw.Size;
    int64_t allBytes=rootBytes+(int64_t)((XL6_X509CertificateV1*)mid)->view.Raw.Size;
    for(unsigned operation=0;operation<5;++operation){
        bool completed=false;
        for(size_t prefix=0;prefix<128;++prefix){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            void* store=xl6X509StoreCreateV1();assert(store);
            if(operation==1 || operation==3 || operation==4)assert(xl6X509StoreAddV1(store,root));
            void* old=xl6X509StoreSnapshotV1(store);assert(old);
            /* op 0 and 2 exercise the unique-owner fast path. */
            if(operation==0 || operation==2){xl6X509StoreSnapshotDestroyV1(old);old=NULL;}
            assert(xrtMemDebugFailAfter(prefix));int64_t added=0;
            if(operation<2)added=xl6X509StoreAddV1(store,operation?mid:root);
            else if(operation==2)added=xl6X509StoreAddPemV1(store,pem,-1);
            else if(operation==3)added=xl6X509StoreMergeV1(store,input);
            else xl6X509StoreClearV1(store);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
            assert(hit?xrtErrorKind(xrtGetError())==XERR_MEMORY:!xrtGetError());if(hit)++failures;else completed=true;
            xrtClearError();void* now=xl6X509StoreSnapshotV1(store);assert(now);
            int64_t expected=hit?((operation==1 || operation==3 || operation==4)?1:0):(operation==4?0:(operation==0?1:2));
            state(now,expected,expected==0?0:(expected==1?rootBytes:allBytes));
            if(!hit && operation<4)assert(added==(operation==2?2:1));
            if(old)state(old,1,rootBytes);
            state(input,2,allBytes);xl6X509StoreSnapshotDestroyV1(now);xl6X509StoreSnapshotDestroyV1(old);xl6X509StoreDestroyV1(store);
            balanced(before);if(completed)break;
        }assert(completed);
    }
    xl6X509StoreSnapshotDestroyV1(input);xl6X509StoreDestroyV1(source);
}
typedef struct Reader {void* store;int64_t rootBytes,allBytes;bool done;} Reader;
static void readVersions(Reader* input)
{
    for(unsigned i=0;i<32;++i){
        void* snapshot=xl6X509StoreSnapshotV1(input->store);assert(snapshot);
        int64_t count=xl6X509StoreSnapshotCountV1(snapshot);assert(count>=0 && count<=2);
        state(snapshot,count,count==0?0:(count==1?input->rootBytes:input->allBytes));
        if(count){void* item=xl6X509StoreSnapshotCertificateV1(snapshot,0);assert(item);xl6X509CertificateDestroyV1(item);}
        xl6X509StoreSnapshotDestroyV1(snapshot);
    }input->done=true;
}
#if defined(_WIN32)
static DWORD WINAPI worker(LPVOID raw){readVersions(raw);return 0;}
#else
static void* worker(void* raw){readVersions(raw);return NULL;}
#endif
static void concurrent(void* root,void* mid)
{
    void* store=xl6X509StoreCreateV1();assert(store);Reader inputs[4];
#if defined(_WIN32)
    HANDLE threads[4];
#else
    pthread_t threads[4];
#endif
    int64_t bytes=(int64_t)((XL6_X509CertificateV1*)root)->view.Raw.Size,all=bytes+(int64_t)((XL6_X509CertificateV1*)mid)->view.Raw.Size;
    for(unsigned i=0;i<4;++i){inputs[i]=(Reader){store,bytes,all,false};
#if defined(_WIN32)
        threads[i]=CreateThread(NULL,0,worker,&inputs[i],0,NULL);assert(threads[i]);
#else
        assert(!pthread_create(&threads[i],NULL,worker,&inputs[i]));
#endif
    }
    for(unsigned i=0;i<32;++i){xl6X509StoreClearV1(store);assert(xl6X509StoreAddV1(store,root) && xl6X509StoreAddV1(store,mid));}
    for(unsigned i=0;i<4;++i){
#if defined(_WIN32)
        assert(WaitForSingleObject(threads[i],INFINITE)==WAIT_OBJECT_0 && CloseHandle(threads[i]));
#else
        assert(!pthread_join(threads[i],NULL));
#endif
        assert(inputs[i].done);
    }xl6X509StoreDestroyV1(store);
}
int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    FAULTS(xl6X509StoreCreateV1(),xl6X509StoreDestroyV1);
    for(unsigned i=0;i<4;++i){
        const PathVector* vector=&pathVectors[i];void* root=xl6X509CertificateCreateSpanV1(vector->root),*mid=xl6X509CertificateCreateSpanV1(vector->mid);assert(root && mid);
        void* store=xl6X509StoreCreateV1();assert(store && xl6X509StoreAddV1(store,root));
        XL6_X509StoreVersionV1* snapshot=xl6X509StoreSnapshotV1(store);assert(snapshot);
        assert(((XL6_X509StoreV1*)store)->current==snapshot);
        assert(xrtMemDebugFailAfter(0));void* same=xl6X509StoreSnapshotV1(store);
        assert(same==snapshot && xl6X509StoreCountV1(store)==1 && xl6X509StoreBytesV1(store)==(int64_t)vector->root.Size && !xrtMemDebugFailTriggered());xrtMemDebugFailClear();xl6X509StoreSnapshotDestroyV1(same);
        void* copy=xl6X509StoreCloneV1(store);assert(copy && copy!=store && ((XL6_X509StoreV1*)copy)->current==snapshot);
        assert(xl6X509StoreAddV1(copy,mid) && ((XL6_X509StoreV1*)copy)->current!=snapshot);
        state(snapshot,1,(int64_t)vector->root.Size);assert(xl6X509StoreCountV1(copy)==2 && !xl6X509StoreAddV1(copy,mid) && !xrtGetError());
        assert(!xl6X509StoreSnapshotCertificateV1(snapshot,-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        FAULTS(xl6X509StoreCloneV1(store),xl6X509StoreDestroyV1);
        FAULTS(xl6X509StoreSnapshotCertificateV1(snapshot,0),xl6X509CertificateDestroyV1);
        FAULTS(xl6X509StoreSnapshotAnchorV1(snapshot,0),xl6X509AnchorDestroyV1);
        char text[8192];size_t first,second;assert(xrtPemEncode("CERTIFICATE",vector->root.Data,vector->root.Size,text,sizeof(text),&first));
        assert(xrtPemEncode("CERTIFICATE",vector->mid.Data,vector->mid.Size,text+first,sizeof(text)-first,&second));
        mutations(root,mid,(xstrview){text,first+second});concurrent(root,mid);
        if(!i){
            publication(store);publish(xl6X509StoreCreateV1(),xl6X509StoreHandlePolicyV1Get(),xl6OwnershipDomainX509StoreV1Get());
            publish(xl6X509StoreSnapshotV1(store),xl6X509StoreSnapshotHandlePolicyV1Get(),xl6OwnershipDomainX509StoreSnapshotV1Get());
        }
        void* item=xl6X509StoreSnapshotCertificateV1(snapshot,0),*anchor=xl6X509StoreSnapshotAnchorV1(snapshot,0);assert(item && anchor);
        xl6X509StoreSnapshotDestroyV1(snapshot);xl6X509StoreDestroyV1(store);xl6X509StoreDestroyV1(copy);
        assert(equal(((XL6_X509CertificateV1*)item)->view.Raw,vector->root) && equal(((XL6_X509AnchorV1*)anchor)->certificate.Raw,vector->root));
        xl6X509CertificateDestroyV1(item);xl6X509AnchorDestroyV1(anchor);xl6X509CertificateDestroyV1(root);xl6X509CertificateDestroyV1(mid);
    }
    balanced(before);assert(xrtMemDebugReset());
    printf("X509 owned store: %zu allocation budgets, %zu failures, %zu publication budgets; 2 catalogs; 4x32 readers with 32 update rounds; balanced\n",budgets,failures,publications);return 0;
}
