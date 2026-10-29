#include "xl6_tls_parameters_internal.h"
#include "xl6_array_input.h"

/* Configuration is small and bounded by the protocol vocabulary. Each input
 * detaches independent values before inspection; no mutable array pin or item
 * pointer survives creation. XRT validates the atomic cross-list policy. */
static bool readList(const xvalue* values,int64_t* output,size_t capacity,size_t* count)
{
    XL6_ArrayInputV1 input={0};
    if(!xl6ArrayInputAcquireBoundedV1(values,capacity,&input))return false;
    bool ok=xl6TlsLimitV1(input.count,(int64_t)capacity,"too many TLS policy preferences");
    for(size_t i=0;ok && i<input.count;++i){
        ok=xrtValueGetInt(input.items[i],&output[i]);
        if(ok && (output[i]<0 || output[i]>UINT16_MAX))ok=xl6TlsFailV1(XERR_RANGE,"TLS wire preference exceeds uint16");
    }
    if(ok)*count=input.count;
    xl6ArrayInputReleaseV1(&input);return ok && !xrtGetError();
}
void* xl6TlsPolicyPreferencesV1(const xvalue* versions,const xvalue* ciphers,const xvalue* groups,const xvalue* signatures,int64_t keyShare)
{
    if(xrtGetError())return NULL;
    if(keyShare<XTLS_KEY_SHARE_PREFER_GROUP || keyShare>XTLS_KEY_SHARE_PREFER_READY){
        xl6TlsFailV1(XERR_RANGE,"invalid TLS key-share preference policy");return NULL;
    }
    xtlspolicy policy={0};int64_t values[14];
#define XL6_TLS_POLICY_LIST(Singular,Plural,Type,Limit) Type Plural[Limit]; \
    if(!readList(Singular##Input,values,Limit,&policy.Singular##Count))return NULL; \
    for(size_t i=0;i<policy.Singular##Count;++i)Plural[i]=(Type)values[i]; \
    policy.Plural=Plural;
    const xvalue* VersionInput=versions;const xvalue* CipherInput=ciphers;
    const xvalue* GroupInput=groups;const xvalue* SignatureInput=signatures;
#include "xl6_tls_policy_lists.inc"
#undef XL6_TLS_POLICY_LIST
    policy.KeySharePolicy=(xtlskeysharepolicy)keyShare;
    xtlscontextconfig config;xrtTlsContextConfigInit(&config);config.Policy=&policy;
    return xrtTlsContextCreate(&config);
}
