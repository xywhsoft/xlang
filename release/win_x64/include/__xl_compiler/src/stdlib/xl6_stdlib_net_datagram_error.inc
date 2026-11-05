/* Error queue records are data, not thrown language Error instances. */
#define XL6_NET_DATAGRAM_ERROR_SOURCE \
    "enum NetPathMtuMode as int\n    system = 0\n    discover = 1\n    fragment = 2\n    probe = 3\nend\n" \
    "enum NetDatagramErrorOrigin as int\n    unknown = 0\n    local = 1\n    icmp = 2\n    icmp6 = 3\nend\n" \
    "enum NetDatagramErrorFlags as int\n    none = 0\n    remote = 1\n    offender = 2\n    pathMtu = 4\n" \
    "    payloadTruncated = 8\n    metadataTruncated = 16\nend\n" \
    "class NetDatagramErrorInfo\n    private native point handle\n" \
    "    extern \"xl6NetDatagramErrorInfoCreateV1\" @nonblocking create()\n" \
    "    extern \"xl6NetDatagramErrorInfoDestroyV1\" @no_raise destroy()\n" \
    "    extern \"xl6NetDatagramErrorInfoClearV1\" @no_raise @nonblocking func clear()\n" \
    "    extern \"xl6NetDatagramErrorInfoValidV1\" @no_raise @nonblocking get hasError() as bool\n" \
    "    extern \"xl6NetDatagramErrorInfoFlagsV1\" @no_raise @nonblocking get flags() as NetDatagramErrorFlags\n" \
    "    extern \"xl6NetDatagramErrorInfoOriginV1\" @nonblocking get origin() as NetDatagramErrorOrigin\n" \
    "    extern \"xl6NetDatagramErrorInfoKindCodeV1\" @nonblocking get kindCode() as int\n" \
    "    extern \"xl6NetDatagramErrorInfoSystemCodeV1\" @nonblocking get systemCode() as int\n" \
    "    extern \"xl6NetDatagramErrorInfoTypeV1\" @nonblocking get protocolType() as int\n" \
    "    extern \"xl6NetDatagramErrorInfoCodeV1\" @nonblocking get protocolCode() as int\n" \
    "    extern \"xl6NetDatagramErrorInfoInfoV1\" @nonblocking get detailInfo() as int\n" \
    "    extern \"xl6NetDatagramErrorInfoDataV1\" @nonblocking get detailData() as int\n" \
    "    extern \"xl6NetDatagramErrorInfoPathMtuV1\" @nonblocking get pathMtu() as uint64\n" \
    "    extern \"xl6NetDatagramErrorInfoRemoteV1\" @nonblocking get remote() as NetAddress\n" \
    "    extern \"xl6NetDatagramErrorInfoOffenderV1\" @nonblocking get offender() as NetAddress\nend\n"
#define XL6_NET_SOCKET_DATAGRAM_ERROR_METHODS \
    "    extern \"xl6NetSocketPathMtuModeSetV1\" func setPathMtuMode(NetPathMtuMode mode)\n" \
    "    extern \"xl6NetSocketPathMtuModeV1\" func pathMtuMode() as NetPathMtuMode\n" \
    "    extern \"xl6NetSocketPathMtuV1\" func pathMtu() as uint64\n" \
    "    extern \"xl6NetSocketDatagramErrorsSetV1\" func enableDatagramErrors(bool enabled = true)\n" \
    "    extern \"xl6NetSocketDatagramErrorsV1\" func datagramErrorsEnabled() as bool\n" \
    "    extern \"xl6NetSocketReceiveErrorIntoV1\" func receiveErrorInto(Buffer output, NetDatagramErrorInfo info, int maxBytes = 65536) as (NetStatus, uint64)\n"
