#include "xl6_x509_path_internal.h"
bool xl6X509PathPolicyValidV1(const xx509pathconfig* config)
{
    if(!config || (config->Flags&~(X509_PATH_REQUIRE_KEY_USAGE|X509_PATH_REQUIRE_PURPOSE|X509_PATH_ALLOW_SHA1)) ||
        (config->KeyUsage&UINT16_C(0xfe00)) ||
        ((config->Flags&X509_PATH_REQUIRE_KEY_USAGE) && !config->KeyUsage) ||
        ((config->Flags&X509_PATH_REQUIRE_PURPOSE) && !config->Purpose.Size) ||
        (config->Purpose.Size && !config->Purpose.Data))
        return xl6X509FailV1(XERR_ARGUMENT,"invalid explicit path policy");
    if(config->Purpose.Size) {
        xbuffer text={0};bool ok=xl6X509OutcomeV1(xrtDerOidDecode(config->Purpose,&text));
        xl6X509BufferFreeV1(&text);if(!ok)return false;
    }
    return true;
}
