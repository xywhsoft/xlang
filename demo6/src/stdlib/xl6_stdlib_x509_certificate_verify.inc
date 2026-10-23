    "    extern \"xl6X509CertificateCheckSignatureV1\" @pure @nonblocking func checkSignature(Certificate issuer) as X509SignatureStatus\n"
    "    extern \"xl6X509CertificateCheckSignatureKeyV1\" @pure @nonblocking func checkSignatureWithKey(X509PublicKeyInfo publicKey) as X509SignatureStatus\n"
    "    extern \"xl6X509CertificateVerifySignatureV1\" @pure @nonblocking func verifySignature(Certificate issuer)\n"
    "    extern \"xl6X509CertificateVerifySignatureKeyV1\" @pure @nonblocking func verifySignatureWithKey(X509PublicKeyInfo publicKey)\n"
