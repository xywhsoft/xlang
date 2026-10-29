/* Offline fixed data, complete adapter allocation prefixes and real leaf
 * ownership. No malformed point/signature campaigns or application traffic. */
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_crypto_ed25519_verify_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8 seedData[]={0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
static const uint8 publicData[]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
static const uint8 pureSignature[]={0xe5,0x56,0x43,0x00,0xc3,0x60,0xac,0x72,0x90,0x86,0xe2,0xcc,0x80,0x6e,0x82,0x8a,0x84,0x87,0x7f,0x1e,0xb8,0xe5,0xd9,0x74,0xd8,0x73,0xe0,0x65,0x22,0x49,0x01,0x55,0x5f,0xb8,0x82,0x15,0x90,0xa3,0x3b,0xac,0xc6,0x1e,0x39,0x70,0x1c,0xf9,0xb4,0x6b,0xd2,0x5b,0xf5,0xf0,0x59,0x5b,0xbe,0x24,0x65,0x51,0x41,0x43,0x8e,0x7a,0x10,0x0b};
static const uint8 digestData[]={0xdd,0xaf,0x35,0xa1,0x93,0x61,0x7a,0xba,0xcc,0x41,0x73,0x49,0xae,0x20,0x41,0x31,0x12,0xe6,0xfa,0x4e,0x89,0xa9,0x7e,0xa2,0x0a,0x9e,0xee,0xe6,0x4b,0x55,0xd3,0x9a,0x21,0x92,0x99,0x2a,0x27,0x4f,0xc1,0xa8,0x36,0xba,0x3c,0x23,0xa3,0xfe,0xeb,0xbd,0x45,0x4d,0x44,0x23,0x64,0x3c,0xe8,0x0e,0x2a,0x9a,0xc9,0x4f,0xa5,0x4c,0xa4,0x9f};

typedef struct State {
    xvalue *seed,*publicKey,*message,*context,*digest,*pure,*ctx,*ph;
    void *key,*buffer;
    uint8 ctxSignature[64],phSignature[64];
} State;
static size_t prefixes,failures,graphs,metadataCases;
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
}
static void initialize(State* state)
{
    memset(state,0,sizeof(*state));
    state->seed=xrtValueBytes((xbytesview){seedData,32});
    state->publicKey=xrtValueBytes((xbytesview){publicData,32});
    state->message=xrtValueBytes((xbytesview){NULL,0});
    state->context=xrtValueBytes((xbytesview){(const uint8*)"foo",3});
    state->digest=xrtValueBytes((xbytesview){digestData,64});
    state->pure=xrtValueBytes((xbytesview){pureSignature,64});
    assert(state->seed&&state->publicKey&&state->message&&state->context&&state->digest&&state->pure);
    state->key=xl6CryptoEd25519CreateV1(state->seed);
    state->buffer=xl6BufferCreateV1();assert(state->key&&state->buffer);
    const XL6_CryptoEd25519KeyV1* key=state->key;
    assert(xrtEd25519SignMode(&key->expanded,XED25519_CONTEXT,"foo",3,NULL,0,state->ctxSignature));
    assert(xrtEd25519SignMode(&key->expanded,XED25519_PREHASH,"foo",3,digestData,64,state->phSignature));
    state->ctx=xrtValueBytes((xbytesview){state->ctxSignature,64});
    state->ph=xrtValueBytes((xbytesview){state->phSignature,64});assert(state->ctx&&state->ph);
}
static void dispose(State* state)
{
    xl6CryptoEd25519DestroyV1(state->key);xl6BufferDestroyV1(state->buffer);
    xrtValueRelease(state->seed);xrtValueRelease(state->publicKey);xrtValueRelease(state->message);
    xrtValueRelease(state->context);xrtValueRelease(state->digest);xrtValueRelease(state->pure);
    xrtValueRelease(state->ctx);xrtValueRelease(state->ph);
}
static bool fault(unsigned operation,size_t prefix)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state;initialize(&state);
    void* owned=(operation==1||operation==23)?xl6CryptoEd25519CreateV1(state.seed):NULL;
    assert((operation!=1&&operation!=23)||owned);
    xvalue* value=NULL;void* object=NULL;bool success=false;
    const uint8* expected=NULL;size_t size=0;assert(xrtMemDebugFailAfter(prefix));
    switch(operation) {
    case 0: /* Create */ object=xl6CryptoEd25519CreateV1(state.seed);success=object!=NULL;expected=NULL;size=0;break;
    case 1: /* Destroy */ xl6CryptoEd25519DestroyV1(owned);owned=NULL;success=true;expected=NULL;size=0;break;
    case 2: /* Public */ value=xl6CryptoEd25519PublicV1(state.key);success=value!=NULL;expected=publicData;size=32;break;
    case 3: /* ExportSeed */ value=xl6CryptoEd25519ExportSeedV1(state.key);success=value!=NULL;expected=seedData;size=32;break;
    case 4: /* Generate */ object=xl6CryptoEd25519GenerateV1();success=object!=NULL;expected=NULL;size=0;break;
    case 5: /* KeySignText */ value=xl6CryptoEd25519KeySignTextV1(state.key,((xstrview){"",0}),-1);success=value!=NULL;expected=pureSignature;size=64;break;
    case 6: /* KeySignContextText */ value=xl6CryptoEd25519KeySignContextTextV1(state.key,((xstrview){"",0}),state.context,-1);success=value!=NULL;expected=state.ctxSignature;size=64;break;
    case 7: /* SignText */ value=xl6CryptoEd25519SignTextV1(state.seed,((xstrview){"",0}),-1);success=value!=NULL;expected=pureSignature;size=64;break;
    case 8: /* VerifyText */ success=xl6CryptoEd25519VerifyTextV1(state.publicKey,((xstrview){"",0}),state.pure,-1);expected=NULL;size=0;break;
    case 9: /* VerifyContextText */ success=xl6CryptoEd25519VerifyContextTextV1(state.publicKey,((xstrview){"",0}),state.ctx,state.context,-1);expected=NULL;size=0;break;
    case 10: /* KeySignBytes */ value=xl6CryptoEd25519KeySignBytesV1(state.key,state.message,-1);success=value!=NULL;expected=pureSignature;size=64;break;
    case 11: /* KeySignContextBytes */ value=xl6CryptoEd25519KeySignContextBytesV1(state.key,state.message,state.context,-1);success=value!=NULL;expected=state.ctxSignature;size=64;break;
    case 12: /* SignBytes */ value=xl6CryptoEd25519SignBytesV1(state.seed,state.message,-1);success=value!=NULL;expected=pureSignature;size=64;break;
    case 13: /* VerifyBytes */ success=xl6CryptoEd25519VerifyBytesV1(state.publicKey,state.message,state.pure,-1);expected=NULL;size=0;break;
    case 14: /* VerifyContextBytes */ success=xl6CryptoEd25519VerifyContextBytesV1(state.publicKey,state.message,state.ctx,state.context,-1);expected=NULL;size=0;break;
    case 15: /* KeySignPrehash */ value=xl6CryptoEd25519KeySignPrehashV1(state.key,state.digest,state.context);success=value!=NULL;expected=state.phSignature;size=64;break;
    case 16: /* KeySignBuffer */ value=xl6CryptoEd25519KeySignBufferV1(state.key,state.buffer,-1);success=value!=NULL;expected=pureSignature;size=64;break;
    case 17: /* KeySignContextBuffer */ value=xl6CryptoEd25519KeySignContextBufferV1(state.key,state.buffer,state.context,-1);success=value!=NULL;expected=state.ctxSignature;size=64;break;
    case 18: /* SignBuffer */ value=xl6CryptoEd25519SignBufferV1(state.seed,state.buffer,-1);success=value!=NULL;expected=pureSignature;size=64;break;
    case 19: /* PublicFromSeed */ value=xl6CryptoEd25519PublicFromSeedV1(state.seed);success=value!=NULL;expected=publicData;size=32;break;
    case 20: /* VerifyPrehash */ success=xl6CryptoEd25519VerifyPrehashV1(state.publicKey,state.digest,state.ph,state.context);expected=NULL;size=0;break;
    case 21: /* VerifyBuffer */ success=xl6CryptoEd25519VerifyBufferV1(state.publicKey,state.buffer,state.pure,-1);expected=NULL;size=0;break;
    case 22: /* VerifyContextBuffer */ success=xl6CryptoEd25519VerifyContextBufferV1(state.publicKey,state.buffer,state.ctx,state.context,-1);expected=NULL;size=0;break;
    case 23:value=xl6NativeHandleTakeV1(&owned,xl6CryptoEd25519HandlePolicyV1Get());assert(!owned);success=value!=NULL;break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++prefixes;
    if(hit){assert(!success&&!object&&!value&&xrtGetError());++failures;}else assert(success);
    xerror* error=xrtTakeError();
    if(value&&expected)equalBytes(value,expected,size);
    xrtValueRelease(value);xl6CryptoEd25519DestroyV1(object);xl6CryptoEd25519DestroyV1(owned);
    value=xl6CryptoEd25519PublicV1(state.key);equalBytes(value,publicData,32);xrtValueRelease(value);
    value=xl6CryptoEd25519ExportSeedV1(state.key);equalBytes(value,seedData,32);xrtValueRelease(value);
    dispose(&state);xrtErrorFree(error);xrtClearError();balanced(before);return hit;
}
static void leaves(bool reverse)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state;initialize(&state);
    const XL6_OwnershipDomainV1* domains[2];
    domains[reverse?1:0]=xl6OwnershipDomainCryptoEd25519V1Get();
    domains[reverse?0:1]=xl6OwnershipDomainBufferV1Get();
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    xvalue* values[2];
    values[0]=xl6NativeHandleTakeV1(&state.key,xl6CryptoEd25519HandlePolicyV1Get());
    values[1]=xl6NativeHandleTakeV1(&state.buffer,xl6BufferHandlePolicyV1Get());
    assert(values[0]&&values[1]&&!state.key&&!state.buffer);
    for(size_t i=0;i<2;++i) {
        xrtownershipref ref=xrtValueOwnership(values[i]);xrtownershipscope freeze={0};size_t count=0;
        assert(xrtOwnershipFreezeTryBegin(&freeze)&&ref.Ops->Count(ref.Data,&count)&&count==1);
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* finish=NULL;
        assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&prep,&finish)&&adapter&&!prep&&!finish);
        int token=0;assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
        assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xrtValueRelease(values[i]);++graphs;
    }
    xl6OwnershipCatalogDestroyV1(catalog);dispose(&state);balanced(before);
}
static xerror* metadata(xerrkind kind,const char* domain,int code)
{
    xerrordesc description={0};description.Kind=kind;description.Domain=domain;description.Code=code;
    description.Operation="ordinary-fixture";description.Message="typed error contract";
    xerror* error=xrtErrorBuild(&description);assert(error);return error;
}
static void finishCases(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const struct {xerrkind kind;const char* domain;int code;bool reject;} cases[]={
        {XERR_PROTOCOL,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE,true},
        {XERR_VALUE,"xrt.crypto",XCRYPTO_ERROR_KEY,true},
        {XERR_MEMORY,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE,false},
        {XERR_STATE,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE,false},
        {XERR_PROTOCOL,"ordinary.fixture",XCRYPTO_ERROR_SIGNATURE,false},
        {XERR_PROTOCOL,"xrt.crypto",999,false},
        {XERR_ARGUMENT,"xrt.crypto",XCRYPTO_ERROR_KEY,false}
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i) {
        xerror* error=metadata(cases[i].kind,cases[i].domain,cases[i].code);xrtSetErrorTake(error);
        assert(!xl6CryptoEd25519VerifyFinish(false,NULL));
        assert(cases[i].reject?!xrtGetError():xrtGetError()==error);xrtClearError();++metadataCases;
    }
    xerror* error=metadata(XERR_PROTOCOL,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE);xrtSetErrorTake(error);
    assert(xl6CryptoEd25519VerifyFinish(true,NULL)&&xrtGetError()==error);xrtClearError();++metadataCases;
    xerror* previous=metadata(XERR_STATE,"ordinary.first",7);
    xrtSetErrorTake(metadata(XERR_PROTOCOL,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE));
    assert(!xl6CryptoEd25519VerifyFinish(false,previous)&&xrtGetError()==previous);xrtClearError();++metadataCases;
    previous=metadata(XERR_STATE,"ordinary.first",7);
    xrtSetErrorTake(metadata(XERR_MEMORY,"ordinary.resource",9));
    assert(!xl6CryptoEd25519VerifyFinish(false,previous)&&xrtGetError()==previous);xrtClearError();++metadataCases;
    xrtSetErrorTake(metadata(XERR_PROTOCOL,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE));
    assert(xrtMemDebugFailAfter(0));assert(!xl6CryptoEd25519VerifyFinish(false,NULL)&&!xrtGetError());
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();++metadataCases;balanced(before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    void* warm=xl6CryptoEd25519GenerateV1();assert(warm);xl6CryptoEd25519DestroyV1(warm);
    for(unsigned operation=0;operation<24;++operation) {
        size_t prefix=0;while(prefix<32&&fault(operation,prefix))++prefix;assert(prefix<32);
    }
    leaves(false);leaves(true);finishCases();
    xvalue* seed=xrtValueBytes((xbytesview){seedData,32});assert(seed);
    void* key=xl6CryptoEd25519CreateV1(seed);assert(key);
    xrtSetErrorKind(XERR_RANGE);const xerror* first=xrtGetError();xl6CryptoEd25519DestroyV1(key);
    assert(xrtGetError()==first);xrtClearError();xrtValueRelease(seed);
    printf("managed Ed25519: %zu allocation prefixes, %zu triggered failures, %zu real leaf/catalog graphs, %zu typed error boundary cases\n",prefixes,failures,graphs,metadataCases);
    assert(prefixes>=40&&failures>=20&&graphs==4&&metadataCases==11&&!xrtGetError());return 0;
}
