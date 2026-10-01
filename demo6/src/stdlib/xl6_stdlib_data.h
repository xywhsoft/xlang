#ifndef XL6_STDLIB_DATA_H
#define XL6_STDLIB_DATA_H

#include <stddef.h>

#include "src/ast/xl6_ast.h"

#define XL6_STDLIB_CONST_COUNT 140
#define XL6_STDLIB_FUNC_COUNT 1028

typedef struct XL6_StdlibParam {
    const char* type;
    const char* name;
} XL6_StdlibParam;

typedef struct XL6_StdlibFunc {
    const char* ns;
    const char* name;
    const char* internalName;
    const char* cSymbol;
    const XL6_StdlibParam* params;
    size_t paramCount;
    const char* returnType;
    const char* sourceBody;
} XL6_StdlibFunc;

typedef enum XL6_StdlibForeignEffect {
    XL6_STDLIB_FOREIGN_EFFECT_NONE = 0,
    XL6_STDLIB_FOREIGN_EFFECT_PURE = 1u << 0,
    XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE = 1u << 1,
    XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING = 1u << 2
} XL6_StdlibForeignEffect;

/* Transitional declarative catalog for generated extern declarations.  It is
 * deliberately separate from XL6_StdlibFunc so unreviewed entries retain the
 * conservative default instead of inheriting optimistic effects. */
typedef struct XL6_StdlibForeignContract {
    const char* internalName;
    const char* cSymbol;
    unsigned int effects;
} XL6_StdlibForeignContract;

typedef struct XL6_StdlibConst {
    const char* ns;
    const char* name;
    XL6_AstKind kind;
    const char* value;
} XL6_StdlibConst;

extern const char* xl6StdlibCoreClassSource;
extern const char* xl6StdlibConsoleOptionsSource;
extern const char* xl6StdlibConsoleSessionSource;
extern const char* xl6StdlibIoReaderSource;
extern const char* xl6StdlibIoReaderBufferSource;
extern const char* xl6StdlibIoWriterSource;
extern const char* xl6StdlibIoWriterBufferSource;
extern const char* xl6StdlibIoLinesSource;
extern const char* xl6StdlibStringIteratorSource;
extern const char* xl6StdlibIoProtocolsSource;
extern const char* xl6StdlibVfsSource;
extern const char* xl6StdlibFsFileSource;
extern const char* xl6StdlibFsFileVfsSource;
extern const char* xl6StdlibFsStatSource;
extern const char* xl6StdlibFsStatVfsSource;
extern const char* xl6StdlibFsDirectorySource;
extern const char* xl6StdlibFsDirectoryVfsSource;
extern const char* xl6StdlibFsResourceSource;
extern const char* xl6StdlibFsTreeSource;
extern const char* xl6StdlibFsWalkSource;
extern const char* xl6StdlibFsPathSource;
extern const char* xl6StdlibFloatClassSource;
extern const char* xl6StdlibRandomClassSource;
extern const char* xl6StdlibRandomBufferMethodsSource;
extern const char* xl6StdlibRandomClassEndSource;
extern const char* xl6StdlibSipHasher64ClassSource;
extern const char* xl6StdlibSipHasher64BufferMethodSource;
extern const char* xl6StdlibSipHasher64ClassEndSource;
extern const char* xl6StdlibXidClassSource;
extern const char* xl6StdlibBufferClassSource;
extern const char* xl6StdlibLoggerClassSource;
extern const char* xl6StdlibLogDefaultScopeSource;
extern const char* xl6StdlibSignalClassSource;
extern const char* xl6StdlibTaskGroupClassSource;
extern const char* xl6StdlibTemplateClassSource;
extern const char* xl6StdlibCryptoClassSource;
extern const char* xl6StdlibNetAddressClassSource;
extern const char* xl6StdlibNetInterfaceClassSource;
extern const char* xl6StdlibNetBytesClassSource;
extern const char* xl6StdlibNetFrameClassSource;
extern const char* xl6StdlibLineFramerClassSource;
extern const char* xl6StdlibLengthFramerClassSource;
extern const char* xl6StdlibNetEngineStatsClassSource;
extern const char* xl6StdlibNetResolverStatsClassSource;
extern const char* xl6StdlibNetProxyClassSource;
extern const char* xl6StdlibNetEngineClassSource;
extern const char* xl6StdlibTcpStreamStatsClassSource;
extern const char* xl6StdlibTcpServerStatsClassSource;
extern const char* xl6StdlibTcpStreamClassSource;
extern const char* xl6StdlibTcpStreamFileMethodSource;
extern const char* xl6StdlibTcpStreamLineFrameMethodSource;
extern const char* xl6StdlibTcpStreamLengthFrameMethodSource;
extern const char* xl6StdlibTcpStreamClassEndSource;
extern const char* xl6StdlibTcpServerClassSource;
extern const char* xl6StdlibUdpStatsClassSource;
extern const char* xl6StdlibUdpClassSource;
extern const char* xl6StdlibTlsSessionClassSource;
extern const char* xl6StdlibTlsStreamClassSource;
extern const char* xl6StdlibTlsServerClassSource;
extern const char* xl6StdlibHttp1ClassSource;
extern const char* xl6StdlibHttpDecoderClassSource;
extern const char* xl6StdlibWsFrameClassSource;
extern const char* xl6StdlibWsHandshakeClassSource;
extern const char* xl6StdlibWsUpgradeClassSource;
extern const char* xl6StdlibWebSocketTlsExternSource;
extern const char* xl6StdlibWebSocketConfigClassSource;
extern const char* xl6StdlibWebSocketClassSource;
extern const char* xl6StdlibWebSocketTlsMethodSource;
extern const char* xl6StdlibWebSocketClassEndSource;
extern const char* xl6StdlibTcpConnectConfigClassSource;
extern const char* xl6StdlibTcpListenConfigClassSource;
extern const char* xl6StdlibUdpConfigClassSource;
extern const char* xl6StdlibAsyncFileClassSource;
extern const char* xl6StdlibFsAsyncStatsSource;
extern const char* xl6StdlibJsonClassSource;
extern const char* xl6StdlibProcessClassSource;
extern const char* xl6StdlibProcessWaitAsyncMethodSource;
extern const char* xl6StdlibProcessClassEndSource;
extern const char* xl6StdlibTccClassSource;
extern const char* xl6StdlibXlangClassSource;

extern const XL6_StdlibConst xl6StdlibConsts[];
extern const XL6_StdlibFunc xl6StdlibFuncs[];
extern const XL6_StdlibForeignContract xl6StdlibForeignContracts[];
extern const size_t xl6StdlibConstCount;
extern const size_t xl6StdlibFuncCount;
extern const size_t xl6StdlibForeignContractCount;

#endif
