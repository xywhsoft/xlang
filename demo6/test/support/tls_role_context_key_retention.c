/* Ordinary signatures prove the projected identity still owns its key after
 * both the source identity and role context have been released. No transport. */
#include "src/runtime/xl6_tls_role_context_internal.h"
#include <assert.h>
#include <stdio.h>
#include "tls_role_context_vectors.h"
typedef xtlsidentity* (*Factory)(const xbytesview*,size_t,xbytesview);
int main(void)
{
    static const Factory factories[]={xrtTlsIdentityRsa,xrtTlsIdentityP256,xrtTlsIdentityP384,xrtTlsIdentityEd25519};
    static const xtlssignature schemes[]={XTLS_SIGNATURE_RSA_PKCS1_SHA256,XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,XTLS_SIGNATURE_ED25519};
    static const uint8 message[]="ordinary TLS context retention";
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    for(size_t i=0;i<4;++i){
        xbytesview chain[2];chain[0]=pathVectors[i].leaf;chain[1]=pathVectors[i].mid;
        xtlsidentity* identity=factories[i](chain,2,tlsIdentityKeys[i]);assert(identity);
        void* context=xl6TlsServerContextDefaultV1(identity);assert(context);xrtTlsIdentityRelease(identity);
        xvalue* empty=xrtValueArray();assert(empty);
        assert(!xl6TlsServerContextWithProtocolsV1(context,empty,true) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
        xrtClearError();xrtValueRelease(empty);
        assert(!xl6TlsServerContextRequiresProtocolV1(context) && !xl6TlsServerContextProtocolCountV1(context));
        xtlsidentity* projected=xl6TlsServerContextIdentityV1(context);assert(projected);xl6TlsServerContextDestroyV1(context);
        uint8 signature[1024];size_t size=0;xtlsversion version=i?XTLS_VERSION_13:XTLS_VERSION_12;
        assert(xrtTlsIdentitySign(projected,version,schemes[i],(xbytesview){message,sizeof(message)-1},signature,sizeof(signature),&size) && size);
        printf("TLS role retained signature %zu ",i);for(size_t j=0;j<size;++j)printf("%02x",(unsigned)signature[j]);putchar('\n');
        xrtTlsIdentityRelease(projected);
    }
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("TLS role retained keys: independent context projections, balanced");return 0;
}
