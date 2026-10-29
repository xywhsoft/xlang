#define XL6_NET_BATCH_SOURCE \
    "class NetBatchProgress\n    private native point handle\n" \
    "    extern \"xl6NetBatchProgressCreateV1\" @nonblocking create()\n" \
    "    extern \"xl6NetBatchProgressDestroyV1\" @no_raise destroy()\n" \
    "    extern \"xl6NetBatchProgressClearV1\" @no_raise @nonblocking func clear()\n" \
    "    extern \"xl6NetBatchProgressAttemptedV1\" @no_raise @nonblocking get attempted() as bool\n" \
    "    extern \"xl6NetBatchProgressCountV1\" @no_raise @nonblocking get completedCount() as uint64\n" \
    "    extern \"xl6NetBatchProgressStartV1\" @no_raise @nonblocking get startIndex() as uint64\n" \
    "    extern \"xl6NetBatchProgressStatusV1\" @no_raise @nonblocking get status() as NetStatus\nend\n" \
    "class NetDatagramPacket\n    private native point handle\n" \
    "    extern \"xl6NetDatagramPacketCreateV1\" @nonblocking create(int capacity = 0)\n" \
    "    extern \"xl6NetDatagramPacketCreateTextV1\" @nonblocking create(string data, int size = -1)\n" \
    "    extern \"xl6NetDatagramPacketCreateBytesV1\" @nonblocking create(bytes data, int size = -1)\n" \
    "    extern \"xl6NetDatagramPacketCreateBufferV1\" @nonblocking create(Buffer storage)\n" \
    "    extern \"xl6NetDatagramPacketDestroyV1\" destroy()\n" \
    "    extern \"xl6NetDatagramPacketBufferHandleV1\" @nonblocking func shareBuffer() as Buffer\n" \
    "    extern \"xl6NetDatagramPacketTargetSetV1\" @nonblocking func setTarget(NetAddress target)\n" \
    "    extern \"xl6NetDatagramPacketTargetClearV1\" @no_raise @nonblocking func clearTarget()\n" \
    "    extern \"xl6NetDatagramPacketHasTargetV1\" @no_raise @nonblocking get hasTarget() as bool\n" \
    "    extern \"xl6NetDatagramPacketTargetV1\" @nonblocking get target() as NetAddress\n" \
    "    extern \"xl6NetDatagramPacketValidV1\" @no_raise @nonblocking get hasPacket() as bool\n" \
    "    extern \"xl6NetDatagramPacketStatusV1\" @nonblocking get status() as NetStatus\n" \
    "    extern \"xl6NetDatagramPacketInfoV1\" @nonblocking get info() as NetDatagramInfo\n" \
    "    extern \"xl6NetDatagramPacketClearV1\" @nonblocking func clear()\nend\n"
#define XL6_NET_SOCKET_BATCH_METHODS \
    "    extern \"xl6NetSocketSendBatchV1\" func sendBatch(array<NetDatagramPacket> packets, NetBatchProgress progress, int start = 0, int count = -1) as (NetStatus, uint64)\n" \
    "    extern \"xl6NetSocketReceiveBatchV1\" func receiveBatch(array<NetDatagramPacket> packets, NetBatchProgress progress, int maxBytesPerPacket = 65536, int start = 0, int count = -1) as (NetStatus, uint64)\n"
