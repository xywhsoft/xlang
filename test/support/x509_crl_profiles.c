/* Offline normal profiles, ownership and ordinary parameter/OOM contracts. */
#include "src/runtime/xl6_x509_crl_profiles_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include "x509_crl_vectors.h"
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

static void* crl(xbytesview der)
{xvalue* value=input(der);void* result=xl6X509CrlFromDerV1(value,0);xrtValueRelease(value);assert(result);return result;}
static void plain_entry(void* entry,int64_t version)
{
    assert(entry && xl6X509CrlEntryVersionV1(entry)==version);
    bytesEqual(xl6X509CrlEntryToDerV1(entry),expected_plain_view);
    bytesEqual(xl6X509CrlEntrySerialV1(entry),expected_plain_serial_view);
    assert(xl6X509CrlEntryRevokedAtV1(entry)==expected_revoked_at);
    bytesEqual(xl6X509CrlEntryExtensionsDerV1(entry),(xbytesview){0});
    void* extensions=xl6X509CrlEntryExtensionsV1(entry,0);assert(extensions && !xl6X509ExtensionsCountV1(extensions));
    xl6X509ExtensionsDestroyV1(extensions);
    assert(!xl6X509CrlEntryHasReasonV1(entry) && !xl6X509CrlEntryHasInvalidityDateV1(entry) && !xl6X509CrlEntryIssuerV1(entry,0));
    assert(!xl6X509CrlEntryReasonV1(entry) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(!xl6X509CrlEntryInvalidityDateV1(entry) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
}
static void check_lists(void)
{
    xbytesview encoded[4],lists[4];
    encoded[0]=fixture_full_view;encoded[1]=fixture_delta_view;encoded[2]=fixture_sparse_view;encoded[3]=fixture_v1_view;
    lists[0]=entries_full_view;lists[1]=entries_delta_view;lists[2]=entries_sparse_view;lists[3]=entries_v1_view;
    const uint64_t counts[]={2,1,0,1};
    for(size_t i=0;i<4;++i) {
        int64_t version=i==3?1:2;void* parent=crl(encoded[i]);
        FAULTS(xl6X509CrlEntriesV1(parent,0),xl6X509CrlEntriesDestroyV1);
        void* list=xl6X509CrlEntriesV1(parent,0);assert(list);xl6X509CrlDestroyV1(parent);
        assert(xl6X509CrlEntriesCountV1(list)==counts[i] && xl6X509CrlEntriesVersionV1(list)==version);
        bytesEqual(xl6X509CrlEntriesToDerV1(list),lists[i]);FAULTS(xl6X509CrlEntriesToDerV1(list),xrtValueRelease);
        if(counts[i]) {
            FAULTS(xl6X509CrlEntriesAtV1(list,(int64_t)counts[i]-1),xl6X509CrlEntryDestroyV1);
            void* entry=xl6X509CrlEntriesAtV1(list,(int64_t)counts[i]-1);xl6X509CrlEntriesDestroyV1(list);list=NULL;
            plain_entry(entry,version);xl6X509CrlEntryDestroyV1(entry);
        } else {
            assert(!xl6X509CrlEntriesAtV1(list,0) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
        }
        xl6X509CrlEntriesDestroyV1(list);
        xvalue* value=input(lists[i]);FAULTS(xl6X509CrlEntriesFromDerV1(value,version,0,0),xl6X509CrlEntriesDestroyV1);
        void* parsed=xl6X509CrlEntriesFromDerV1(value,version,0,0);xrtValueRelease(value);assert(parsed);
        assert(xl6X509CrlEntriesCountV1(parsed)==counts[i]);xl6X509CrlEntriesDestroyV1(parsed);
    }
}
static void check_entry(void)
{
    xvalue* value=input(expected_entry_view);
    FAULTS(xl6X509CrlEntryFromDerV1(value,2,0),xl6X509CrlEntryDestroyV1);
    PUBLISH(xl6X509CrlEntryFromDerV1(value,2,0),CrlEntry);
    void* entry=xl6X509CrlEntryFromDerV1(value,2,0);xrtValueRelease(value);assert(entry);
    bytesEqual(xl6X509CrlEntryToDerV1(entry),expected_entry_view);bytesEqual(xl6X509CrlEntrySerialV1(entry),expected_serial_view);
    bytesEqual(xl6X509CrlEntryExtensionsDerV1(entry),expected_extensions_view);
    assert(xl6X509CrlEntryRevokedAtV1(entry)==expected_revoked_at && xl6X509CrlEntryVersionV1(entry)==2);
    assert(xl6X509CrlEntryHasReasonV1(entry) && xl6X509CrlEntryReasonV1(entry)==X509_CRL_REASON_KEY_COMPROMISE);
    assert(xl6X509CrlEntryHasInvalidityDateV1(entry) && xl6X509CrlEntryInvalidityDateV1(entry)==expected_invalidity_time);
    FAULTS(xl6X509CrlEntryToDerV1(entry),xrtValueRelease);FAULTS(xl6X509CrlEntrySerialV1(entry),xrtValueRelease);
    FAULTS(xl6X509CrlEntryExtensionsDerV1(entry),xrtValueRelease);
    FAULTS(xl6X509CrlEntryExtensionsV1(entry,0),xl6X509ExtensionsDestroyV1);
    FAULTS(xl6X509CrlEntryIssuerV1(entry,0),xl6X509GeneralNamesDestroyV1);
    void* extensions=xl6X509CrlEntryExtensionsV1(entry,0);void* issuer=xl6X509CrlEntryIssuerV1(entry,0);
    xl6X509CrlEntryDestroyV1(entry);assert(extensions && xl6X509ExtensionsCountV1(extensions)==3);
    bytesEqual(xl6X509ExtensionsToDerV1(extensions),expected_extensions_view);xl6X509ExtensionsDestroyV1(extensions);
    bytesEqual(xl6X509GeneralNamesToDerV1(issuer),expected_issuer_view);xl6X509GeneralNamesDestroyV1(issuer);
}
static void check_issuing(void)
{
    xbytesview encoded[4];encoded[0]=expected_idp_view;encoded[1]=expected_relative_idp_view;encoded[2]=expected_indirect_idp_view;encoded[3]=expected_attribute_idp_view;
    for(size_t i=0;i<4;++i) {
        xvalue* value=input(encoded[i]);FAULTS(xl6X509IssuingDistributionPointFromDerV1(value,0),xl6X509IssuingDistributionPointDestroyV1);
        PUBLISH(xl6X509IssuingDistributionPointFromDerV1(value,0),IssuingDistributionPoint);
        void* point=xl6X509IssuingDistributionPointFromDerV1(value,0);xrtValueRelease(value);assert(point);
        bytesEqual(xl6X509IssuingDistributionPointToDerV1(point),encoded[i]);FAULTS(xl6X509IssuingDistributionPointToDerV1(point),xrtValueRelease);
        assert(xl6X509IssuingDistributionPointHasNameV1(point)==(i<2));
        assert(xl6X509IssuingDistributionPointOnlyUsersV1(point)==(i==0));
        assert(xl6X509IssuingDistributionPointOnlyCasV1(point)==(i==1));
        assert(xl6X509IssuingDistributionPointOnlyAttributesV1(point)==(i==3));
        assert(xl6X509IssuingDistributionPointIndirectV1(point)==(i==0 || i==2) && xl6X509IssuingDistributionPointHasReasonsV1(point)==(i==0));
        assert(xl6X509IssuingDistributionPointReasonsV1(point)==(i==0?X509_CRL_REASON_FLAG_KEY_COMPROMISE:0));
        if(i==0) {
            assert(xl6X509IssuingDistributionPointNameKindV1(point)==X509_DISTRIBUTION_FULL_NAME);
            bytesEqual(xl6X509IssuingDistributionPointNameDerV1(point),expected_name_view);
            FAULTS(xl6X509IssuingDistributionPointFullNamesV1(point,0),xl6X509GeneralNamesDestroyV1);
            void* names=xl6X509IssuingDistributionPointFullNamesV1(point,0);xl6X509IssuingDistributionPointDestroyV1(point);point=NULL;
            bytesEqual(xl6X509GeneralNamesToDerV1(names),expected_fullnames_view);xl6X509GeneralNamesDestroyV1(names);
        } else {
            assert(!xl6X509IssuingDistributionPointFullNamesV1(point,0));
            if(i==1) {
                assert(xl6X509IssuingDistributionPointNameKindV1(point)==X509_DISTRIBUTION_RELATIVE_NAME);
                bytesEqual(xl6X509IssuingDistributionPointRelativeNameV1(point),expected_relative_view);
                FAULTS(xl6X509IssuingDistributionPointRelativeNameV1(point),xrtValueRelease);
            } else {
                bytesEqual(xl6X509IssuingDistributionPointNameDerV1(point),(xbytesview){0});
                assert(!xl6X509IssuingDistributionPointNameKindV1(point) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
                assert(!xl6X509IssuingDistributionPointRelativeNameV1(point) && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
            }
        }
        xl6X509IssuingDistributionPointDestroyV1(point);
    }
}
static void check_parent(void)
{
    void* full=crl(fixture_full_view);void* cert=certificate(expected_certificate_view);
    xvalue* serial=input(expected_serial_view);xvalue* missing=input((xbytesview){(const uint8*)"missing",7});
    FAULTS(xl6X509CrlFindSerialV1(full,serial),xl6X509CrlEntryDestroyV1);
    FAULTS(xl6X509CrlFindCertificateV1(full,cert),xl6X509CrlEntryDestroyV1);
    void* found=xl6X509CrlFindCertificateV1(full,cert);void* bySerial=xl6X509CrlFindSerialV1(full,serial);
    assert(found && bySerial && !xl6X509CrlFindSerialV1(full,missing) && !xrtGetError());
    xl6X509CertificateDestroyV1(cert);xrtValueRelease(serial);xrtValueRelease(missing);
    FAULTS(xl6X509CrlNumberV1(full),xrtValueRelease);bytesEqual(xl6X509CrlNumberV1(full),expected_number_value_view);
    xvalue* absent=xl6X509CrlDeltaBaseV1(full);assert(absent && xrtValueType(absent)==XVALUE_NULL);xrtValueRelease(absent);
    FAULTS(xl6X509CrlAuthorityKeyIdV1(full),xl6X509AuthorityKeyIdentifierDestroyV1);
    FAULTS(xl6X509CrlIssuingPointV1(full),xl6X509IssuingDistributionPointDestroyV1);
    FAULTS(xl6X509CrlFreshestV1(full,0),xl6X509DistributionPointsDestroyV1);
    void* aki=xl6X509CrlAuthorityKeyIdV1(full);void* idp=xl6X509CrlIssuingPointV1(full);void* fresh=xl6X509CrlFreshestV1(full,0);
    xl6X509CrlDestroyV1(full);
    bytesEqual(xl6X509CrlEntryToDerV1(found),expected_entry_view);bytesEqual(xl6X509CrlEntryToDerV1(bySerial),expected_entry_view);
    xl6X509CrlEntryDestroyV1(found);xl6X509CrlEntryDestroyV1(bySerial);
    bytesEqual(xl6X509AuthorityKeyIdentifierToDerV1(aki),expected_aki_view);xl6X509AuthorityKeyIdentifierDestroyV1(aki);
    bytesEqual(xl6X509IssuingDistributionPointToDerV1(idp),expected_idp_view);xl6X509IssuingDistributionPointDestroyV1(idp);
    bytesEqual(xl6X509DistributionPointsToDerV1(fresh),expected_fresh_view);xl6X509DistributionPointsDestroyV1(fresh);
    void* delta=crl(fixture_delta_view);static const uint8 number[]={1,0};
    bytesEqual(xl6X509CrlNumberV1(delta),(xbytesview){number,2});bytesEqual(xl6X509CrlDeltaBaseV1(delta),expected_number_value_view);
    assert(!xl6X509CrlFreshestV1(delta,0) && !xrtGetError());xl6X509CrlDestroyV1(delta);
    void* sparse=crl(fixture_sparse_view);absent=xl6X509CrlNumberV1(sparse);assert(absent && xrtValueType(absent)==XVALUE_NULL);xrtValueRelease(absent);
    absent=xl6X509CrlDeltaBaseV1(sparse);assert(absent && xrtValueType(absent)==XVALUE_NULL);xrtValueRelease(absent);
    assert(!xl6X509CrlAuthorityKeyIdV1(sparse) && !xl6X509CrlIssuingPointV1(sparse) && !xl6X509CrlFreshestV1(sparse,0));xl6X509CrlDestroyV1(sparse);
}
static void check_utilities(void)
{
    xvalue* value=input(expected_ski_view);FAULTS(xl6X509SubjectKeyIdParseV1(value,0),xrtValueRelease);
    bytesEqual(xl6X509SubjectKeyIdParseV1(value,0),expected_ski_value_view);xrtValueRelease(value);
    value=input(expected_number_view);FAULTS(xl6X509CrlNumberParseV1(value,0),xrtValueRelease);
    bytesEqual(xl6X509CrlNumberParseV1(value,0),expected_number_value_view);xrtValueRelease(value);
    value=input(expected_big_view);bytesEqual(xl6X509CrlNumberParseV1(value,0),expected_big_value_view);xrtValueRelease(value);
    static const uint8 zero[]={0};
    value=input(expected_zero_view);bytesEqual(xl6X509CrlNumberParseV1(value,0),(xbytesview){zero,1});xrtValueRelease(value);
    value=input(expected_reason_view);assert(xl6X509CrlReasonParseV1(value,0)==X509_CRL_REASON_KEY_COMPROMISE);xrtValueRelease(value);
    value=input(expected_invalidity_view);assert(xl6X509CrlInvalidityDateParseV1(value,0)==expected_invalidity_time);xrtValueRelease(value);
    value=input(expected_entry_view);
    assert(!xl6X509CrlEntryFromDerV1(value,2,1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6X509CrlEntryFromDerV1(value,3,0) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    assert(!xl6X509CrlEntryFromDerV1(value,2,-1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();xrtValueRelease(value);
    value=input(entries_full_view);PUBLISH(xl6X509CrlEntriesFromDerV1(value,2,0,0),CrlEntries);
    assert(!xl6X509CrlEntriesFromDerV1(value,2,0,1) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();xrtValueRelease(value);
}
int main(void)
{
    assert(!xrtGetError());xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    check_lists();check_entry();check_issuing();check_parent();check_utilities();balanced(before);assert(!xrtGetError());
    printf("X509 CRL profiles: 4 valid vectors, %zu allocation budgets, %zu failures, %zu publication budgets; balanced\n",budgets,failures,publications);return 0;
}
