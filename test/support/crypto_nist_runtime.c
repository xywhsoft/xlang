/* Offline fixed public data, native allocation prefixes and real leaf ownership. */
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_crypto_nist_verify_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8 p256Private[]={0xC9,0xAF,0xA9,0xD8,0x45,0xBA,0x75,0x16,0x6B,0x5C,0x21,0x57,0x67,0xB1,0xD6,0x93,0x4E,0x50,0xC3,0xDB,0x36,0xE8,0x9B,0x12,0x7B,0x8A,0x62,0x2B,0x12,0x0F,0x67,0x21};
static const uint8 p256Public[]={0x04,0x60,0xFE,0xD4,0xBA,0x25,0x5A,0x9D,0x31,0xC9,0x61,0xEB,0x74,0xC6,0x35,0x6D,0x68,0xC0,0x49,0xB8,0x92,0x3B,0x61,0xFA,0x6C,0xE6,0x69,0x62,0x2E,0x60,0xF2,0x9F,0xB6,0x79,0x03,0xFE,0x10,0x08,0xB8,0xBC,0x99,0xA4,0x1A,0xE9,0xE9,0x56,0x28,0xBC,0x64,0xF2,0xF1,0xB2,0x0C,0x2D,0x7E,0x9F,0x51,0x77,0xA3,0xC2,0x94,0xD4,0x46,0x22,0x99};
static const uint8 p256Signature[]={0xEF,0xD4,0x8B,0x2A,0xAC,0xB6,0xA8,0xFD,0x11,0x40,0xDD,0x9C,0xD4,0x5E,0x81,0xD6,0x9D,0x2C,0x87,0x7B,0x56,0xAA,0xF9,0x91,0xC3,0x4D,0x0E,0xA8,0x4E,0xAF,0x37,0x16,0x08,0x34,0xE3,0x6A,0xD2,0x9A,0x83,0xBF,0x2B,0xC9,0x38,0x5E,0x49,0x1D,0x60,0x99,0xC8,0xFD,0xF9,0xD1,0xED,0x67,0xAA,0x7E,0xA5,0xF5,0x1F,0x93,0x78,0x28,0x57,0xA9};
static const uint8 p256Der[]={0x30,0x45,0x02,0x21,0x00,0xEF,0xD4,0x8B,0x2A,0xAC,0xB6,0xA8,0xFD,0x11,0x40,0xDD,0x9C,0xD4,0x5E,0x81,0xD6,0x9D,0x2C,0x87,0x7B,0x56,0xAA,0xF9,0x91,0xC3,0x4D,0x0E,0xA8,0x4E,0xAF,0x37,0x16,0x02,0x20,0x08,0x34,0xE3,0x6A,0xD2,0x9A,0x83,0xBF,0x2B,0xC9,0x38,0x5E,0x49,0x1D,0x60,0x99,0xC8,0xFD,0xF9,0xD1,0xED,0x67,0xAA,0x7E,0xA5,0xF5,0x1F,0x93,0x78,0x28,0x57,0xA9};
static const uint8 p384Private[]={0x6B,0x9D,0x3D,0xAD,0x2E,0x1B,0x8C,0x1C,0x05,0xB1,0x98,0x75,0xB6,0x65,0x9F,0x4D,0xE2,0x3C,0x3B,0x66,0x7B,0xF2,0x97,0xBA,0x9A,0xA4,0x77,0x40,0x78,0x71,0x37,0xD8,0x96,0xD5,0x72,0x4E,0x4C,0x70,0xA8,0x25,0xF8,0x72,0xC9,0xEA,0x60,0xD2,0xED,0xF5};
static const uint8 p384Public[]={0x04,0xEC,0x3A,0x4E,0x41,0x5B,0x4E,0x19,0xA4,0x56,0x86,0x18,0x02,0x9F,0x42,0x7F,0xA5,0xDA,0x9A,0x8B,0xC4,0xAE,0x92,0xE0,0x2E,0x06,0xAA,0xE5,0x28,0x6B,0x30,0x0C,0x64,0xDE,0xF8,0xF0,0xEA,0x90,0x55,0x86,0x60,0x64,0xA2,0x54,0x51,0x54,0x80,0xBC,0x13,0x80,0x15,0xD9,0xB7,0x2D,0x7D,0x57,0x24,0x4E,0xA8,0xEF,0x9A,0xC0,0xC6,0x21,0x89,0x67,0x08,0xA5,0x93,0x67,0xF9,0xDF,0xB9,0xF5,0x4C,0xA8,0x4B,0x3F,0x1C,0x9D,0xB1,0x28,0x8B,0x23,0x1C,0x3A,0xE0,0xD4,0xFE,0x73,0x44,0xFD,0x25,0x33,0x26,0x47,0x20};
static const uint8 p384Signature[]={0x94,0xED,0xBB,0x92,0xA5,0xEC,0xB8,0xAA,0xD4,0x73,0x6E,0x56,0xC6,0x91,0x91,0x6B,0x3F,0x88,0x14,0x06,0x66,0xCE,0x9F,0xA7,0x3D,0x64,0xC4,0xEA,0x95,0xAD,0x13,0x3C,0x81,0xA6,0x48,0x15,0x2E,0x44,0xAC,0xF9,0x6E,0x36,0xDD,0x1E,0x80,0xFA,0xBE,0x46,0x66,0x10,0xB5,0x14,0xEA,0x0E,0x87,0x31,0x5E,0x01,0xBF,0x24,0xD9,0xFC,0xEC,0x70,0xEC,0xF1,0x8B,0xF5,0xE6,0x9D,0xBA,0xD9,0xA7,0x27,0xEA,0x30,0x23,0x93,0x84,0x8F,0xB4,0xF0,0x4C,0x6C,0xD0,0x42,0x3F,0xDC,0x71,0x69,0x52,0x50,0x94,0x62,0x9E,0xAB};
static const uint8 p384Der[]={0x30,0x65,0x02,0x31,0x00,0x94,0xED,0xBB,0x92,0xA5,0xEC,0xB8,0xAA,0xD4,0x73,0x6E,0x56,0xC6,0x91,0x91,0x6B,0x3F,0x88,0x14,0x06,0x66,0xCE,0x9F,0xA7,0x3D,0x64,0xC4,0xEA,0x95,0xAD,0x13,0x3C,0x81,0xA6,0x48,0x15,0x2E,0x44,0xAC,0xF9,0x6E,0x36,0xDD,0x1E,0x80,0xFA,0xBE,0x46,0x02,0x30,0x66,0x10,0xB5,0x14,0xEA,0x0E,0x87,0x31,0x5E,0x01,0xBF,0x24,0xD9,0xFC,0xEC,0x70,0xEC,0xF1,0x8B,0xF5,0xE6,0x9D,0xBA,0xD9,0xA7,0x27,0xEA,0x30,0x23,0x93,0x84,0x8F,0xB4,0xF0,0x4C,0x6C,0xD0,0x42,0x3F,0xDC,0x71,0x69,0x52,0x50,0x94,0x62,0x9E,0xAB};
typedef struct State {
    xvalue *privateKey[2],*publicKey[2],*digest[2],*signature[2],*der[2],*one[2];
    void* key[2];
    uint8 shared[2][48],twice[2][97];
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
    uint8 digest0[32],one0[32]={0},two0[32]={0};one0[31]=1;two0[31]=2;
    assert(xrtSha256("sample",6,digest0));
    state->privateKey[0]=xrtValueBytes((xbytesview){p256Private,sizeof(p256Private)});
    state->publicKey[0]=xrtValueBytes((xbytesview){p256Public,sizeof(p256Public)});
    state->digest[0]=xrtValueBytes((xbytesview){digest0,sizeof(digest0)});
    state->signature[0]=xrtValueBytes((xbytesview){p256Signature,sizeof(p256Signature)});
    state->der[0]=xrtValueBytes((xbytesview){p256Der,sizeof(p256Der)});
    state->one[0]=xrtValueBytes((xbytesview){one0,sizeof(one0)});
    assert(state->privateKey[0]&&state->publicKey[0]&&state->digest[0]&&state->signature[0]&&state->der[0]&&state->one[0]);
    state->key[0]=xl6CryptoP256CreateV1(state->privateKey[0]);assert(state->key[0]);
    assert(xrtP256Shared(p256Private,p256Public,state->shared[0]));
    assert(xrtP256Multiply(two0,p256Public,state->twice[0]));
    uint8 digest1[48],one1[48]={0},two1[48]={0};one1[47]=1;two1[47]=2;
    assert(xrtSha384("sample",6,digest1));
    state->privateKey[1]=xrtValueBytes((xbytesview){p384Private,sizeof(p384Private)});
    state->publicKey[1]=xrtValueBytes((xbytesview){p384Public,sizeof(p384Public)});
    state->digest[1]=xrtValueBytes((xbytesview){digest1,sizeof(digest1)});
    state->signature[1]=xrtValueBytes((xbytesview){p384Signature,sizeof(p384Signature)});
    state->der[1]=xrtValueBytes((xbytesview){p384Der,sizeof(p384Der)});
    state->one[1]=xrtValueBytes((xbytesview){one1,sizeof(one1)});
    assert(state->privateKey[1]&&state->publicKey[1]&&state->digest[1]&&state->signature[1]&&state->der[1]&&state->one[1]);
    state->key[1]=xl6CryptoP384CreateV1(state->privateKey[1]);assert(state->key[1]);
    assert(xrtP384Shared(p384Private,p384Public,state->shared[1]));
    assert(xrtP384Multiply(two1,p384Public,state->twice[1]));
}
static void dispose(State* state)
{
    xl6CryptoP256DestroyV1(state->key[0]);xl6CryptoP384DestroyV1(state->key[1]);
    for(unsigned i=0;i<2;++i) {xrtValueRelease(state->privateKey[i]);xrtValueRelease(state->publicKey[i]);xrtValueRelease(state->digest[i]);xrtValueRelease(state->signature[i]);xrtValueRelease(state->der[i]);xrtValueRelease(state->one[i]);}
}
static bool fault(unsigned operation,size_t prefix)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state;initialize(&state);
    void* owned=NULL;
    if(operation==1||operation==32)owned=xl6CryptoP256CreateV1(state.privateKey[0]);
    if(operation==15||operation==33)owned=xl6CryptoP384CreateV1(state.privateKey[1]);
    assert((operation!=1&&operation!=15&&operation<32)||owned);
    xvalue* value=NULL;void* object=NULL;bool success=false;
    const uint8* expected=NULL;size_t size=0;
    assert(xrtMemDebugFailAfter(prefix));
    switch(operation) {
    case 0: /* xl6CryptoP256CreateV1 */ object=xl6CryptoP256CreateV1(state.privateKey[0]);success=object!=NULL;break;
    case 1: /* xl6CryptoP256DestroyV1 */ xl6CryptoP256DestroyV1(owned);owned=NULL;success=true;break;
    case 2: /* xl6CryptoP256PublicV1 */ value=xl6CryptoP256PublicV1(state.key[0]);success=value!=NULL;expected=p256Public;size=65;break;
    case 3: /* xl6CryptoP256ExportPrivateV1 */ value=xl6CryptoP256ExportPrivateV1(state.key[0]);success=value!=NULL;expected=p256Private;size=32;break;
    case 4: /* xl6CryptoP256GenerateV1 */ object=xl6CryptoP256GenerateV1();success=object!=NULL;break;
    case 5: /* xl6CryptoP256SharedV1 */ value=xl6CryptoP256SharedV1(state.key[0],state.publicKey[0]);success=value!=NULL;expected=state.shared[0];size=32;break;
    case 6: /* xl6CryptoP256SignHashV1 */ value=xl6CryptoP256SignHashV1(state.key[0],state.digest[0],3);success=value!=NULL;expected=p256Signature;size=64;break;
    case 7: /* xl6CryptoP256SignHashDerV1 */ value=xl6CryptoP256SignHashDerV1(state.key[0],state.digest[0],3);success=value!=NULL;expected=p256Der;size=71;break;
    case 8: /* xl6CryptoP256PublicFromPrivateV1 */ value=xl6CryptoP256PublicFromPrivateV1(state.privateKey[0]);success=value!=NULL;expected=p256Public;size=65;break;
    case 9: /* xl6CryptoP256VerifyHashV1 */ success=xl6CryptoP256VerifyHashV1(state.publicKey[0],state.digest[0],state.signature[0]);break;
    case 10: /* xl6CryptoP256VerifyHashDerV1 */ success=xl6CryptoP256VerifyHashDerV1(state.publicKey[0],state.digest[0],state.der[0]);break;
    case 11: /* xl6CryptoP256ValidV1 */ success=xl6CryptoP256ValidV1(state.publicKey[0]);break;
    case 12: /* xl6CryptoP256MultiplyV1 */ value=xl6CryptoP256MultiplyV1(state.one[0],state.publicKey[0]);success=value!=NULL;expected=p256Public;size=65;break;
    case 13: /* xl6CryptoP256AddV1 */ value=xl6CryptoP256AddV1(state.publicKey[0],state.publicKey[0]);success=value!=NULL;expected=state.twice[0];size=65;break;
    case 14: /* xl6CryptoP384CreateV1 */ object=xl6CryptoP384CreateV1(state.privateKey[1]);success=object!=NULL;break;
    case 15: /* xl6CryptoP384DestroyV1 */ xl6CryptoP384DestroyV1(owned);owned=NULL;success=true;break;
    case 16: /* xl6CryptoP384PublicV1 */ value=xl6CryptoP384PublicV1(state.key[1]);success=value!=NULL;expected=p384Public;size=97;break;
    case 17: /* xl6CryptoP384ExportPrivateV1 */ value=xl6CryptoP384ExportPrivateV1(state.key[1]);success=value!=NULL;expected=p384Private;size=48;break;
    case 18: /* xl6CryptoP384GenerateV1 */ object=xl6CryptoP384GenerateV1();success=object!=NULL;break;
    case 19: /* xl6CryptoP384SharedV1 */ value=xl6CryptoP384SharedV1(state.key[1],state.publicKey[1]);success=value!=NULL;expected=state.shared[1];size=48;break;
    case 20: /* xl6CryptoP384SignHashV1 */ value=xl6CryptoP384SignHashV1(state.key[1],state.digest[1],4);success=value!=NULL;expected=p384Signature;size=96;break;
    case 21: /* xl6CryptoP384SignHashDerV1 */ value=xl6CryptoP384SignHashDerV1(state.key[1],state.digest[1],4);success=value!=NULL;expected=p384Der;size=103;break;
    case 22: /* xl6CryptoP384PublicFromPrivateV1 */ value=xl6CryptoP384PublicFromPrivateV1(state.privateKey[1]);success=value!=NULL;expected=p384Public;size=97;break;
    case 23: /* xl6CryptoP384VerifyHashV1 */ success=xl6CryptoP384VerifyHashV1(state.publicKey[1],state.digest[1],state.signature[1]);break;
    case 24: /* xl6CryptoP384VerifyHashDerV1 */ success=xl6CryptoP384VerifyHashDerV1(state.publicKey[1],state.digest[1],state.der[1]);break;
    case 25: /* xl6CryptoP384ValidV1 */ success=xl6CryptoP384ValidV1(state.publicKey[1]);break;
    case 26: /* xl6CryptoP384MultiplyV1 */ value=xl6CryptoP384MultiplyV1(state.one[1],state.publicKey[1]);success=value!=NULL;expected=p384Public;size=97;break;
    case 27: /* xl6CryptoP384AddV1 */ value=xl6CryptoP384AddV1(state.publicKey[1],state.publicKey[1]);success=value!=NULL;expected=state.twice[1];size=97;break;
    case 28: /* xl6CryptoEcdsaP256ToDerV1 */ value=xl6CryptoEcdsaP256ToDerV1(state.signature[0]);success=value!=NULL;expected=p256Der;size=71;break;
    case 29: /* xl6CryptoEcdsaP256FromDerV1 */ value=xl6CryptoEcdsaP256FromDerV1(state.der[0]);success=value!=NULL;expected=p256Signature;size=64;break;
    case 30: /* xl6CryptoEcdsaP384ToDerV1 */ value=xl6CryptoEcdsaP384ToDerV1(state.signature[1]);success=value!=NULL;expected=p384Der;size=103;break;
    case 31: /* xl6CryptoEcdsaP384FromDerV1 */ value=xl6CryptoEcdsaP384FromDerV1(state.der[1]);success=value!=NULL;expected=p384Signature;size=96;break;
    case 32:value=xl6NativeHandleTakeV1(&owned,xl6CryptoP256HandlePolicyV1Get());assert(!owned);success=value!=NULL;break;
    case 33:value=xl6NativeHandleTakeV1(&owned,xl6CryptoP384HandlePolicyV1Get());assert(!owned);success=value!=NULL;break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++prefixes;
    if(hit){assert(!success&&!object&&!value&&xrtGetError());++failures;}else assert(success);
    xerror* error=xrtTakeError();
    if(value&&expected)equalBytes(value,expected,size);
    xrtValueRelease(value);
    if((operation>=14&&operation<28)||operation==33) {xl6CryptoP384DestroyV1(object);xl6CryptoP384DestroyV1(owned);}
    else {xl6CryptoP256DestroyV1(object);xl6CryptoP256DestroyV1(owned);}
    value=xl6CryptoP256PublicV1(state.key[0]);equalBytes(value,p256Public,sizeof(p256Public));xrtValueRelease(value);
    value=xl6CryptoP384PublicV1(state.key[1]);equalBytes(value,p384Public,sizeof(p384Public));xrtValueRelease(value);
    dispose(&state);xrtErrorFree(error);xrtClearError();balanced(before);return hit;
}
static void leaves(bool reverse)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);State state;initialize(&state);
    const XL6_OwnershipDomainV1* domains[2];
    domains[reverse?1:0]=xl6OwnershipDomainCryptoP256V1Get();
    domains[reverse?0:1]=xl6OwnershipDomainCryptoP384V1Get();
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    xvalue* values[2]={xl6NativeHandleTakeV1(&state.key[0],xl6CryptoP256HandlePolicyV1Get()),xl6NativeHandleTakeV1(&state.key[1],xl6CryptoP384HandlePolicyV1Get())};
    assert(values[0]&&values[1]&&!state.key[0]&&!state.key[1]);
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
        assert(!xl6CryptoNistVerifyFinish(false,NULL));
        assert(cases[i].reject?!xrtGetError():xrtGetError()==error);xrtClearError();++metadataCases;
    }
    xerror* error=metadata(XERR_PROTOCOL,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE);xrtSetErrorTake(error);
    assert(xl6CryptoNistVerifyFinish(true,NULL)&&xrtGetError()==error);xrtClearError();++metadataCases;
    xerror* previous=metadata(XERR_STATE,"ordinary.first",7);
    xrtSetErrorTake(metadata(XERR_PROTOCOL,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE));
    assert(!xl6CryptoNistVerifyFinish(false,previous)&&xrtGetError()==previous);xrtClearError();++metadataCases;
    previous=metadata(XERR_STATE,"ordinary.first",7);
    xrtSetErrorTake(metadata(XERR_MEMORY,"ordinary.resource",9));
    assert(!xl6CryptoNistVerifyFinish(false,previous)&&xrtGetError()==previous);xrtClearError();++metadataCases;
    xrtSetErrorTake(metadata(XERR_PROTOCOL,"xrt.crypto",XCRYPTO_ERROR_SIGNATURE));
    assert(xrtMemDebugFailAfter(0));assert(!xl6CryptoNistVerifyFinish(false,NULL)&&!xrtGetError());
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();++metadataCases;balanced(before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    void* warm=xl6CryptoP256GenerateV1();assert(warm);xl6CryptoP256DestroyV1(warm);
    warm=xl6CryptoP384GenerateV1();assert(warm);xl6CryptoP384DestroyV1(warm);
    for(unsigned operation=0;operation<34;++operation) {
        size_t prefix=0;while(prefix<32&&fault(operation,prefix))++prefix;assert(prefix<32);
    }
    leaves(false);leaves(true);finishCases();
    xvalue* keyBytes=xrtValueBytes((xbytesview){p256Private,sizeof(p256Private)});assert(keyBytes);
    void* key=xl6CryptoP256CreateV1(keyBytes);assert(key);xrtSetErrorKind(XERR_RANGE);
    const xerror* first=xrtGetError();xl6CryptoP256DestroyV1(key);assert(xrtGetError()==first);
    xrtClearError();xrtValueRelease(keyBytes);
    printf("managed NIST: %zu allocation prefixes, %zu triggered failures, %zu real leaf/catalog graphs, %zu typed error boundary cases\n",prefixes,failures,graphs,metadataCases);
    assert(prefixes>=50&&failures>=20&&graphs==4&&metadataCases==11&&!xrtGetError());return 0;
}
