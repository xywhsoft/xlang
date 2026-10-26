#include "xl6_tls_revocation_internal.h"
#include "xl6_tls_internal.h"
#include "xl6_x509_path_internal.h"
static bool equal(xbytesview a,xbytesview b)
{return a.Size==b.Size&&(!a.Size||!memcmp(a.Data,b.Data,a.Size));}
static bool unknown(const XL6_TlsRevocationOptionsV1* options)
{return options->allowUnknown||xl6TlsFailV1(XERR_PERMISSION,"TLS revocation is unknown or has incomplete applicable coverage");}
static bool sourceBound(const XL6_TlsRevocationRequestV1* request,const XL6_X509ValidatedCrlV1* source,
    const XL6_X509PathV1* signerPath,xbytesview issuer,xbytesview anchor)
{
    const XL6_TlsRevocationOptionsV1* options=&request->options;
    if(!source||source->time!=xl6TlsVerifiedPeerPacketTimeV1(request->peer)||
        source->skew!=(uint64)options->skew||source->flags!=options->flags)
        return xl6TlsFailV1(XERR_ARGUMENT,"CRL source does not match this verification time and policy");
    bool direct=equal(source->issuer.Raw,issuer);
    if(!direct&&(!source->value.HasIssuingPoint||!source->value.IssuingPoint.Indirect))
        return xl6TlsFailV1(XERR_ARGUMENT,"direct CRL signer is not the selected certificate issuer");
    if(!direct&&!signerPath)
        return xl6TlsFailV1(XERR_ARGUMENT,"indirect CRL needs its actual validated signer path");
    if(signerPath){
        if(!signerPath->count||signerPath->count>options->maxSignerCertificates||
            !equal(signerPath->certificates[0].Raw,source->issuer.Raw)||
            !equal(signerPath->anchorCertificate.Raw,anchor)||
            signerPath->policy.Time!=source->time||
            !(signerPath->policy.Flags&X509_PATH_REQUIRE_KEY_USAGE)||
            !(signerPath->policy.KeyUsage&X509_USAGE_CRL_SIGN)||
            ((signerPath->policy.Flags&X509_PATH_ALLOW_SHA1)&&!(options->flags&X509_CRL_ALLOW_SHA1)))
            return xl6TlsFailV1(XERR_ARGUMENT,"CRL signer path is not bound to this signer, anchor, time and CRL-sign policy");
    }
    return true;
}
bool xl6TlsRevocationCheckBoundV1(const XL6_TlsRevocationRequestV1* request,const XL6_X509RevocationCheckV1* result)
{
    if(xrtGetError())return false;
    if(!request||!request->peer)return xl6TlsFailV1(XERR_ARGUMENT,"missing TLS revocation target binding");
    if(!result)return unknown(&request->options);
    xbytesview target=xl6TlsVerifiedPeerPacketPathDerV1(request->peer,request->index);
    if(!result->target||!equal(result->target->value.Raw,target))
        return xl6TlsFailV1(XERR_ARGUMENT,"revocation result belongs to another certificate target");
    if(!result->hasPolicy)return unknown(&request->options);
    if(result->time!=xl6TlsVerifiedPeerPacketTimeV1(request->peer)||result->skew!=(uint64)request->options.skew||
        result->flags!=request->options.flags)
        return xl6TlsFailV1(XERR_ARGUMENT,"revocation result belongs to another verification time or policy");
    if(!result->evidence)return unknown(&request->options);
    if(result->evidence->count>request->options.maxSources)
        return xl6TlsFailV1(XERR_RANGE,"TLS revocation source budget exceeded");
    xbytesview issuer=request->index+1<xl6TlsVerifiedPeerPacketPathCountV1(request->peer)?
        xl6TlsVerifiedPeerPacketPathDerV1(request->peer,request->index+1):xl6TlsVerifiedPeerPacketAnchorDerV1(request->peer);
    xbytesview anchor=xl6TlsVerifiedPeerPacketAnchorDerV1(request->peer);
    for(const XL6_X509RevocationEvidenceV1* evidence=result->evidence;evidence;evidence=evidence->previous){
        if(!sourceBound(request,evidence->base,evidence->signerPath,issuer,anchor)||
            (evidence->delta&&!sourceBound(request,evidence->delta,evidence->signerPath,issuer,anchor)))return false;
    }
    if(!result->value.Determined||!result->result)return unknown(&request->options);
    if(result->result->value.State==X509_REVOCATION_REVOKED)
        return xl6TlsFailV1(XERR_PERMISSION,"TLS peer certification path contains a revoked certificate");
    if(result->result->value.State!=X509_REVOCATION_GOOD||
        result->result->value.CoveredReasons!=X509_CRL_REASON_FLAG_ALL)return unknown(&request->options);
    return true;
}
