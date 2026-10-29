#ifndef XL6_TLS_METADATA_FIELDS_H
#define XL6_TLS_METADATA_FIELDS_H
/* One declaration schema, available in the private header-only SDK too.
 * Each consumer defines the field operations before invoking this list.
 * The list can be expanded repeatedly; no implementation body is embedded. */
#define XL6_TLS_METADATA_FIELDS() \
XL6_TLS_INFO_ENUM(Role,role,TlsRole) \
XL6_TLS_INFO_ENUM(State,state,TlsStreamState) \
XL6_TLS_INFO_ENUM(SessionState,sessionState,TlsState) \
XL6_TLS_INFO_BOOL(Ready,ready) \
XL6_TLS_INFO_BOOL(HasSelection,hasSelection) \
XL6_TLS_INFO_BOOL(Resumed,resumed) \
XL6_TLS_INFO_UINT(Available,available) \
XL6_TLS_INFO_UINT(Pending,pending) \
XL6_TLS_INFO_UINT(AsyncBytes,asyncBytes) \
XL6_TLS_INFO_UINT(AsyncCount,asyncCount) \
XL6_TLS_INFO_BOOL(HasServerCookie,hasServerCookie) \
XL6_TLS_INFO_UINT(ServerCookie,serverCookie) \
XL6_TLS_INFO_BOOL(HasPeerAlert,hasPeerAlert) \
XL6_TLS_INFO_INT(PeerAlertLevel,peerAlertLevel) \
XL6_TLS_INFO_INT(PeerAlertCode,peerAlertCode) \
XL6_TLS_INFO_BOOL(HasError,hasError) \
XL6_TLS_INFO_INT(ErrorKind,errorKind) \
XL6_TLS_INFO_INT(ErrorCode,errorCode) \
XL6_TLS_INFO_BYTES(Protocol,protocol) \
XL6_TLS_INFO_BYTES(ServerName,serverName) \
XL6_TLS_INFO_TEXT(ErrorDomain,errorDomain) \
XL6_TLS_INFO_TEXT(ErrorOperation,errorOperation) \
XL6_TLS_INFO_TEXT(ErrorMessage,errorMessage) \
XL6_TLS_STATS_UINT(Handshakes,handshakes) \
XL6_TLS_STATS_UINT(Accepted,accepted) \
XL6_TLS_STATS_UINT(Rejected,rejected) \
XL6_TLS_STATS_UINT(HandshakeErrors,handshakeErrors) \
XL6_TLS_STATS_UINT(ActiveHandshakes,activeHandshakes) \
XL6_TLS_STATS_UINT(PeakHandshakes,peakHandshakes) \
XL6_TLS_STATS_UINT(QueuedAccepts,queuedAccepts) \
XL6_TLS_STATS_UINT(PeakQueuedAccepts,peakQueuedAccepts) \
XL6_TLS_STATS_UINT(AcceptWaiters,acceptWaiters)
#endif
