/* Ordinary offline fixed vectors, allocation rollback and real leaf ownership.
 * No traffic, malformed points, protocol/security tests or algorithm substitute. */
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8 x25519_private[]={0x77,0x07,0x6d,0x0a,0x73,0x18,0xa5,0x7d,0x3c,0x16,0xc1,0x72,0x51,0xb2,0x66,0x45,0xdf,0x4c,0x2f,0x87,0xeb,0xc0,0x99,0x2a,0xb1,0x77,0xfb,0xa5,0x1d,0xb9,0x2c,0x2a};
static const uint8 x25519_public[]={0x85,0x20,0xf0,0x09,0x89,0x30,0xa7,0x54,0x74,0x8b,0x7d,0xdc,0xb4,0x3e,0xf7,0x5a,0x0d,0xbf,0x3a,0x0d,0x26,0x38,0x1a,0xf4,0xeb,0xa4,0xa9,0x8e,0xaa,0x9b,0x4e,0x6a};
static const uint8 x25519_peer[]={0xde,0x9e,0xdb,0x7d,0x7b,0x7d,0xc1,0xb4,0xd3,0x5b,0x61,0xc2,0xec,0xe4,0x35,0x37,0x3f,0x83,0x43,0xc8,0x5b,0x78,0x67,0x4d,0xad,0xfc,0x7e,0x14,0x6f,0x88,0x2b,0x4f};
static const uint8 x25519_shared[]={0x4a,0x5d,0x9d,0x5b,0xa4,0xce,0x2d,0xe1,0x72,0x8e,0x3b,0xf4,0x80,0x35,0x0f,0x25,0xe0,0x7e,0x21,0xc9,0x47,0xd1,0x9e,0x33,0x76,0xf0,0x9b,0x3c,0x1e,0x16,0x17,0x42};
static const uint8 x448_private[]={0x9a,0x8f,0x49,0x25,0xd1,0x51,0x9f,0x57,0x75,0xcf,0x46,0xb0,0x4b,0x58,0x00,0xd4,0xee,0x9e,0xe8,0xba,0xe8,0xbc,0x55,0x65,0xd4,0x98,0xc2,0x8d,0xd9,0xc9,0xba,0xf5,0x74,0xa9,0x41,0x97,0x44,0x89,0x73,0x91,0x00,0x63,0x82,0xa6,0xf1,0x27,0xab,0x1d,0x9a,0xc2,0xd8,0xc0,0xa5,0x98,0x72,0x6b};
static const uint8 x448_public[]={0x9b,0x08,0xf7,0xcc,0x31,0xb7,0xe3,0xe6,0x7d,0x22,0xd5,0xae,0xa1,0x21,0x07,0x4a,0x27,0x3b,0xd2,0xb8,0x3d,0xe0,0x9c,0x63,0xfa,0xa7,0x3d,0x2c,0x22,0xc5,0xd9,0xbb,0xc8,0x36,0x64,0x72,0x41,0xd9,0x53,0xd4,0x0c,0x5b,0x12,0xda,0x88,0x12,0x0d,0x53,0x17,0x7f,0x80,0xe5,0x32,0xc4,0x1f,0xa0};
static const uint8 x448_peer[]={0x3e,0xb7,0xa8,0x29,0xb0,0xcd,0x20,0xf5,0xbc,0xfc,0x0b,0x59,0x9b,0x6f,0xec,0xcf,0x6d,0xa4,0x62,0x71,0x07,0xbd,0xb0,0xd4,0xf3,0x45,0xb4,0x30,0x27,0xd8,0xb9,0x72,0xfc,0x3e,0x34,0xfb,0x42,0x32,0xa1,0x3c,0xa7,0x06,0xdc,0xb5,0x7a,0xec,0x3d,0xae,0x07,0xbd,0xc1,0xc6,0x7b,0xf3,0x36,0x09};
static const uint8 x448_shared[]={0x07,0xff,0xf4,0x18,0x1a,0xc6,0xcc,0x95,0xec,0x1c,0x16,0xa9,0x4a,0x0f,0x74,0xd1,0x2d,0xa2,0x32,0xce,0x40,0xa7,0x75,0x52,0x28,0x1d,0x28,0x2b,0xb6,0x0c,0x0b,0x56,0xfd,0x24,0x64,0xc3,0x35,0x54,0x39,0x36,0x52,0x1c,0x24,0x40,0x30,0x85,0xd5,0x9a,0x44,0x9a,0x50,0x37,0x51,0x4a,0x87,0x9d};

