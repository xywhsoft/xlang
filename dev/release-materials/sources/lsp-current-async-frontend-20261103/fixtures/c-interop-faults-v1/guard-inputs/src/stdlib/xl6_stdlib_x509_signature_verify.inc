    "    extern \"xl6X509SignatureInfoCheckBytesV1\" @pure @nonblocking func check(bytes content, bytes signature, X509PublicKeyInfo publicKey, int size = -1) as X509SignatureStatus\n"
    "    extern \"xl6X509SignatureInfoVerifyBytesV1\" @pure @nonblocking func verify(bytes content, bytes signature, X509PublicKeyInfo publicKey, int size = -1)\n"
    "    extern \"xl6X509SignatureInfoCheckTextV1\" @pure @nonblocking func check(string content, bytes signature, X509PublicKeyInfo publicKey, int size = -1) as X509SignatureStatus\n"
    "    extern \"xl6X509SignatureInfoVerifyTextV1\" @pure @nonblocking func verify(string content, bytes signature, X509PublicKeyInfo publicKey, int size = -1)\n"
