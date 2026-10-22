/* Normal public fixed vectors, every new native entry, allocation rollback and
 * actual pre-existing leaf policies. No malformed/security/network campaigns. */
#include "src/runtime/xl6_key_object_internal.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "key_vectors.h"
#include "key_alternate_public.h"

enum { RESULT_KEY,RESULT_BYTES,RESULT_TEXT };
enum { FORMAT_PUBLIC,FORMAT_SPKI,FORMAT_PKCS8,FORMAT_INNER,FORMAT_RSA_PUBLIC };
typedef struct State {
    unsigned family;const KeyVector* vector;void* raw;
    xvalue *spki,*pkcs8,*inner,*publicBytes;
    xstrview spkiPem,innerPem;
} State;
typedef struct Result {void* key;xvalue* bytes;xstrview text;} Result;
typedef struct Entry {const char* name;unsigned family,result,format;const char* label;bool flag;} Entry;
static State states[7];
static size_t budgets,failures,leaves;
static size_t mismatches,publicationBudgets;
static void equal(xbytesview actual,xbytesview expected)
{assert(actual.Size==expected.Size && (!actual.Size || !memcmp(actual.Data,expected.Data,actual.Size)));}
static void valueEqual(const xvalue* value,xbytesview expected)
{xbytesview actual;assert(value && xrtValueGetBytes(value,&actual));equal(actual,expected);}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void destroy(unsigned family,void* raw)
{
    switch(family) {
    case 0:xl6CryptoRsaPublicDestroyV1(raw);break;
    case 1:xl6CryptoRsaPrivateDestroyV1(raw);break;
    case 2:xl6CryptoX25519DestroyV1(raw);break;
    case 3:xl6CryptoX448DestroyV1(raw);break;
    case 4:xl6CryptoEd25519DestroyV1(raw);break;
    case 5:xl6CryptoP256DestroyV1(raw);break;
    case 6:xl6CryptoP384DestroyV1(raw);break;
    default:assert(0);
    }
}
static void* original(unsigned family)
{
    const KeyVector* vector=states[family].vector;
    if(family<2) {
        xvalue* parts[8]={0};void* raw;
        for(size_t i=0;i<(family?8:2);++i){parts[i]=xrtValueBytes(vector->rsa[i]);assert(parts[i]);}
        if(family)raw=xl6CryptoRsaPrivateCreateCrtV1(parts[0],parts[1],parts[2],parts[3],parts[4],parts[5],parts[6],parts[7]);
        else raw=xl6CryptoRsaPublicCreateV1(parts[0],parts[1]);
        for(size_t i=0;i<8;++i)xrtValueRelease(parts[i]);
        return raw;
    }
    xvalue* input=xrtValueBytes(vector->privateKey);assert(input);void* raw=NULL;
    switch(family) {
    case 2:raw=xl6CryptoX25519CreateV1(input);break;
    case 3:raw=xl6CryptoX448CreateV1(input);break;
    case 4:raw=xl6CryptoEd25519CreateV1(input);break;
    case 5:raw=xl6CryptoP256CreateV1(input);break;
    case 6:raw=xl6CryptoP384CreateV1(input);break;
    default:assert(0);
    }
    xrtValueRelease(input);return raw;
}
static void checkKey(unsigned family,void* raw)
{
    assert(raw && !xrtGetError());const KeyVector* vector=states[family].vector;
    if(family<2) {
        void* pub=family?xl6CryptoRsaPrivatePublicV1(raw):raw;assert(pub);
        xvalue* n=xl6CryptoRsaPublicModulusV1(pub);xvalue* e=xl6CryptoRsaPublicExponentV1(pub);
        valueEqual(n,vector->rsa[0]);valueEqual(e,vector->rsa[1]);xrtValueRelease(n);xrtValueRelease(e);
        if(family) {
            xl6CryptoRsaPublicDestroyV1(pub);
            for(int64_t part=0;part<6;++part){xvalue* value=xl6CryptoRsaPrivateExportPartV1(raw,part);valueEqual(value,vector->rsa[(size_t)part+2]);xrtValueRelease(value);}
        }
        return;
    }
    xvalue *secret=NULL,*pub=NULL;
    switch(family) {
    case 2:secret=xl6CryptoX25519ExportPrivateV1(raw);pub=xl6CryptoX25519PublicV1(raw);break;
    case 3:secret=xl6CryptoX448ExportPrivateV1(raw);pub=xl6CryptoX448PublicV1(raw);break;
    case 4:secret=xl6CryptoEd25519ExportSeedV1(raw);pub=xl6CryptoEd25519PublicV1(raw);break;
    case 5:secret=xl6CryptoP256ExportPrivateV1(raw);pub=xl6CryptoP256PublicV1(raw);break;
    case 6:secret=xl6CryptoP384ExportPrivateV1(raw);pub=xl6CryptoP384PublicV1(raw);break;
    default:assert(0);
    }
    valueEqual(secret,vector->privateKey);valueEqual(pub,vector->publicKey);xrtValueRelease(secret);xrtValueRelease(pub);
}
static void* fromPkcs8(unsigned family,const xvalue* encoded)
{
    switch(family) {
    case 1:return xl6CryptoRsaPrivateFromPkcs8V1(encoded);
    case 2:return xl6CryptoX25519FromPkcs8V1(encoded);
    case 3:return xl6CryptoX448FromPkcs8V1(encoded);
    case 4:return xl6CryptoEd25519FromPkcs8V1(encoded);
    case 5:return xl6CryptoP256FromPkcs8V1(encoded);
    case 6:return xl6CryptoP384FromPkcs8V1(encoded);
    default:assert(0);return NULL;
    }
}
static const XL6_NativeHandlePolicyV1* policy(unsigned family)
{
    switch(family) {
    case 0:return xl6CryptoRsaPublicHandlePolicyV1Get();case 1:return xl6CryptoRsaPrivateHandlePolicyV1Get();
    case 2:return xl6CryptoX25519HandlePolicyV1Get();case 3:return xl6CryptoX448HandlePolicyV1Get();
    case 4:return xl6CryptoEd25519HandlePolicyV1Get();case 5:return xl6CryptoP256HandlePolicyV1Get();
    case 6:return xl6CryptoP384HandlePolicyV1Get();default:assert(0);return NULL;
    }
}
/* This generated include has one typed call for each of the 74 declarations.
 * It performs no casts between incompatible native function pointer ABIs. */
