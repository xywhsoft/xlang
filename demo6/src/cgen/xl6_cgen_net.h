#ifndef XL6_CGEN_NET_H
#define XL6_CGEN_NET_H

#include "src/common/xl6_output.h"

/* Verified feature selection for generated-program network components. XLIR
 * derives this record only from explicit runtime-bridge identities. */
typedef struct XL6_CgenNetNeeds {
    int address;
    int interfaces;
    int bytes;
    int engineStats;
    int resolverStats;
    int proxy;
    int engineObject;
    int tcpStream;
    int tcpVectors;
    int tcpFile;
    int tcpServer;
    int tcpConnectConfig;
    int tcpListenConfig;
    int udp;
    int udpConfig;
    int udpBatch;
    int tlsStream;
    int tlsDial;
    int tlsServer;
    int resolverAsync;
    int tcpDial;
    int tcpListen;
    int engine;
    int resolver;
    int future;
    int stats;
    int addressHelper;
    int deadlineHelper;
} XL6_CgenNetNeeds;

/* Emits TCP operation bodies after XLIR has supplied lifecycle, address and
 * Future-payload adapters. */
void xl6CgenEmitNetTcpComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
);

/* Emits the common NetBytes and transport-buffer operation bodies. */
void xl6CgenEmitNetBufferComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
);

/* Emits the statistics value ABI and selected TCP stream/listener snapshots. */
void xl6CgenEmitNetTcpStatsComponent(
    XL6_Output* out,
    const XL6_CgenNetNeeds* needs
);

/* Exact int64/bool/void ABI component. XLIR supplies owned Future mappers and
 * a checked Buffer object projection before these bodies. */
typedef struct XL6_CgenNetDatagramNeeds {
    int addressList, resolveAsync, socket, packet, batch, recvAsync, buffers;
    int engineStats, resolverStats, udpStats;
} XL6_CgenNetDatagramNeeds;
void xl6CgenEmitNetDatagramComponent(XL6_Output* out, const XL6_CgenNetDatagramNeeds* needs);

#endif
