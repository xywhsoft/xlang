/* Fixed public vectors, normal format interoperability and allocation rollback.
 * No malformed key/point campaigns, traffic, signing or algorithm replacement. */
#include "src/runtime/xl6_key_format_internal.h"
#include <assert.h>
#include <stdio.h>
#include "key_vectors.h"

static size_t prefixes,failures;
static void same(xbytesview actual,xbytesview expected)
{assert(actual.Size==expected.Size && (!actual.Size || !memcmp(actual.Data,expected.Data,actual.Size)));}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void inner(const KeyVector* vector,xbytesview encoded,bool standalone)
{
    if (vector->algorithm==XL6_KEY_RSA_V1) {
        xbytesview parts[8];assert(xl6KeyRsaPrivateReadV1(encoded,parts));
        for(size_t i=0;i<8;++i)same(parts[i],vector->rsa[i]);
    } else {
        xbytesview secret,publicKey={0};
        if (vector->algorithm==XL6_KEY_P256_V1 || vector->algorithm==XL6_KEY_P384_V1) {
            assert(xl6KeySec1ReadV1(encoded,vector->algorithm,!standalone,&secret,&publicKey));
            if (publicKey.Data) same(publicKey,vector->publicKey);
        } else assert(xl6KeyCurvePrivateReadV1(encoded,vector->algorithm,&secret));
        same(secret,vector->privateKey);
    }
}
static void output(const char* directory,const KeyVector* vector,const char* extension,xbytesview data)
{
    char path[4096];int n=snprintf(path,sizeof(path),"%s/%s.%s",directory,vector->name,extension);
    assert(n>0 && (size_t)n<sizeof(path));
    FILE* file=fopen(path,"wb");assert(file);
    assert(fwrite(data.Data,1,data.Size,file)==data.Size && !fclose(file));
}
static bool writeInner(const KeyVector* vector,xbuffer* result)
{
    if (vector->algorithm==XL6_KEY_RSA_V1) return xl6KeyRsaPrivateWriteV1(vector->rsa,result);
    if (vector->algorithm==XL6_KEY_P256_V1 || vector->algorithm==XL6_KEY_P384_V1)
        return xl6KeySec1WriteV1(vector->privateKey,vector->publicKey,vector->algorithm,result);
    return xl6KeyCurvePrivateWriteV1(vector->privateKey,vector->algorithm,result);
}
static void ordinary(const KeyVector* vector,const char* directory)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xbytesview publicKey,secret;
    assert(xl6KeySpkiReadV1(vector->spki,vector->algorithm,&publicKey));same(publicKey,vector->publicKey);
    if (vector->algorithm==XL6_KEY_RSA_V1) {
        xbytesview parts[2];assert(xl6KeyRsaPublicReadV1(publicKey,parts));
        same(parts[0],vector->rsa[0]);same(parts[1],vector->rsa[1]);
    }
    assert(xl6KeyPkcs8ReadV1(vector->pkcs8,vector->algorithm,&secret,&publicKey));assert(!publicKey.Data);
    inner(vector,secret,false);inner(vector,vector->inner,true);

    xbuffer spki={0},encodedInner={0},pkcs8={0},withPublic={0};
    assert(xl6KeySpkiWriteV1(vector->publicKey,vector->algorithm,&spki));same(xrtBufferView(&spki),vector->spki);
    assert(writeInner(vector,&encodedInner));inner(vector,xrtBufferView(&encodedInner),true);
    assert(xl6KeyPkcs8WriteV1(xrtBufferView(&encodedInner),(xbytesview){0},vector->algorithm,&pkcs8));
    assert(xl6KeyPkcs8WriteV1(xrtBufferView(&encodedInner),vector->publicKey,vector->algorithm,&withPublic));
    assert(xl6KeyPkcs8ReadV1(xrtBufferView(&withPublic),vector->algorithm,&secret,&publicKey));
    same(publicKey,vector->publicKey);inner(vector,secret,false);
    output(directory,vector,"spki.der",xrtBufferView(&spki));
    output(directory,vector,"pkcs8.der",xrtBufferView(&pkcs8));
    output(directory,vector,"pkcs8-public.der",xrtBufferView(&withPublic));
    output(directory,vector,"inner.der",xrtBufferView(&encodedInner));

    str pem=NULL;size_t textSize;bytes decoded=NULL;size_t dataSize;
    assert(xl6KeyPemEncodeV1(xrtBufferView(&pkcs8),"PRIVATE KEY",&pem,&textSize));
    assert(xl6KeyPemDecodeV1((xstrview){pem,textSize},"PRIVATE KEY",&decoded,&dataSize));
    same((xbytesview){decoded,dataSize},xrtBufferView(&pkcs8));
    output(directory,vector,"pkcs8.pem",(xbytesview){(const uint8*)pem,textSize});
    xl6KeyFreeV1(decoded,dataSize?dataSize:1);xl6KeyFreeV1(pem,textSize+1);
    xl6KeyBufferFreeV1(&withPublic);xl6KeyBufferFreeV1(&pkcs8);
    xl6KeyBufferFreeV1(&encodedInner);xl6KeyBufferFreeV1(&spki);
    assert(!xrtGetError());balanced(before);
}
static bool fault(const KeyVector* vector,unsigned operation,size_t prefix)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xbuffer result={0};str text=NULL;bytes decoded=NULL;size_t size=0;
    assert(xrtBufferAppendByte(&result,0x55));
    assert(xrtMemDebugFailAfter(prefix));
    bool ok=false;
    switch (operation) {
    case 0:ok=xl6KeySpkiWriteV1(vector->publicKey,vector->algorithm,&result);break;
    case 1:ok=xl6KeyPkcs8WriteV1(vector->inner,(xbytesview){0},vector->algorithm,&result);break;
    case 2:ok=xl6KeyPkcs8WriteV1(vector->inner,vector->publicKey,vector->algorithm,&result);break;
    case 3:ok=writeInner(vector,&result);break;
    case 4:ok=xl6KeyPemEncodeV1(vector->pkcs8,"PRIVATE KEY",&text,&size);break;
    case 5:ok=xl6KeyPemDecodeV1(vector->pem,"PRIVATE KEY",&decoded,&size);break;
    default:assert(0);
    }
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++prefixes;
    if (hit) {assert(!ok && xrtGetError());++failures;}
    else assert(ok);
    if (!ok) assert(result.Size==1 && result.Data[0]==0x55 && !text && !decoded && !size);
    const xerror* error=xrtGetError();
    xl6KeyBufferFreeV1(&result);assert(xrtGetError()==error);
    xl6KeyFreeV1(text,text?size+1:0);xl6KeyFreeV1(decoded,decoded?(size?size:1):0);
    assert(xrtGetError()==error);xrtClearError();balanced(before);return hit;
}
static void rfc8410(void)
{
    /* RFC8410 sections 10.1/10.3: ordinary v0, plus v1 with an attribute.
     * The printed RFC example has one extra trailing '-' on its final line;
     * only that boundary typography is normalized here, not the DER payload. */
    static const char v0[]="-----BEGIN PRIVATE KEY-----\nMC4CAQAwBQYDK2VwBCIEINTuctv5E1hK1bbY8fdp+K06/nwoy/HU++CXqI9EdVhC\n-----END PRIVATE KEY-----\n";
    static const char v1[]="-----BEGIN PRIVATE KEY-----\nMHICAQEwBQYDK2VwBCIEINTuctv5E1hK1bbY8fdp+K06/nwoy/HU++CXqI9EdVhC\noB8wHQYKKoZIhvcNAQkJFDEPDA1DdXJkbGUgQ2hhaXJzgSEAGb9ECWmEzf6FQbrB\nZ9w7lshQhqowtrbLDFw4rXAxZuE=\n-----END PRIVATE KEY-----\n";
    static const char pub[]="-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAGb9ECWmEzf6FQbrBZ9w7lshQhqowtrbLDFw4rXAxZuE=\n-----END PUBLIC KEY-----\n";
    bytes raw[3]={0};size_t sizes[3];
    assert(xl6KeyPemDecodeV1((xstrview){v0,sizeof(v0)-1},"PRIVATE KEY",&raw[0],&sizes[0]));
    assert(xl6KeyPemDecodeV1((xstrview){v1,sizeof(v1)-1},"PRIVATE KEY",&raw[1],&sizes[1]));
    assert(xl6KeyPemDecodeV1((xstrview){pub,sizeof(pub)-1},"PUBLIC KEY",&raw[2],&sizes[2]));
    xbytesview private0,private1,public0,public1,expected;
    assert(xl6KeyPkcs8ReadV1((xbytesview){raw[0],sizes[0]},XL6_KEY_ED25519_V1,&private0,&public0));
    assert(xl6KeyPkcs8ReadV1((xbytesview){raw[1],sizes[1]},XL6_KEY_ED25519_V1,&private1,&public1));
    assert(!public0.Data);same(private0,private1);
    assert(xl6KeySpkiReadV1((xbytesview){raw[2],sizes[2]},XL6_KEY_ED25519_V1,&expected));same(public1,expected);
    for(size_t i=0;i<3;++i)xl6KeyFreeV1(raw[i],sizes[i]);
}
static void contracts(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const KeyVector* ed=&keyVectors[3];assert(ed->algorithm==XL6_KEY_ED25519_V1);
    /* Two valid algorithms share a 32-byte width. Explicit target identity,
     * never width guessing, must decide the envelope's interpretation. */
    xbytesview output={(const uint8*)"keep",4},second=output;
    assert(!xl6KeySpkiReadV1(ed->spki,XL6_KEY_X25519_V1,&output));
    assert(output.Size==4 && !memcmp(output.Data,"keep",4));
    assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);
    assert(!strcmp(xrtErrorDomain(xrtGetError()),"xlang.key") &&
        xrtErrorCode(xrtGetError())==XL6_KEY_FORMAT_ALGORITHM_V1);xrtClearError();
    assert(!xl6KeyPkcs8ReadV1(ed->pkcs8,XL6_KEY_X25519_V1,&output,&second));
    assert(output.Size==4 && second.Size==4 && xrtGetError());xrtClearError();
    assert(!xl6KeyPkcs8ReadV1(ed->pkcs8,ed->algorithm,&output,&output));
    assert(output.Size==4 && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    bytes decoded=NULL;size_t size=0;
    assert(!xl6KeyPemDecodeV1(ed->pem,"PUBLIC KEY",&decoded,&size));
    assert(!decoded && !size && xrtGetError());xrtClearError();
    xbuffer buffer={0};assert(xrtBufferAppendByte(&buffer,0x55));
    xerror* prior=xrtErrorCreate(XERR_STATE,"key-test",1,"prior operation");assert(prior);
    xrtSetErrorTake(prior);xl6KeyBufferFreeV1(&buffer);assert(xrtGetError()==prior);
    xrtClearError();balanced(before);
}
int main(int argc,char** argv)
{
    assert(argc==2 && !xrtGetError());
    rfc8410();contracts();
    for(size_t i=0;i<sizeof(keyVectors)/sizeof(keyVectors[0]);++i) {
        ordinary(&keyVectors[i],argv[1]);
        for(unsigned operation=0;operation<6;++operation) {
            size_t prefix=0;
            while(prefix<128 && fault(&keyVectors[i],operation,prefix))++prefix;
            assert(prefix<128);
        }
    }
    assert(!xrtGetError());
    printf("Key formats: six algorithms, RFC8410 v0/v1, SPKI/PKCS8/PKCS1/SEC1/PEM, %zu budgets, %zu failures; balanced\n",prefixes,failures);
    return 0;
}
