#ifndef XL6_XRT_TRIM_H
#define XL6_XRT_TRIM_H

#include "src/ast/xl6_ast.h"
#include "src/compile/xl6_imports.h"
#include "src/common/xl6_output.h"

typedef struct XL6_XrtTrimConfig {
    int full;
    int keepTime;
    int keepFile;
    int keepFileAsync;
    int keepQueue;
    int keepCoroutine;
    int keepNetwork;
    int keepXUrl;
    int keepHttpUtil;
    int keepXCodec;
    int keepXHttp;
    int keepXHttpd;
    int keepXWeb;
    int keepXWs;
    int keepCrypto;
    int keepNetTls;
    int keepXid;
    int keepBuffer;
    int keepArray;
    int keepBsmn;
    int keepMemUnit;
    int keepMemPoolFs;
    int keepStack;
    int keepAvlTree;
    int keepMemPool;
    int keepDict;
    int keepList;
    int keepValue;
    int keepJnum;
    int keepJson;
    int keepXson;
    int keepTemplate;
    int keepRegex;
    int keepSubprocess;
    int keepLogger;
} XL6_XrtTrimConfig;

void xl6XrtTrimConfigInit(XL6_XrtTrimConfig* cfg);
void xl6XrtTrimAnalyze(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps);
void xl6XrtTrimAnalyzeWithGeneratedSource(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps, const char* generatedSource);
int xl6XrtTrimEmitImplementationSource(XL6_Output* out, const XL6_XrtTrimConfig* cfg);
int xl6XrtTrimBuildImplementationSource(const XL6_AstNode* root, const XL6_ImportDeps* deps, char** outText);
int xl6XrtTrimBuildImplementationSourceWithGeneratedSource(const XL6_AstNode* root, const XL6_ImportDeps* deps, const char* generatedSource, char** outText);

#endif
