    "    extern \"xl6X509CrlCheckSignatureV1\" @pure @nonblocking func checkSignature(Certificate issuer) as X509SignatureStatus\n"
    "    extern \"xl6X509CrlCheckSignatureKeyV1\" @pure @nonblocking func checkSignatureWithKey(X509PublicKeyInfo publicKey) as X509SignatureStatus\n"
    "    extern \"xl6X509CrlVerifySignatureV1\" @pure @nonblocking func verifySignature(Certificate issuer)\n"
    "    extern \"xl6X509CrlVerifySignatureKeyV1\" @pure @nonblocking func verifySignatureWithKey(X509PublicKeyInfo publicKey)\n"
