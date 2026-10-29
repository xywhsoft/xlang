    "    extern \"xl6X509CrlIssuerNameV1\" @pure @nonblocking func issuerName(int maxAttributes = 65536) as X509Name\n"
    "    extern \"xl6X509CrlExtensionListV1\" @pure @nonblocking func extensions(int maxExtensions = 65536) as X509Extensions\n"
    "    extern \"xl6X509CrlSignatureAlgorithmV1\" @pure @nonblocking get signatureAlgorithm() as X509Algorithm\n"
    "    extern \"xl6X509CrlTbsSignatureAlgorithmV1\" @pure @nonblocking get tbsSignatureAlgorithm() as X509Algorithm\n"
