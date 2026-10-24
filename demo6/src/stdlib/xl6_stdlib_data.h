#ifndef XL6_STDLIB_DATA_H
#define XL6_STDLIB_DATA_H

#include <stddef.h>

#include "src/ast/xl6_ast.h"

#define XL6_STDLIB_CONST_COUNT 202
#define XL6_STDLIB_FUNC_COUNT 1028
extern const char* xl6StdlibNetBackendSource;
extern const char* xl6StdlibNetEngineStateSource;
extern const char* xl6StdlibNetEngineOptionsSource;
extern const char* xl6StdlibNetSocketSource;
extern const char* xl6StdlibNetStatusSource;
extern const char* xl6StdlibOnceSource;
extern const char* xl6StdlibExecutorSource;
extern const char* xl6StdlibTaskPoolSource;
extern const char* xl6StdlibEventSource;
extern const char* xl6StdlibThreadLocalSource;
extern const char* xl6StdlibCancellationSource;
extern const char* xl6StdlibPromiseSource;
extern const char* xl6StdlibChannelCaseSource;
extern const char* xl6StdlibChannelDirectFutureSource;
extern const char* xl6StdlibTemplateBufferSource;
extern const char* xl6StdlibTemplateWriterSource;
extern const char* xl6StdlibTemplateFileSource;
extern const char* xl6StdlibTemplateVfsSource;
extern const char* xl6StdlibTemplateEndSource;
extern const char* xl6StdlibTemplateEngineSource;
extern const char* xl6StdlibTemplateEngineFileSource;
extern const char* xl6StdlibTemplateEngineVfsSource;
extern const char* xl6StdlibTemplateEngineEndSource;
extern const char* xl6StdlibTemplateReadFileSource;
extern const char* xl6StdlibTemplateReadVfsSource;
extern const char* xl6StdlibCompressOptionsSource;
extern const char* xl6StdlibDeflaterSource;
extern const char* xl6StdlibInflaterSource;
extern const char* xl6StdlibNumberParseFlagsSource;
extern const char* xl6StdlibIntegerTextFlagsSource;
extern const char* xl6StdlibFloatTextFlagsSource;

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
extern const char* xl6StdlibFixedStackSource;
extern const char* xl6StdlibConsoleOptionsSource;
extern const char* xl6StdlibConsoleSessionSource;
extern const char* xl6StdlibIoReaderSource;
extern const char* xl6StdlibIoReaderBufferSource;
extern const char* xl6StdlibIoWriterSource;
extern const char* xl6StdlibIoWriterBufferSource;
extern const char* xl6StdlibIoLinesSource;
extern const char* xl6StdlibStringIteratorSource;
extern const char* xl6StdlibUtf8ValidatorSource;
extern const char* xl6StdlibRegexSource;
extern const char* xl6StdlibRegexBufferSource;
extern const char* xl6StdlibRegexEndSource;
extern const char* xl6StdlibRegexSetSource;
extern const char* xl6StdlibRegexReplacerSource;
extern const char* xl6StdlibRegexReplacerBufferSource;
extern const char* xl6StdlibRegexSplitFlagsSource;
extern const char* xl6StdlibRegexFlagsSource;
extern const char* xl6StdlibPatternSource;
extern const char* xl6StdlibDateTimeSource;
extern const char* xl6StdlibTimeFoldSource;
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
extern const char* xl6StdlibSignalHandleClassSource;
extern const char* xl6StdlibTaskGroupClassSource;
extern const char* xl6StdlibTemplateClassSource;
extern const char* xl6StdlibAtomicInt32Source;
extern const char* xl6StdlibAtomicUInt32Source;
extern const char* xl6StdlibAtomicInt64Source;
extern const char* xl6StdlibAtomicUInt64Source;
extern const char* xl6StdlibAtomicBoolSource;
extern const char* xl6StdlibAsn1ClassSource;
extern const char* xl6StdlibCryptoClassSource;
extern const char* xl6StdlibXdhClassSource;
extern const char* xl6StdlibX509DataSource;
extern const char* xl6StdlibTlsIdentitySource;
extern const char* xl6StdlibTlsParametersSource;
extern const char* xl6StdlibTlsVerificationPolicySource;
extern const char* xl6StdlibTlsVerifierSource;
extern const char* xl6StdlibTlsClientContextSource;
extern const char* xl6StdlibTlsServerContextSource;
extern const char* xl6StdlibX509RecordsSource;
extern const char* xl6StdlibX509SignatureInfoSource;
extern const char* xl6StdlibX509SignatureInfoBufferSource;
extern const char* xl6StdlibX509PublicKeyInfoSource;
extern const char* xl6StdlibX509ProfilesSource;
extern const char* xl6StdlibX509CrlProfilesSource;
extern const char* xl6StdlibX509SignatureStatusSource;
extern const char* xl6StdlibX509CrlPolicySource;
extern const char* xl6StdlibX509PathEnumsSource;
extern const char* xl6StdlibX509PathSource;
extern const char* xl6StdlibX509StoreSource;
extern const char* xl6StdlibX509CrlPolicyEnumsSource;
extern const char* xl6StdlibX509EnumsSource;
extern const char* xl6StdlibEd25519ClassSource;
extern const char* xl6StdlibSignatureHashSource;
extern const char* xl6StdlibNistClassSource;
extern const char* xl6StdlibRsaHashSource;
extern const char* xl6StdlibRsaPrivatePartSource;
extern const char* xl6StdlibRsaPublicClassSource;
extern const char* xl6StdlibRsaPrivateClassSource;
extern const char* xl6StdlibNetAddressClassSource;
extern const char* xl6StdlibNetAddressListClassSource;
extern const char* xl6StdlibNetResolverOptionsSource;
extern const char* xl6StdlibNetResolverSource;
extern const char* xl6StdlibNetResolveOperationSource;
extern const char* xl6StdlibNetDefaultResolverSource;
extern const char* xl6StdlibNetBufferSource;
extern const char* xl6StdlibNetBufferRefMembersSource;
extern const char* xl6StdlibNetFrameResultSource;
extern const char* xl6StdlibNetLineFramerSource;
extern const char* xl6StdlibNetLengthFramerSource;
extern const char* xl6StdlibNetInterfaceClassSource;
extern const char* xl6StdlibNetBytesClassSource;
extern const char* xl6StdlibNetBytesAdoptSource;
extern const char* xl6StdlibTcpSendRefMembersSource;
extern const char* xl6StdlibTcpSendMoveMembersSource;
extern const char* xl6StdlibTcpRecvOwnedMembersSource;
extern const char* xl6StdlibNetFrameClassSource;
extern const char* xl6StdlibLineFramerClassSource;
extern const char* xl6StdlibLengthFramerClassSource;
extern const char* xl6StdlibNetEngineStatsClassSource;
extern const char* xl6StdlibNetResolverStatsClassSource;
extern const char* xl6StdlibNetProxyClassSource;
extern const char* xl6StdlibNetProxyHandshakeSource;
extern const char* xl6StdlibTcpStreamOptionsSource;
extern const char* xl6StdlibTcpDialOptionsSource;
extern const char* xl6StdlibTcpProxyDialOptionsSource;
extern const char* xl6StdlibTcpProxyDialMembersSource;
extern const char* xl6StdlibTcpVectorMembersSource;
extern const char* xl6StdlibTcpNetBufferMembersSource;
extern const char* xl6StdlibTcpLineMembersSource;
extern const char* xl6StdlibTcpLengthMembersSource;
extern const char* xl6StdlibTcpFileMembersSource;
extern const char* xl6StdlibTcpListenOptionsSource;
extern const char* xl6StdlibNetEngineClassSource;
extern const char* xl6StdlibNetFileClassSource;
extern const char* xl6StdlibTcpStreamStatsClassSource;
extern const char* xl6StdlibTcpListenerStatsClassSource;
extern const char* xl6StdlibTcpStreamClassBodySource;
extern const char* xl6StdlibTcpListenerClassSource;
extern const char* xl6StdlibTcpServerClassSource;
extern const char* xl6StdlibTcpServerOptionsSource;
extern const char* xl6StdlibTcpServerStatsSource;
extern const char* xl6StdlibTcpServerStateSource;
extern const char* xl6StdlibTcpStreamStateSource;
extern const char* xl6StdlibTcpListenerStateSource;
extern const char* xl6StdlibTcpWaitSource;
extern const char* xl6StdlibUdpStatsClassSource;
extern const char* xl6StdlibUdpStateSource;
extern const char* xl6StdlibUdpClassSource;
extern const char* xl6StdlibTlsLegacyConfigClassSource;
extern const char* xl6StdlibTlsManagedSessionClassSource;
extern const char* xl6StdlibTlsStreamOptionsSource;
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
extern const char* xl6StdlibUdpConfigClassSource;
extern const char* xl6StdlibAsyncFileClassSource;
extern const char* xl6StdlibFsAsyncStatsSource;
extern const char* xl6StdlibJsonClassSource;
extern const char* xl6StdlibProcessClassSource;
extern const char* xl6StdlibProcessStatusSource;
extern const char* xl6StdlibProcessCommandSource;
extern const char* xl6StdlibProcessFileSource;
extern const char* xl6StdlibProcessLiveSource;
extern const char* xl6StdlibProcessStreamSource;
extern const char* xl6StdlibProcessPipelineSource;
extern const char* xl6StdlibTccClassSource;
extern const char* xl6StdlibXlangClassSource;

extern const XL6_StdlibConst xl6StdlibConsts[];
extern const XL6_StdlibFunc xl6StdlibFuncs[];
extern const XL6_StdlibForeignContract xl6StdlibForeignContracts[];
extern const size_t xl6StdlibConstCount;
extern const size_t xl6StdlibFuncCount;
extern const size_t xl6StdlibForeignContractCount;

#endif
