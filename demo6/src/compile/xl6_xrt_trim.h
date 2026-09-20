#ifndef XL6_XRT_TRIM_H
#define XL6_XRT_TRIM_H

#include "src/ast/xl6_ast.h"
#include "src/compile/xl6_imports.h"
#include "src/common/xl6_output.h"

typedef struct XL6_XrtTrimConfig {
    int full;
    /* Artifact mode emits only declared roots, without legacy implicit needs. */
    int explicitModules;
    const XL6_PackageStringList* packageXrtModules;
    /* Additional native roots owned by the verified artifact linkage plan. */
    const char* const* artifactModules;
    size_t artifactModuleCount;
    int keepConsole;
    int keepTime;
    int keepFile;
    int keepFileAsync;
    int keepQueue;
    int keepAtomic;
    int keepCoroutine;
    int keepThread;
    int keepMutex;
    int keepSem;
    int keepCond;
    int keepRWLock;
    int keepNetwork;
    int keepNetInterface;
    int keepNetInterfaceText;
    int keepNetDns;
    int keepNetResolver;
    int keepNetEngine;
    int keepNetBuffer;
    int keepNetProxy;
    int keepNetProxyDial;
    int keepNetTcp;
    int keepNetTcpFile;
    int keepNetTcpDial;
    int keepNetTcpServer;
    int keepNetUdp;
    int keepHttpUtil;
    int keepCodecHex;
    int keepCodecBase64;
    int keepBufferHex;
    int keepBufferBase64;
    int keepCodecPercent;
    int keepFormUrl;
    int keepHttp1Core;
    int keepHttp1Codec;
    int keepHttpDecode;
    int keepWebSocketCodec;
    int keepWebSocketHandshake;
    int keepWebSocketUpgrade;
    int keepWebSocketStream;
    int keepWebSocketStreamTls;
    int keepCrypto;
    int keepTls;
    int keepTlsStream;
    int keepTlsDial;
    int keepTlsServer;
    int keepXid;
    int keepBuffer;
    int keepValue;
    int keepValueCollection;
    int keepRuntimeObject;
    int keepRuntimeWeak;
    int keepDynamicFields;
    int keepTypeStringValue;
    int keepTypedContainer;
    int keepTypedStack;
    int keepTypedTree;
    int keepTypedQueueSPSC;
    int keepTypedQueueMPSC;
    int keepTypedQueueMPMC;
    int keepStringCore;
    int keepStdString;
    int keepUnicode;
    int keepUnicodeDistance;
    int keepEncoding;
    int keepJnum;
    int keepMath;
    int keepJson;
    int keepXson;
    int keepTemplate;
    int keepSubprocess;
    int keepLogger;
} XL6_XrtTrimConfig;

void xl6XrtTrimConfigInit(XL6_XrtTrimConfig* cfg);
void xl6XrtTrimAnalyze(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps);
void xl6XrtTrimAnalyzeWithGeneratedSource(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps, const char* generatedSource);
int xl6XrtTrimEmitImplementationSource(XL6_Output* out, const XL6_XrtTrimConfig* cfg);
int xl6XrtTrimBuildImplementationSource(const XL6_AstNode* root, const XL6_ImportDeps* deps, char** outText);
int xl6XrtTrimBuildImplementationSourceWithGeneratedSource(const XL6_AstNode* root, const XL6_ImportDeps* deps, const char* generatedSource, char** outText);
int xl6XrtTrimBuildImplementationSourceWithRuntimeModules(const XL6_AstNode* root,
    const XL6_ImportDeps* deps, const char* generatedSource,
    const char* const* modules, size_t moduleCount, char** outText);
/* XLIR entry: only verified artifact roots and explicit package policy. */
int xl6XrtTrimBuildImplementationSourceFromArtifact(const XL6_ImportDeps* deps,
    const char* const* modules, size_t moduleCount, char** outText);

#endif