typedef struct KeyApi {
    const char* name;size_t size;const uint8 *privateKey,*publicKey,*peer,*shared;
    void* (*create)(const xvalue*);
    void* (*generate)(void);
    void (*destroy)(void*);
    xvalue* (*publicBytes)(void*);
    xvalue* (*exportPrivate)(void*);
    xvalue* (*sharedBytes)(void*,const xvalue*);
    xvalue* (*raw)(const xvalue*,const xvalue*);
    const XL6_NativeHandlePolicyV1* (*policy)(void);
} KeyApi;
static const KeyApi apis[]={
    {"X25519",32,x25519_private,x25519_public,x25519_peer,x25519_shared,
     xl6CryptoX25519CreateV1,xl6CryptoX25519GenerateV1,xl6CryptoX25519DestroyV1,xl6CryptoX25519PublicV1,
     xl6CryptoX25519ExportPrivateV1,xl6CryptoX25519SharedV1,xl6CryptoX25519RawV1,xl6CryptoX25519HandlePolicyV1Get},
    {"X448",56,x448_private,x448_public,x448_peer,x448_shared,
     xl6CryptoX448CreateV1,xl6CryptoX448GenerateV1,xl6CryptoX448DestroyV1,xl6CryptoX448PublicV1,
     xl6CryptoX448ExportPrivateV1,xl6CryptoX448SharedV1,xl6CryptoX448RawV1,xl6CryptoX448HandlePolicyV1Get}
};
static size_t prefixes,failures,graphs;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void equalBytes(xvalue* value,const uint8* expected,size_t size)
{
    xbytesview view;assert(value&&xrtValueGetBytes(value,&view)&&view.Size==size&&!memcmp(view.Data,expected,size));
    xrtValueRelease(value);
}
static void leaf(const KeyApi* api,bool reverse)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xvalue* input=xrtValueBytes((xbytesview){api->privateKey,api->size});assert(input);
    void* raw=api->create(input);assert(raw);
    xvalue* value=xl6NativeHandleTakeV1(&raw,api->policy());assert(value&&!raw);
    const XL6_OwnershipDomainV1* domains[2];
    domains[reverse?1:0]=xl6OwnershipDomainCryptoX25519V1Get();
    domains[reverse?0:1]=xl6OwnershipDomainCryptoX448V1Get();
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
    assert(xrtOwnershipFreezeTryBegin(&freeze)&&ref.Ops->Count(ref.Data,&count)&&count==1);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&prep,&finish)&&adapter&&!prep&&!finish);
    int token=0;assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);
    xrtValueRelease(value);xrtValueRelease(input);xl6OwnershipCatalogDestroyV1(catalog);
    balanced(before);++graphs;
}
static bool fault(const KeyApi* api,unsigned operation,size_t prefix)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xvalue* input=xrtValueBytes((xbytesview){api->privateKey,api->size});
    xvalue* peer=xrtValueBytes((xbytesview){api->peer,api->size});assert(input&&peer);
    void* base=api->create(input);assert(base);
    void* owned=operation==6?api->create(input):NULL;assert(operation!=6||owned);
    void* object=NULL;xvalue* value=NULL;
    assert(xrtMemDebugFailAfter(prefix));
    switch(operation) {
    case 0:object=api->create(input);break;
    case 1:object=api->generate();break;
    case 2:value=api->publicBytes(base);break;
    case 3:value=api->exportPrivate(base);break;
    case 4:value=api->sharedBytes(base,peer);break;
    case 5:value=api->raw(input,peer);break;
    case 6:value=xl6NativeHandleTakeV1(&owned,api->policy());assert(!owned);break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++prefixes;
    if(hit){assert(!object&&!value&&xrtGetError());++failures;}else assert(object||value);
    xerror* error=xrtTakeError();
    if(!hit&&value&&operation<6)equalBytes(value,operation==2?api->publicKey:operation==3?api->privateKey:api->shared,api->size);
    else xrtValueRelease(value);
    api->destroy(object);api->destroy(owned);
    equalBytes(api->publicBytes(base),api->publicKey,api->size);
    equalBytes(api->exportPrivate(base),api->privateKey,api->size);
    api->destroy(base);xrtValueRelease(input);xrtValueRelease(peer);xrtErrorFree(error);xrtClearError();
    balanced(before);return hit;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(size_t i=0;i<sizeof(apis)/sizeof(*apis);++i) {
        const KeyApi* api=&apis[i];void* warm=api->generate();assert(warm);api->destroy(warm);
        for(unsigned operation=0;operation<7;++operation) {
            size_t prefix=0;while(prefix<32&&fault(api,operation,prefix))++prefix;
            assert(prefix<32); /* The first complete success closes every preceding prefix. */
        }
        leaf(api,false);leaf(api,true);
        xvalue* input=xrtValueBytes((xbytesview){api->privateKey,api->size});assert(input);
        void* key=api->create(input);assert(key);
        xrtSetErrorKind(XERR_RANGE);const xerror* original=xrtGetError();api->destroy(key);
        assert(xrtGetError()==original);xrtClearError();xrtValueRelease(input);
    }
    printf("managed XDH: %zu allocation prefixes, %zu triggered failures, %zu real catalog/leaf graphs; no partial outputs\n",prefixes,failures,graphs);
    assert(prefixes>=28&&failures>=14&&graphs==4&&!xrtGetError());return 0;
}
