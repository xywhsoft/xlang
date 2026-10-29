#ifndef XLS_PRIVATE_FRONTEND_RAW_BODY_H
#define XLS_PRIVATE_FRONTEND_RAW_BODY_H
#include "analysis/raw_body_projection.h"
#include "analysis/frontend_context.h"
#include "src/compile/xl6_source_report.h"
typedef struct xls_raw_binding {
    XL6_AstNode* function;
    xls_raw_unit* unit;
    size_t bytes;
    int deferred,stdlibVisited,stdlibMarks,consumed;
} xls_raw_binding;
typedef struct xls_raw_stdlib_frame {
    struct xls_raw_stdlib_frame* previous;
    xls_raw_binding* binding;
} xls_raw_stdlib_frame;

typedef int (*xls_raw_encoder)(const XL6_AstNode*,xstrbuf*);
typedef struct xls_raw_scope {
    struct xls_raw_scope* previous;
    const void* baseline;
    xarray bindings,environments;
    xmap functions;
    size_t limit,unitBytes,environmentBytes;
    uint64_t registered,deferred,consumed,declined;
    int ready,environmentReady,environmentInitialized,environmentStarted;
    int failed,preprocessValid;
    xls_raw_encoder encoder;
    xls_raw_stdlib_frame* stdlibFrame;
    xstrbuf preprocess;
} xls_raw_scope;
void xlsRawBodyScopeBegin(xls_raw_scope*,const void* baseline,size_t);
void xlsRawBodyScopeEnd(xls_raw_scope*);
xls_raw_scope* xlsRawBodyScopeCurrent(void);
xls_raw_binding* xlsRawBodyBinding(const XL6_AstNode*);
void xlsRawBodyRecord(xls_raw_unit*,const char*,XL6_AstNode*,int,void*);
int xlsRawBodyAdmit(xls_raw_unit*,const char*,const XL6_AstRange*,size_t*,void*);
int xlsRawBodyCopyParse(const xls_syntax*,XL6_ParseResult*);
int xlsRawBodyCopyCursorParse(const xls_syntax*,struct XL6_CompletionQuery*,XL6_ParseResult*);
int xlsRawBodyCopySignatureParse(const xls_syntax*,struct XL6_CompletionQuery*,XL6_ParseResult*,size_t callOpen);
void xlsRawBodyRegisterSyntax(const xls_syntax*,XL6_AstNode*);
void xlsRawBodyStdlibBegin(xls_raw_stdlib_frame*,XL6_AstNode*);
void xlsRawBodyStdlibEnd(xls_raw_stdlib_frame*);
void xlsRawBodyStdlibMark(void);
int xlsRawBodyFeatureSpelling(const char*);
void xlsRawBodyRecordEnvironment(const char*,const XL6_AstNode*);
int xlsRawBodyEnvironmentCopy(xstrbuf*);
int xlsRawBodyEnvironmentHasPath(const char*);
int xlsRawBodySealPreprocess(const XL6_SourceReport*);
/* Native checker functions implement exact original key/environment rules. */
int xlsRawBodyEnvironmentKey(const XL6_AstNode*,xstrbuf*);
int xlsRawBodyPreprocessInputs(xstrbuf*,const XL6_SourceReport*);
int xlsRawBodyMemoAdmit(const void*,xls_raw_unit*,const char*,const XL6_AstRange*,size_t*);
int xlsRawBodyMemoPreprocessEqual(const void*,const xstrbuf*);
/* Existing syntax translation unit owns immutable declaration layout. */
int xlsRawBodySyntaxCopy(const xls_syntax*,struct XL6_CompletionQuery*,XL6_ParseResult*,xls_raw_scope*);
int xlsRawBodySyntaxCopySignature(const xls_syntax*,struct XL6_CompletionQuery*,XL6_ParseResult*,size_t callOpen,xls_raw_scope*);
void xlsRawBodySyntaxRegister(const xls_syntax*,XL6_AstNode*,xls_raw_scope*);
#endif