#include "key_calls.inc"

static void initialize(void)
{
    const size_t vectors[]={0,0,4,5,3,1,2};
    for(unsigned family=0;family<7;++family) {
        State* state=&states[family];state->family=family;state->vector=&keyVectors[vectors[family]];
        const KeyVector* vector=state->vector;
        state->spki=xrtValueBytes(vector->spki);state->pkcs8=xrtValueBytes(vector->pkcs8);
        state->inner=xrtValueBytes(family?vector->inner:vector->publicKey);state->publicBytes=xrtValueBytes(vector->publicKey);
        assert(state->spki && state->pkcs8 && state->inner && state->publicBytes);
        str text=NULL;size_t size=0;
        assert(xl6KeyPemEncodeV1(vector->spki,"PUBLIC KEY",&text,&size));state->spkiPem=(xstrview){text,size};
        const char* label=family==0?"RSA PUBLIC KEY":family==1?"RSA PRIVATE KEY":"EC PRIVATE KEY";
        assert(xl6KeyPemEncodeV1(family?vector->inner:vector->publicKey,label,&text,&size));state->innerPem=(xstrview){text,size};
        state->raw=original(family);checkKey(family,state->raw);
    }
}
static void dispose(void)
{
    for(unsigned family=0;family<7;++family) {
        State* state=&states[family];destroy(family,state->raw);
        xrtValueRelease(state->spki);xrtValueRelease(state->pkcs8);xrtValueRelease(state->inner);xrtValueRelease(state->publicBytes);
        xl6KeyFreeV1((void*)state->spkiPem.Data,state->spkiPem.Size+1);
        xl6KeyFreeV1((void*)state->innerPem.Data,state->innerPem.Size+1);
    }
}
static void release(const Entry* entry,Result* result)
{
    destroy(entry->family,result->key);xrtValueRelease(result->bytes);
    xl6KeyFreeV1((void*)result->text.Data,result->text.Data?result->text.Size+1:0);
}
static void validate(const Entry* entry,Result* result)
{
    const KeyVector* vector=states[entry->family].vector;
    if(entry->result==RESULT_KEY){checkKey(entry->family,result->key);return;}
    xbytesview encoded;bytes temporary=NULL;size_t size=0;
    if(entry->result==RESULT_TEXT) {
        assert(result->text.Data && xl6KeyPemDecodeV1(result->text,entry->label,&temporary,&size));encoded=(xbytesview){temporary,size};
    } else assert(result->bytes && xrtValueGetBytes(result->bytes,&encoded));
    if(entry->format==FORMAT_PUBLIC)equal(encoded,vector->publicKey);
    else if(entry->format==FORMAT_SPKI)equal(encoded,vector->spki);
    else if(entry->format==FORMAT_RSA_PUBLIC)equal(encoded,vector->publicKey);
    else if(entry->format==FORMAT_PKCS8) {
        xvalue* input=xrtValueBytes(encoded);assert(input);void* raw=fromPkcs8(entry->family,input);
        checkKey(entry->family,raw);destroy(entry->family,raw);xrtValueRelease(input);
    } else {
        assert(entry->format==FORMAT_INNER);xvalue* input=xrtValueBytes(encoded);assert(input);void* raw=NULL;
        if(entry->family==1)raw=xl6CryptoRsaPrivateFromPkcs1V1(input);
        else if(entry->family==5)raw=xl6CryptoP256FromSec1V1(input);
        else if(entry->family==6)raw=xl6CryptoP384FromSec1V1(input);
        else assert(0);
        checkKey(entry->family,raw);destroy(entry->family,raw);xrtValueRelease(input);
    }
    xl6KeyFreeV1(temporary,size?size:1);
}
static bool fault(size_t index,size_t prefix)
{
    const Entry* entry=&entries[index];xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    Result result={0};assert(xrtMemDebugFailAfter(prefix));invoke(index,true,&result);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
    bool success=result.key || result.bytes || result.text.Data;
    if(hit){assert(!success && xrtGetError());++failures;}else{assert(success && !xrtGetError());validate(entry,&result);}
    xerror* error=xrtTakeError();release(entry,&result);xrtClearError();xrtSetErrorTake(error);
    if(hit)assert(xrtGetError());
    xrtClearError();balanced(before);return hit;
}
static void leaf(unsigned family)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* raw=family?fromPkcs8(family,states[family].pkcs8):xl6CryptoRsaPublicFromSpkiV1(states[family].spki);
    assert(raw);xvalue* value=xl6NativeHandleTakeV1(&raw,policy(family));assert(value && !raw);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainCryptoRsaPublicV1Get(),xl6OwnershipDomainCryptoRsaPrivateV1Get(),
        xl6OwnershipDomainCryptoX25519V1Get(),xl6OwnershipDomainCryptoX448V1Get(),xl6OwnershipDomainCryptoEd25519V1Get(),
        xl6OwnershipDomainCryptoP256V1Get(),xl6OwnershipDomainCryptoP384V1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,7);assert(catalog);
    xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count;
    assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&finish) && adapter && !preparation && !finish);
    int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xrtValueRelease(value);xl6OwnershipCatalogDestroyV1(catalog);
    balanced(before);++leaves;
}
static void publication(unsigned family)
{
    for(size_t prefix=0;prefix<32;++prefix) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void* raw=family?fromPkcs8(family,states[family].pkcs8):xl6CryptoRsaPublicFromSpkiV1(states[family].spki);
        assert(raw && !xrtGetError());assert(xrtMemDebugFailAfter(prefix));
        xvalue* value=xl6NativeHandleTakeV1(&raw,policy(family));
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publicationBudgets;
        assert(!raw && (hit?value==NULL && xrtGetError():value!=NULL && !xrtGetError()));
        xrtClearError();xrtValueRelease(value);assert(!xrtGetError());balanced(before);
        if(!hit)return;
    }
    assert(0);
}
static void mismatch(unsigned family)
{
    State* state=&states[family];size_t index=(size_t)(state->vector-keyVectors);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xbuffer encoded={0};assert(xl6KeyPkcs8WriteV1(state->vector->inner,alternatePublic[index],state->vector->algorithm,&encoded));
    xvalue* input=xrtValueBytes(xrtBufferView(&encoded));assert(input);
    assert(!fromPkcs8(family,input) && xrtGetError());xrtClearError();
    xrtValueRelease(input);xl6KeyBufferFreeV1(&encoded);++mismatches;
    if(family>=5) {
        assert(xl6KeySec1WriteV1(state->vector->privateKey,alternatePublic[index],state->vector->algorithm,&encoded));
        input=xrtValueBytes(xrtBufferView(&encoded));assert(input);
        void* raw=family==5?xl6CryptoP256FromSec1V1(input):xl6CryptoP384FromSec1V1(input);
        assert(!raw && xrtGetError());xrtClearError();xrtValueRelease(input);++mismatches;
        xbuffer outer={0};assert(xl6KeyPkcs8WriteV1(xrtBufferView(&encoded),state->vector->publicKey,state->vector->algorithm,&outer));
        input=xrtValueBytes(xrtBufferView(&outer));assert(input);
        assert(!fromPkcs8(family,input) && xrtGetError());xrtClearError();xrtValueRelease(input);++mismatches;
        xl6KeyBufferFreeV1(&outer);xl6KeyBufferFreeV1(&encoded);
    }
    balanced(before);
}
static void incompleteRsa(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xvalue* parts[3];
    for(size_t i=0;i<3;++i){parts[i]=xrtValueBytes(keyVectors[0].rsa[i]);assert(parts[i]);}
    void* raw=xl6CryptoRsaPrivateCreateV1(parts[0],parts[1],parts[2]);assert(raw);
    assert(!xl6CryptoRsaPrivateToPkcs1V1(raw) && xrtGetError());xrtClearError();
    assert(!xl6CryptoRsaPrivateToPkcs8V1(raw,true) && xrtGetError());xrtClearError();
    xstrview text=xl6CryptoRsaPrivateToPkcs8PemV1(raw,false);assert(!text.Data && !text.Size && xrtGetError());xrtClearError();
    xl6CryptoRsaPrivateDestroyV1(raw);for(size_t i=0;i<3;++i)xrtValueRelease(parts[i]);balanced(before);
}
static void writeResult(const char* directory,const Entry* entry,const Result* result)
{
    if(!directory)return;
    const char* suffix=NULL;
    if(strstr(entry->name,"ToPkcs8V1"))suffix="pkcs8.der";
    else if(strstr(entry->name,"ToPkcs8PemV1"))suffix="pkcs8.pem";
    else if(strstr(entry->name,"PublicSpkiV1") || !strcmp(entry->name,"xl6CryptoRsaPublicToSpkiV1"))suffix="spki.der";
    else if(strstr(entry->name,"ToSec1V1") || !strcmp(entry->name,"xl6CryptoRsaPrivateToPkcs1V1"))suffix="inner.der";
    if(!suffix)return;
    xbytesview data;
    if(entry->result==RESULT_TEXT)data=(xbytesview){(const uint8*)result->text.Data,result->text.Size};
    else assert(result->bytes && xrtValueGetBytes(result->bytes,&data));
    char path[2048];int length=snprintf(path,sizeof(path),"%s/%s.%s",directory,states[entry->family].vector->name,suffix);
    assert(length>0 && (size_t)length<sizeof(path));FILE* file=fopen(path,"wb");assert(file);
    assert(fwrite(data.Data,1,data.Size,file)==data.Size && !fclose(file));
}
int main(int argc,char** argv)
{
    assert(!xrtGetError());xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);initialize();
    assert(sizeof(entries)/sizeof(entries[0])==74);
    for(size_t index=0;index<74;++index) {
        const Entry* entry=&entries[index];Result result={0};invoke(index,false,&result);
        assert(!xrtGetError());validate(entry,&result);writeResult(argc>1?argv[1]:NULL,entry,&result);release(entry,&result);
        if(entry->flag){result=(Result){0};invoke(index,true,&result);validate(entry,&result);release(entry,&result);}
        size_t prefix=0;while(prefix<256 && fault(index,prefix))++prefix;assert(prefix<256);
        checkKey(entry->family,states[entry->family].raw);
    }
    for(unsigned family=0;family<7;++family){leaf(family);publication(family);if(family)mismatch(family);}
    incompleteRsa();assert(mismatches==10);
    dispose();assert(!xrtGetError());balanced(before);
    printf("Key objects: 74 entries, %zu budgets, %zu failures, %zu real leaves, %zu publication budgets, %zu public-key mismatches; balanced\n",
        budgets,failures,leaves,publicationBudgets,mismatches);return 0;
}
