const char* xl6StdlibX509SignatureInfoBufferSource =
    "    extern \"xl6X509SignatureInfoCheckBufferV1\" @nonblocking func check(Buffer content, bytes signature, X509PublicKeyInfo publicKey, int size = -1) as X509SignatureStatus\n"
    "    extern \"xl6X509SignatureInfoVerifyBufferV1\" @nonblocking func verify(Buffer content, bytes signature, X509PublicKeyInfo publicKey, int size = -1)\n";
