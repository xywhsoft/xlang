const char* xl6StdlibTlsVerifiedPeerSource=
    "class TlsVerifiedPeer\n"
    "    private native point handle\n"
    "    extern \"xl6TlsVerifiedPeerDestroyV1\" @no_raise @nonblocking destroy()\n"
    "    extern \"xl6TlsVerifiedPeerRoleV1\" @pure @nonblocking get role() as TlsRole\n"
    "    extern \"xl6TlsVerifiedPeerTimeV1\" @pure @nonblocking get verifiedAt() as time\n"
    "    extern \"xl6TlsVerifiedPeerNameV1\" @pure @nonblocking get verificationName() as string\n"
    "    extern \"xl6TlsVerifiedPeerCertificateCountV1\" @pure @nonblocking get certificateCount() as int\n"
    "    extern \"xl6TlsVerifiedPeerPathCountV1\" @pure @nonblocking get pathCount() as int\n"
    "    extern \"xl6TlsVerifiedPeerCertificateAtV1\" @pure @nonblocking func certificateAt(int index) as Certificate\n"
    "    extern \"xl6TlsVerifiedPeerPathAtV1\" @pure @nonblocking func pathAt(int index) as Certificate\n"
    "    extern \"xl6TlsVerifiedPeerAnchorV1\" @pure @nonblocking get anchor() as Certificate\n"
    "end\n";
