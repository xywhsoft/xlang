/* Offline normal profiles, ownership and ordinary parameter/OOM contracts. */
#include "src/runtime/xl6_x509_profiles_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "x509_profile_vectors.h"
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
static void bytesEqualAt(xvalue* raw,xbytesview expected,unsigned line)
{
    xbytesview actual;assert(raw && xrtValueGetBytes(raw,&actual));
    if(actual.Size!=expected.Size || (actual.Size && memcmp(actual.Data,expected.Data,actual.Size)))
        fprintf(stderr,"field mismatch at line %u: actual %zu bytes, expected %zu bytes\n",line,actual.Size,expected.Size);
    equal(actual,expected);xrtValueRelease(raw);
}
#define bytesEqual(Raw,...) bytesEqualAt(Raw,(__VA_ARGS__),__LINE__)
static void textEqual(xstrview value,const char* text)
{assert(value.Data && value.Size==strlen(text) && !memcmp(value.Data,text,value.Size) && !value.Data[value.Size]);xl6X509FreeV1((void*)value.Data);}
static xvalue* input(xbytesview view){xvalue* value=xrtValueBytes(view);assert(value);return value;}
static void* certificate(xbytesview der)
{xvalue* value=input(der);void* result=xl6X509CertificateFromDerV1(value,0);xrtValueRelease(value);assert(result);return result;}
#define FAULTS(Call,Drop) do { \
    for(size_t prefix=0;prefix<64;++prefix) { \
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix)); \
        void* output=(void*)(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets; \
        assert(hit?(!output && xrtGetError()):(output && !xrtGetError()));if(hit)++failures; \
        xrtClearError();Drop(output);balanced(before);if(!hit)break;assert(prefix<63); \
    } \
}while(0)
static void publish(void* raw,const XL6_NativeHandlePolicyV1* policy,const XL6_OwnershipDomainV1* domain,size_t prefix,bool* hit)
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
        publish(Call,xl6X509##Part##HandlePolicyV1Get(),xl6OwnershipDomainX509##Part##V1Get(),prefix,&hit); \
        balanced(before);if(!hit)break;assert(prefix<31); \
    } \
}while(0)
static void check_names(void* names)
{
    assert(names && xl6X509GeneralNamesCountV1(names)==8);bytesEqual(xl6X509GeneralNamesToDerV1(names),expected_san_view);
    FAULTS(xl6X509GeneralNamesToDerV1(names),xrtValueRelease);FAULTS(xl6X509GeneralNamesAtV1(names,0),xl6X509GeneralNameDestroyV1);
    for(size_t i=0;i<8;++i) {
        void* item=xl6X509GeneralNamesAtV1(names,(int64_t)i);assert(item);
        assert(xl6X509GeneralNameKindV1(item)==generalNames[i].kind && !xl6X509GeneralNameConstraintBaseV1(item));
        bytesEqual(xl6X509GeneralNameToDerV1(item),generalNames[i].raw);bytesEqual(xl6X509GeneralNameValueV1(item),generalNames[i].value);
        FAULTS(xl6X509GeneralNameToDerV1(item),xrtValueRelease);FAULTS(xl6X509GeneralNameValueV1(item),xrtValueRelease);
        if(i>=1 && i<=3) {
            const char* texts[]={"person@sub.example.invalid","service.example.invalid","https://service.example.invalid/item"};
            textEqual(xl6X509GeneralNameTextV1(item),texts[i-1]);FAULTS(xl6X509GeneralNameTextV1(item).Data,xl6X509FreeV1);
        }
        if(i==6) {
            FAULTS(xl6X509GeneralNameDirectoryV1(item,0),xl6X509NameDestroyV1);
            void* directory=xl6X509GeneralNameDirectoryV1(item,0);xl6X509GeneralNameDestroyV1(item);item=NULL;
            bytesEqual(xl6X509NameToDerV1(directory),expected_directory_view);xl6X509NameDestroyV1(directory);
        }
        if(i==7){textEqual(xl6X509GeneralNameOidV1(item),"1.2.3.4");FAULTS(xl6X509GeneralNameOidV1(item).Data,xl6X509FreeV1);}
        xl6X509GeneralNameDestroyV1(item);
    }
    assert(!xl6X509GeneralNamesAtV1(names,-1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
}
static void check_optional(void* cert,bool leaf)
{
    void* san=xl6X509CertificateSubjectAltNamesV1(cert,0);
    assert(leaf?(san && xl6X509GeneralNamesCountV1(san)==2):!san);xl6X509GeneralNamesDestroyV1(san);
    assert(!xl6X509CertificateIssuerAltNamesV1(cert,0) && !xl6X509CertificateKeyUsageV1(cert));
    assert(!xl6X509CertificateExtendedKeyUsageV1(cert,0));
    xvalue* absent=xl6X509CertificateSubjectKeyIdV1(cert);assert(absent && xrtValueType(absent)==XVALUE_NULL);xrtValueRelease(absent);
    assert(!xl6X509CertificateAuthorityKeyIdV1(cert) && !xl6X509CertificateCrlPointsV1(cert,0));
    assert(!xl6X509CertificateFreshestCrlV1(cert,0) && !xl6X509CertificateNameConstraintsV1(cert));
    void* basic=xl6X509CertificateBasicConstraintsV1(cert);
    if(leaf) {
        assert(basic && !xl6X509BasicConstraintsCaV1(basic) && !xl6X509BasicConstraintsHasPathLimitV1(basic));
        assert(!xl6X509BasicConstraintsPathLimitV1(basic) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    } else assert(!basic);
    xl6X509BasicConstraintsDestroyV1(basic);assert(!xrtGetError());
}
static void check(void)
{
    void* cert=certificate(fixture_profile_view);void* leaf=certificate(fixture_leaf_view);void* sparse=certificate(fixture_sparse_view);
    check_optional(leaf,true);check_optional(sparse,false);xl6X509CertificateDestroyV1(sparse);
    void* names=xl6X509CertificateSubjectAltNamesV1(cert,0);void* ian=xl6X509CertificateIssuerAltNamesV1(cert,0);
    void* usage=xl6X509CertificateKeyUsageV1(cert);void* basic=xl6X509CertificateBasicConstraintsV1(cert);
    void* oids=xl6X509CertificateExtendedKeyUsageV1(cert,0);void* aki=xl6X509CertificateAuthorityKeyIdV1(cert);
    void* points=xl6X509CertificateCrlPointsV1(cert,0);void* freshest=xl6X509CertificateFreshestCrlV1(cert,0);
    void* constraints=xl6X509CertificateNameConstraintsV1(cert);
    assert(names && ian && usage && basic && oids && aki && points && freshest && constraints && !xrtGetError());
    bytesEqual(xl6X509CertificateSubjectKeyIdV1(cert),expected_ski_view);
    FAULTS(xl6X509CertificateSubjectAltNamesV1(cert,0),xl6X509GeneralNamesDestroyV1);FAULTS(xl6X509CertificateIssuerAltNamesV1(cert,0),xl6X509GeneralNamesDestroyV1);
    FAULTS(xl6X509CertificateKeyUsageV1(cert),xl6X509KeyUsageDestroyV1);FAULTS(xl6X509CertificateBasicConstraintsV1(cert),xl6X509BasicConstraintsDestroyV1);
    FAULTS(xl6X509CertificateExtendedKeyUsageV1(cert,0),xl6X509OidsDestroyV1);FAULTS(xl6X509CertificateSubjectKeyIdV1(cert),xrtValueRelease);
    FAULTS(xl6X509CertificateAuthorityKeyIdV1(cert),xl6X509AuthorityKeyIdentifierDestroyV1);
    FAULTS(xl6X509CertificateCrlPointsV1(cert,0),xl6X509DistributionPointsDestroyV1);FAULTS(xl6X509CertificateFreshestCrlV1(cert,0),xl6X509DistributionPointsDestroyV1);
    FAULTS(xl6X509CertificateNameConstraintsV1(cert),xl6X509NameConstraintsDestroyV1);
    PUBLISH(xl6X509CertificateSubjectAltNamesV1(cert,0),GeneralNames);PUBLISH(xl6X509GeneralNamesAtV1(names,0),GeneralName);
    PUBLISH(xl6X509CertificateKeyUsageV1(cert),KeyUsage);PUBLISH(xl6X509CertificateBasicConstraintsV1(cert),BasicConstraints);
    PUBLISH(xl6X509CertificateExtendedKeyUsageV1(cert,0),Oids);PUBLISH(xl6X509CertificateAuthorityKeyIdV1(cert),AuthorityKeyIdentifier);
    PUBLISH(xl6X509CertificateCrlPointsV1(cert,0),DistributionPoints);PUBLISH(xl6X509DistributionPointsAtV1(points,0),DistributionPoint);
    PUBLISH(xl6X509CertificateNameConstraintsV1(cert),NameConstraints);
    xl6X509CertificateDestroyV1(cert);
    check_names(names);bytesEqual(xl6X509GeneralNamesToDerV1(ian),expected_ian_view);xl6X509GeneralNamesDestroyV1(ian);
    assert(xl6X509KeyUsageFlagsV1(usage)==497);xl6X509KeyUsageDestroyV1(usage);
    assert(xl6X509BasicConstraintsCaV1(basic) && xl6X509BasicConstraintsHasPathLimitV1(basic) && !xl6X509BasicConstraintsPathLimitV1(basic));xl6X509BasicConstraintsDestroyV1(basic);
    assert(xl6X509OidsCountV1(oids)==3);bytesEqual(xl6X509OidsToDerV1(oids),expected_eku_view);
    static const uint8 oid1[]={43,6,1,5,5,7,3,1},oid2[]={43,6,1,5,5,7,3,2},oid3[]={42,3,4};
    const xbytesview expectedOids[]={{oid1,sizeof(oid1)},{oid2,sizeof(oid2)},{oid3,sizeof(oid3)}};
    const char* oidTexts[]={"1.3.6.1.5.5.7.3.1","1.3.6.1.5.5.7.3.2","1.2.3.4"};
    for(size_t i=0;i<3;++i){bytesEqual(xl6X509OidsAtV1(oids,(int64_t)i),expectedOids[i]);textEqual(xl6X509OidsOidV1(oids,(int64_t)i),oidTexts[i]);}
    xvalue* id=input(expectedOids[2]);assert(xl6X509OidsContainsOidV1(oids,id) && xl6X509OidsContainsV1(oids,(xstrview){"1.2.3.4",7}));
    assert(!xl6X509OidsContainsV1(oids,(xstrview){"1.2.3.99",8}) && !xrtGetError());xrtValueRelease(id);
    FAULTS(xl6X509OidsToDerV1(oids),xrtValueRelease);FAULTS(xl6X509OidsAtV1(oids,0),xrtValueRelease);FAULTS(xl6X509OidsOidV1(oids,0).Data,xl6X509FreeV1);
    assert(xl6X509AuthorityKeyIdentifierHasKeyIdV1(aki) && xl6X509AuthorityKeyIdentifierHasIssuerV1(aki) && xl6X509AuthorityKeyIdentifierHasSerialV1(aki));
    bytesEqual(xl6X509AuthorityKeyIdentifierToDerV1(aki),expected_aki_view);bytesEqual(xl6X509AuthorityKeyIdentifierKeyIdV1(aki),expected_ski_view);
    bytesEqual(xl6X509AuthorityKeyIdentifierSerialV1(aki),expected_aki_serial_view);
    FAULTS(xl6X509AuthorityKeyIdentifierToDerV1(aki),xrtValueRelease);FAULTS(xl6X509AuthorityKeyIdentifierKeyIdV1(aki),xrtValueRelease);FAULTS(xl6X509AuthorityKeyIdentifierSerialV1(aki),xrtValueRelease);
    FAULTS(xl6X509AuthorityKeyIdentifierIssuerV1(aki,0),xl6X509GeneralNamesDestroyV1);
    void* issuer=xl6X509AuthorityKeyIdentifierIssuerV1(aki,0);xl6X509AuthorityKeyIdentifierDestroyV1(aki);
    bytesEqual(xl6X509GeneralNamesToDerV1(issuer),expected_aki_issuer_view);xl6X509GeneralNamesDestroyV1(issuer);
    assert(xl6X509DistributionPointsCountV1(points)==2);bytesEqual(xl6X509DistributionPointsToDerV1(points),expected_points_view);
    bytesEqual(xl6X509DistributionPointsToDerV1(freshest),expected_freshest_view);xl6X509DistributionPointsDestroyV1(freshest);
    FAULTS(xl6X509DistributionPointsToDerV1(points),xrtValueRelease);FAULTS(xl6X509DistributionPointsAtV1(points,0),xl6X509DistributionPointDestroyV1);
    void* first=xl6X509DistributionPointsAtV1(points,0);void* relative=xl6X509DistributionPointsAtV1(points,1);xl6X509DistributionPointsDestroyV1(points);
    assert(xl6X509DistributionPointHasNameV1(first) && xl6X509DistributionPointNameKindV1(first)==0 && xl6X509DistributionPointHasReasonsV1(first));
    assert(xl6X509DistributionPointReasonsV1(first)==6 && xl6X509DistributionPointHasIssuerV1(first));
    bytesEqual(xl6X509DistributionPointToDerV1(first),expected_point0_view);bytesEqual(xl6X509DistributionPointNameDerV1(first),expected_name0_view);
    FAULTS(xl6X509DistributionPointToDerV1(first),xrtValueRelease);FAULTS(xl6X509DistributionPointNameDerV1(first),xrtValueRelease);
    FAULTS(xl6X509DistributionPointFullNamesV1(first,0),xl6X509GeneralNamesDestroyV1);FAULTS(xl6X509DistributionPointIssuerV1(first,0),xl6X509GeneralNamesDestroyV1);
    void* fullnames=xl6X509DistributionPointFullNamesV1(first,0);issuer=xl6X509DistributionPointIssuerV1(first,0);xl6X509DistributionPointDestroyV1(first);
    bytesEqual(xl6X509GeneralNamesToDerV1(fullnames),expected_fullnames_view);bytesEqual(xl6X509GeneralNamesToDerV1(issuer),expected_point_issuer_view);
    xl6X509GeneralNamesDestroyV1(fullnames);xl6X509GeneralNamesDestroyV1(issuer);
    assert(xl6X509DistributionPointNameKindV1(relative)==1 && !xl6X509DistributionPointHasReasonsV1(relative) && !xl6X509DistributionPointHasIssuerV1(relative));
    assert(!xl6X509DistributionPointFullNamesV1(relative,0) && !xl6X509DistributionPointIssuerV1(relative,0) && !xrtGetError());
    bytesEqual(xl6X509DistributionPointNameDerV1(relative),expected_name1_view);bytesEqual(xl6X509DistributionPointRelativeNameV1(relative),expected_relative_view);
    FAULTS(xl6X509DistributionPointRelativeNameV1(relative),xrtValueRelease);xl6X509DistributionPointDestroyV1(relative);
    assert(xl6X509NameConstraintsHasPermittedV1(constraints) && xl6X509NameConstraintsHasExcludedV1(constraints));
    bytesEqual(xl6X509NameConstraintsToDerV1(constraints),expected_constraints_view);
    FAULTS(xl6X509NameConstraintsToDerV1(constraints),xrtValueRelease);FAULTS(xl6X509NameConstraintsPermittedV1(constraints,0),xl6X509GeneralSubtreesDestroyV1);FAULTS(xl6X509NameConstraintsExcludedV1(constraints,0),xl6X509GeneralSubtreesDestroyV1);
    xl6X509NameConstraintsCheckV1(constraints,leaf);assert(!xrtGetError());xl6X509CertificateDestroyV1(leaf);
    PUBLISH(xl6X509NameConstraintsPermittedV1(constraints,0),GeneralSubtrees);
    void* permitted=xl6X509NameConstraintsPermittedV1(constraints,0);void* excluded=xl6X509NameConstraintsExcludedV1(constraints,0);xl6X509NameConstraintsDestroyV1(constraints);
    assert(xl6X509GeneralSubtreesCountV1(permitted)==3 && xl6X509GeneralSubtreesCountV1(excluded)==1);
    bytesEqual(xl6X509GeneralSubtreesToDerV1(permitted),expected_permitted_view);bytesEqual(xl6X509GeneralSubtreesToDerV1(excluded),expected_excluded_view);
    FAULTS(xl6X509GeneralSubtreesToDerV1(permitted),xrtValueRelease);FAULTS(xl6X509GeneralSubtreesAtV1(permitted,0),xl6X509GeneralSubtreeDestroyV1);
    PUBLISH(xl6X509GeneralSubtreesAtV1(permitted,0),GeneralSubtree);
    void* subtree=xl6X509GeneralSubtreesAtV1(permitted,0);xl6X509GeneralSubtreesDestroyV1(permitted);xl6X509GeneralSubtreesDestroyV1(excluded);
    assert(!xl6X509GeneralSubtreeHasMinimumV1(subtree) && !xl6X509GeneralSubtreeHasMaximumV1(subtree));
    bytesEqual(xl6X509GeneralSubtreeMinimumV1(subtree),(xbytesview){0});bytesEqual(xl6X509GeneralSubtreeMaximumV1(subtree),(xbytesview){0});
    FAULTS(xl6X509GeneralSubtreeToDerV1(subtree),xrtValueRelease);FAULTS(xl6X509GeneralSubtreeBaseV1(subtree),xl6X509GeneralNameDestroyV1);
    FAULTS(xl6X509GeneralSubtreeMinimumV1(subtree),xrtValueRelease);FAULTS(xl6X509GeneralSubtreeMaximumV1(subtree),xrtValueRelease);
    void* base=xl6X509GeneralSubtreeBaseV1(subtree);xl6X509GeneralSubtreeDestroyV1(subtree);
    void* dns=xl6X509GeneralNamesAtV1(names,2);xl6X509GeneralNamesDestroyV1(names);
    assert(xl6X509GeneralNameConstraintBaseV1(base) && xl6X509GeneralNameWithinV1(dns,base) && !xrtGetError());
    xl6X509GeneralNameDestroyV1(base);xl6X509GeneralNameDestroyV1(dns);xl6X509OidsDestroyV1(oids);
}
static void constructors(void)
{
    xbytesview encoded[8];
    encoded[0]=expected_san_view;encoded[1]=expected_eku_view;encoded[2]=expected_points_view;encoded[3]=expected_permitted_view;
    encoded[4]=expected_aki_view;encoded[5]=expected_point0_view;encoded[6]=expected_constraints_view;encoded[7]=expected_distances_view;
    for(size_t i=0;i<8;++i) {
        xvalue* value=input(encoded[i]);
        switch(i) {
        #define LIST_CASE(Index,Part) case Index: FAULTS(xl6X509##Part##FromDerV1(value,0,0),xl6X509##Part##DestroyV1);break
        LIST_CASE(0,GeneralNames);LIST_CASE(1,Oids);LIST_CASE(2,DistributionPoints);LIST_CASE(3,GeneralSubtrees);
        #undef LIST_CASE
        #define RECORD_CASE(Index,Part) case Index: FAULTS(xl6X509##Part##FromDerV1(value,0),xl6X509##Part##DestroyV1);break
        RECORD_CASE(4,AuthorityKeyIdentifier);RECORD_CASE(5,DistributionPoint);RECORD_CASE(6,NameConstraints);RECORD_CASE(7,GeneralSubtree);
        #undef RECORD_CASE
        }
        xrtValueRelease(value);
    }
    xvalue* value=input(expected_distances_view);void* subtree=xl6X509GeneralSubtreeFromDerV1(value,0);assert(subtree);xrtValueRelease(value);
    static const uint8 minimum[]={128},maximum[]={1,0};
    assert(xl6X509GeneralSubtreeHasMinimumV1(subtree) && xl6X509GeneralSubtreeHasMaximumV1(subtree));
    bytesEqual(xl6X509GeneralSubtreeMinimumV1(subtree),(xbytesview){minimum,1});bytesEqual(xl6X509GeneralSubtreeMaximumV1(subtree),(xbytesview){maximum,2});
    xl6X509GeneralSubtreeDestroyV1(subtree);
    value=input(expected_san_view);
    assert(!xl6X509GeneralNamesFromDerV1(value,0,1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509GeneralNamesFromDerV1(value,-1,0) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();xrtValueRelease(value);
}
int main(void)
{
    assert(!xrtGetError());xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    check();constructors();balanced(before);
    printf("X509 profiles: 3 valid vectors, %zu allocation budgets, %zu failures, %zu publication budgets; balanced\n",budgets,failures,publications);return 0;
}
