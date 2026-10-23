/* Normal independently verified offline data, not a cryptographic campaign. */
#include "src/runtime/xl6_x509_records_internal.h"
#include "src/runtime/xl6_der_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "x509_record_vectors.h"
static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void equal(xbytesview a,xbytesview b)
{assert(a.Size==b.Size && (!a.Size || !memcmp(a.Data,b.Data,a.Size)));}
static void valueEqual(xvalue* value,xbytesview expected)
{xbytesview actual;assert(value && xrtValueGetBytes(value,&actual));equal(actual,expected);xrtValueRelease(value);}
static void textEqual(xstrview value,const char* text)
{assert(value.Data && value.Size==strlen(text) && !memcmp(value.Data,text,value.Size) && !value.Data[value.Size]);xl6X509FreeV1((void*)value.Data);}
/* Probe every allocating operation at all prefixes, including the final
 * untriggered success. No partially owned result may escape a failed call. */
#define FAULTS(Call,Drop) do { \
    for(size_t prefix=0;prefix<64;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix)); \
        void* output=(void*)(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets; \
        assert(hit?(!output && xrtGetError()):(output && !xrtGetError()));if(hit)++failures; \
        xrtClearError();Drop(output);balanced(before);if(!hit)break;assert(prefix<63); \
    } \
}while(0)
static void compareFaults(void* name,void* other)
{
    for(unsigned operation=0;operation<2;++operation)for(size_t prefix=0;prefix<128;++prefix) {
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix));
        bool equal=operation?xl6X509NameWithinV1(name,other):xl6X509NameEqualV1(name,other);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        assert(hit?(!equal && xrtGetError()):(equal && !xrtGetError()));if(hit)++failures;
        xrtClearError();balanced(before);if(!hit)break;assert(prefix<127);
    }
}
static void attribute(void* raw,const AttrOracle* oracle)
{
    assert(raw && !xrtGetError());valueEqual(xl6X509AttributeToDerV1(raw),oracle->raw);
    valueEqual(xl6X509AttributeOidContentV1(raw),oracle->oid);textEqual(xl6X509AttributeOidTextV1(raw),oracle->text);
    assert(xl6X509AttributeRdnV1(raw)==oracle->rdn);valueEqual(xl6X509AttributeValueDerV1(raw),oracle->value);
    void* value=xl6X509AttributeValueV1(raw);assert(value);xl6X509AttributeDestroyV1(raw);
    valueEqual(xl6DerRawV1(value),oracle->value);xl6DerValueDestroyV1(value);
}
static void extension(void* raw,const ExtOracle* oracle)
{
    assert(raw && !xrtGetError());valueEqual(xl6X509ExtensionToDerV1(raw),oracle->raw);
    valueEqual(xl6X509ExtensionOidContentV1(raw),oracle->oid);textEqual(xl6X509ExtensionOidTextV1(raw),oracle->text);
    assert(xl6X509ExtensionCriticalV1(raw)==oracle->critical);valueEqual(xl6X509ExtensionValueV1(raw),oracle->value);
    xl6X509ExtensionDestroyV1(raw);
}
static void algorithm(void* raw,const RecordVector* v)
{
    assert(raw);valueEqual(xl6X509AlgorithmToDerV1(raw),v->algorithm);
    xx509algorithm* parsed=&((XL6_X509AlgorithmV1*)raw)->value;
    valueEqual(xl6X509AlgorithmOidContentV1(raw),parsed->Oid);
    xstrview oid=xl6X509AlgorithmOidTextV1(raw);assert(oid.Data && oid.Size && !oid.Data[oid.Size]);xl6X509FreeV1((void*)oid.Data);
    assert(xl6X509AlgorithmHasParametersV1(raw)==parsed->HasParameters);
    valueEqual(xl6X509AlgorithmParametersV1(raw),parsed->Parameters);
    void* info=xl6X509AlgorithmSignatureInfoV1(raw);assert(info);xl6X509AlgorithmDestroyV1(raw);
    assert(xl6X509SignatureInfoKindV1(info)==v->kind && xl6X509SignatureInfoHashV1(info)==v->hash);
    assert(xl6X509SignatureInfoMaskHashV1(info)==v->mask && xl6X509SignatureInfoSaltBytesV1(info)==v->salt);
    assert(xl6X509SignatureInfoTrailerV1(info)==v->trailer);xl6X509SignatureInfoDestroyV1(info);
}
static void publication(void* raw,const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain,size_t prefix,bool* hit)
{
    assert(raw);assert(xrtMemDebugFailAfter(prefix));xvalue* value=xl6NativeHandleTakeV1(&raw,policy);
    *hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++publications;
    assert(!raw && (*hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
    if(value) {
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(&domain,1);assert(catalog);
        xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
        assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
        assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&finish) && adapter && !preparation && !finish);
        int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
        assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);
    }
    xrtValueRelease(value);assert(!xrtGetError());
}
#define PUBLISH(Call,Part) do { \
    for(size_t prefix=0;prefix<32;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);bool hit=false; \
        publication(Call,xl6X509##Part##HandlePolicyV1Get(),xl6OwnershipDomainX509##Part##V1Get(),prefix,&hit); \
        balanced(before);if(!hit)break;assert(prefix<31); \
    } \
}while(0)
static void check(const RecordVector* v)
{
    xvalue* data=xrtValueBytes(v->der);assert(data);
    void* parent=v->crl?xl6X509CrlFromDerV1(data,0):xl6X509CertificateFromDerV1(data,0);assert(parent);xrtValueRelease(data);
    void* name=v->crl?xl6X509CrlIssuerNameV1(parent,0):xl6X509CertificateSubjectNameV1(parent,0);
    void* issuer=v->crl?xl6X509CrlIssuerNameV1(parent,0):xl6X509CertificateIssuerNameV1(parent,0);
    void* exts=v->crl?xl6X509CrlExtensionListV1(parent,0):xl6X509CertificateExtensionListV1(parent,0);
    void* alg=v->crl?xl6X509CrlSignatureAlgorithmV1(parent):xl6X509CertificateSignatureAlgorithmV1(parent);
    void* tbsAlg=v->crl?xl6X509CrlTbsSignatureAlgorithmV1(parent):xl6X509CertificateTbsSignatureAlgorithmV1(parent);
    void* key=v->crl?NULL:xl6X509CertificateKeyInfoV1(parent);
    assert(name && issuer && exts && alg && tbsAlg && (v->crl || key));
    if(v->crl) {
        FAULTS(xl6X509CrlIssuerNameV1(parent,0),xl6X509NameDestroyV1);
        FAULTS(xl6X509CrlExtensionListV1(parent,0),xl6X509ExtensionsDestroyV1);
        FAULTS(xl6X509CrlSignatureAlgorithmV1(parent),xl6X509AlgorithmDestroyV1);
        FAULTS(xl6X509CrlTbsSignatureAlgorithmV1(parent),xl6X509AlgorithmDestroyV1);
        xl6X509CrlDestroyV1(parent);
    } else {
        assert(xl6X509CertificateHasIssuerUniqueIdV1(parent)==v->unique && xl6X509CertificateHasSubjectUniqueIdV1(parent)==v->unique);
        static const uint8 issuerId[]={0xa0},subjectId[]={0,88};
        valueEqual(xl6X509CertificateIssuerUniqueIdV1(parent),(xbytesview){v->unique?issuerId:NULL,v->unique?1:0});
        valueEqual(xl6X509CertificateSubjectUniqueIdV1(parent),(xbytesview){v->unique?subjectId:NULL,v->unique?2:0});
        assert(xl6X509CertificateIssuerUniqueUnusedV1(parent)==(v->unique?3:0) && xl6X509CertificateSubjectUniqueUnusedV1(parent)==0);
        FAULTS(xl6X509CertificateIssuerNameV1(parent,0),xl6X509NameDestroyV1);FAULTS(xl6X509CertificateSubjectNameV1(parent,0),xl6X509NameDestroyV1);
        FAULTS(xl6X509CertificateExtensionListV1(parent,0),xl6X509ExtensionsDestroyV1);
        FAULTS(xl6X509CertificateSignatureAlgorithmV1(parent),xl6X509AlgorithmDestroyV1);FAULTS(xl6X509CertificateTbsSignatureAlgorithmV1(parent),xl6X509AlgorithmDestroyV1);
        FAULTS(xl6X509CertificateKeyInfoV1(parent),xl6X509PublicKeyInfoDestroyV1);
        FAULTS(xl6X509CertificateIssuerUniqueIdV1(parent),xrtValueRelease);FAULTS(xl6X509CertificateSubjectUniqueIdV1(parent),xrtValueRelease);
        xl6X509CertificateDestroyV1(parent);
    }
    /* Every projection above must work after its certificate/CRL is gone. */
    valueEqual(xl6X509NameToDerV1(name),v->name);assert(xl6X509NameCountV1(name)==v->attrCount);
    assert(xl6X509NameEqualV1(name,issuer) && xl6X509NameWithinV1(name,issuer));compareFaults(name,issuer);xl6X509NameDestroyV1(issuer);
    void* normalized=xl6X509NameCopyV1(normalizedName,0);assert(normalized);
    compareFaults(name,normalized);xl6X509NameDestroyV1(normalized);
    for(size_t i=0;i<v->attrCount;++i) {
        const AttrOracle* a=&v->attrs[i];xvalue* oid=xrtValueBytes(a->oid);assert(oid);
        attribute(xl6X509NameAtV1(name,(int64_t)i),a);
        /* First duplicate OID is intentional; use the independent DER order. */
        size_t first=0;while(first<i && (v->attrs[first].oid.Size!=a->oid.Size || memcmp(v->attrs[first].oid.Data,a->oid.Data,a->oid.Size)))++first;
        attribute(xl6X509NameFindV1(name,(xstrview){a->text,strlen(a->text)}),&v->attrs[first]);
        attribute(xl6X509NameFindOidV1(name,oid),&v->attrs[first]);xrtValueRelease(oid);
    }
    void* a=xl6X509NameAtV1(name,0);assert(a);xvalue* oid=xl6X509AttributeOidContentV1(a);assert(oid);
    FAULTS(xl6X509NameToDerV1(name),xrtValueRelease);FAULTS(xl6X509NameAtV1(name,0),xl6X509AttributeDestroyV1);
    FAULTS(xl6X509NameFindV1(name,(xstrview){v->attrs[0].text,strlen(v->attrs[0].text)}),xl6X509AttributeDestroyV1);
    FAULTS(xl6X509NameFindOidV1(name,oid),xl6X509AttributeDestroyV1);
    FAULTS(xl6X509AttributeToDerV1(a),xrtValueRelease);FAULTS(xl6X509AttributeOidContentV1(a),xrtValueRelease);
    FAULTS(xl6X509AttributeOidTextV1(a).Data,xl6X509FreeV1);FAULTS(xl6X509AttributeValueDerV1(a),xrtValueRelease);FAULTS(xl6X509AttributeValueV1(a),xl6DerValueDestroyV1);
    PUBLISH(xl6X509NameCopyV1(v->name,0),Name);PUBLISH(xl6X509NameAtV1(name,0),Attribute);
    xrtValueRelease(oid);xl6X509AttributeDestroyV1(a);
    xvalue* nameDer=xrtValueBytes(v->name);assert(nameDer);
    FAULTS(xl6X509NameFromDerV1(nameDer,0,0),xl6X509NameDestroyV1);
    void* duplicate=xl6X509NameFromDerV1(nameDer,(int64_t)v->name.Size,(int64_t)v->attrCount);assert(duplicate);xl6X509NameDestroyV1(duplicate);
    assert(!xl6X509NameFromDerV1(nameDer,0,(int64_t)v->attrCount-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509NameFromDerV1(nameDer,(int64_t)v->name.Size-1,0) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509NameFromDerV1(nameDer,0,-1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();xrtValueRelease(nameDer);
    assert(!xl6X509NameAtV1(name,-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509NameAtV1(name,(int64_t)v->attrCount) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509NameFindV1(name,(xstrview){"1.2.3.99",8}) && !xrtGetError());
    void* child=xl6X509NameAtV1(name,0);xl6X509NameDestroyV1(name);attribute(child,&v->attrs[0]);
    valueEqual(xl6X509ExtensionsToDerV1(exts),v->extensions);assert(xl6X509ExtensionsCountV1(exts)==v->extCount);
    for(size_t i=0;i<v->extCount;++i) {
        const ExtOracle* e=&v->exts[i];xvalue* input=xrtValueBytes(e->oid);assert(input);
        extension(xl6X509ExtensionsAtV1(exts,(int64_t)i),e);
        extension(xl6X509ExtensionsFindV1(exts,(xstrview){e->text,strlen(e->text)}),e);extension(xl6X509ExtensionsFindOidV1(exts,input),e);
        xrtValueRelease(input);
    }
    void* e=xl6X509ExtensionsAtV1(exts,0);oid=xl6X509ExtensionOidContentV1(e);assert(e && oid);
    FAULTS(xl6X509ExtensionsToDerV1(exts),xrtValueRelease);FAULTS(xl6X509ExtensionsAtV1(exts,0),xl6X509ExtensionDestroyV1);
    FAULTS(xl6X509ExtensionsFindV1(exts,(xstrview){v->exts[0].text,strlen(v->exts[0].text)}),xl6X509ExtensionDestroyV1);
    FAULTS(xl6X509ExtensionsFindOidV1(exts,oid),xl6X509ExtensionDestroyV1);
    FAULTS(xl6X509ExtensionToDerV1(e),xrtValueRelease);FAULTS(xl6X509ExtensionOidContentV1(e),xrtValueRelease);
    FAULTS(xl6X509ExtensionOidTextV1(e).Data,xl6X509FreeV1);FAULTS(xl6X509ExtensionValueV1(e),xrtValueRelease);
    PUBLISH(xl6X509ExtensionsCopyV1(v->extensions,0),Extensions);PUBLISH(xl6X509ExtensionsAtV1(exts,0),Extension);
    xrtValueRelease(oid);xl6X509ExtensionDestroyV1(e);
    xvalue* extDer=xrtValueBytes(v->extensions);assert(extDer);FAULTS(xl6X509ExtensionsFromDerV1(extDer,0,0),xl6X509ExtensionsDestroyV1);
    duplicate=xl6X509ExtensionsFromDerV1(extDer,(int64_t)v->extensions.Size,(int64_t)v->extCount);assert(duplicate);xl6X509ExtensionsDestroyV1(duplicate);
    if(v->extCount>1){assert(!xl6X509ExtensionsFromDerV1(extDer,0,(int64_t)v->extCount-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();}
    assert(!xl6X509ExtensionsFromDerV1(extDer,0,-1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();xrtValueRelease(extDer);
    assert(!xl6X509ExtensionsAtV1(exts,(int64_t)v->extCount) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509ExtensionsFindV1(exts,(xstrview){"1.2.3.99",8}) && !xrtGetError());
    child=xl6X509ExtensionsAtV1(exts,0);xl6X509ExtensionsDestroyV1(exts);extension(child,&v->exts[0]);
    xvalue* algDer=xrtValueBytes(v->algorithm);assert(algDer);
    FAULTS(xl6X509AlgorithmFromDerV1(algDer,0),xl6X509AlgorithmDestroyV1);FAULTS(xl6X509AlgorithmToDerV1(alg),xrtValueRelease);
    FAULTS(xl6X509AlgorithmOidContentV1(alg),xrtValueRelease);FAULTS(xl6X509AlgorithmOidTextV1(alg).Data,xl6X509FreeV1);
    FAULTS(xl6X509AlgorithmParametersV1(alg),xrtValueRelease);FAULTS(xl6X509AlgorithmSignatureInfoV1(alg),xl6X509SignatureInfoDestroyV1);
    PUBLISH(xl6X509AlgorithmFromDerV1(algDer,0),Algorithm);PUBLISH(xl6X509AlgorithmSignatureInfoV1(alg),SignatureInfo);
    algorithm(alg,v);algorithm(tbsAlg,v);xrtValueRelease(algDer);
    if(key) {
        valueEqual(xl6X509PublicKeyInfoSpkiV1(key),v->spki);valueEqual(xl6X509PublicKeyInfoKeyBytesV1(key),v->key);
        valueEqual(xl6X509PublicKeyInfoModulusV1(key),v->modulus);valueEqual(xl6X509PublicKeyInfoExponentV1(key),v->exponent);
        assert(xl6X509PublicKeyInfoKindV1(key)==v->keyKind && xl6X509PublicKeyInfoCurveV1(key)==v->curve);
        FAULTS(xl6X509PublicKeyInfoSpkiV1(key),xrtValueRelease);FAULTS(xl6X509PublicKeyInfoKeyBytesV1(key),xrtValueRelease);
        FAULTS(xl6X509PublicKeyInfoModulusV1(key),xrtValueRelease);FAULTS(xl6X509PublicKeyInfoExponentV1(key),xrtValueRelease);
        FAULTS(xl6X509PublicKeyInfoAlgorithmV1(key),xl6X509AlgorithmDestroyV1);
        PUBLISH(xl6X509PublicKeyInfoCopyV1(((XL6_X509PublicKeyInfoV1*)key)->raw,&((XL6_X509PublicKeyInfoV1*)key)->value),PublicKeyInfo);
        child=xl6X509PublicKeyInfoAlgorithmV1(key);xl6X509PublicKeyInfoDestroyV1(key);
        assert(child);xvalue* encoded=xl6X509AlgorithmToDerV1(child);assert(encoded);xrtValueRelease(encoded);xl6X509AlgorithmDestroyV1(child);
    }
    assert(!xrtGetError());
}
static void ordinary_edges(void)
{
    static const uint8 unknown[]={48,5,6,3,42,3,4},ed448[]={48,5,6,3,43,101,113};
    xvalue* input=xrtValueBytes((xbytesview){unknown,sizeof(unknown)});assert(input);
    void* alg=xl6X509AlgorithmFromDerV1(input,0);assert(alg && !xl6X509AlgorithmSignatureInfoV1(alg) && !xrtGetError());
    xl6X509AlgorithmDestroyV1(alg);xrtValueRelease(input);
    input=xrtValueBytes((xbytesview){ed448,sizeof(ed448)});alg=xl6X509AlgorithmFromDerV1(input,0);assert(alg);
    void* info=xl6X509AlgorithmSignatureInfoV1(alg);assert(info && xl6X509SignatureInfoKindV1(info)==5);
    xl6X509SignatureInfoDestroyV1(info);xl6X509AlgorithmDestroyV1(alg);xrtValueRelease(input);
    input=xrtValueBytes((xbytesview){0});void* exts=xl6X509ExtensionsFromDerV1(input,0,0);assert(exts && !xl6X509ExtensionsCountV1(exts));
    valueEqual(xl6X509ExtensionsToDerV1(exts),(xbytesview){0});xl6X509ExtensionsDestroyV1(exts);xrtValueRelease(input);
    static const uint8 utc[]={23,13,'2','0','0','1','0','1','0','0','0','0','0','0','Z'};
    static const uint8 generalized[]={24,15,'2','0','2','0','0','1','0','1','0','0','0','0','0','0','Z'};
    const xbytesview times[]={{utc,sizeof(utc)},{generalized,sizeof(generalized)}};
    for(size_t i=0;i<2;++i){void* value=xl6DerCopyValue(times[i]);assert(value && xl6X509DerValueTimeV1(value)==1577836800000000LL);xl6DerValueDestroyV1(value);}
    static const uint8 oid[]={6,3,42,3,4};void* value=xl6DerCopyValue((xbytesview){oid,sizeof(oid)});assert(value);
    textEqual(xl6DerOidTextV1(value),"1.2.3.4");FAULTS(xl6DerOidTextV1(value).Data,xl6X509FreeV1);xl6DerValueDestroyV1(value);
    xbytesview output={0};void* items=NULL;
    assert(!xl6X509RecordAllocV1(SIZE_MAX,0,0,8,(xbytesview){0},&items,&output) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509RecordAllocV1(8,SIZE_MAX,8,8,(xbytesview){0},&items,&output) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509AlgorithmCopyV1(NULL) && xrtGetError());xrtClearError();
    static const uint8 one=0;output=(xbytesview){0};
    assert(!xl6X509RebaseV1((xbytesview){&one,0},(xbytesview){0},(xbytesview){&one,0},&output) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(!xl6X509IndexV1(0,1,NULL) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
}
int main(void)
{
    assert(!xrtGetError());xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<sizeof(recordVectors)/sizeof(*recordVectors);++i){check(&recordVectors[i]);balanced(before);}
    ordinary_edges();balanced(before);
    printf("X509 records: %zu valid vectors, %zu allocation budgets, %zu failures, %zu publication budgets; balanced\n",sizeof(recordVectors)/sizeof(*recordVectors),budgets,failures,publications);
    return 0;
}
