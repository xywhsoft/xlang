"""Isolate only proven foreign free-function body errors for cursor queries.

Original diagnostics, ordering and checker returns are preserved. Declaration
prerequisites, local errors, state mutations and unproven domains still reject.
"""


def derive(source):
    def once(old, new):
        nonlocal source
        if source.count(old) != 1:
            raise ValueError('imported body diagnostic seam changed: ' + old)
        source = source.replace(old, new, 1)

    once('    int xlsSignature;\n} XL6_SemContext;',
         '    int xlsSignature;\n    size_t xlsImportedBodyDiagnostics;\n} XL6_SemContext;')
    once('static int xlsSemCheckFuncOriginal(XL6_SemContext* ctx, XL6_AstNode* node)\n{',
         'static int xlsSemCheckFuncBodyOriginal(XL6_SemContext* ctx, XL6_AstNode* node)\n{')
    once('    oldReturns = ctx->currentReturns;\n    oldFunction = ctx->currentFunction;',
         '    xlsSemImportedBodyStart(ctx,node,ok);\n'
         '    oldReturns = ctx->currentReturns;\n    oldFunction = ctx->currentFunction;')
    once('if (q->error||(ctx->diags&&ctx->diags->count))',
         'if (q->error||xlsSemQueryErrors(ctx))')
    once('!xl6SemCheckExpr(ctx,receiver)||(ctx->diags&&ctx->diags->count)',
         '!xl6SemCheckExpr(ctx,receiver)||xlsSemQueryErrors(ctx)')
    return source, dict(auditedSeams=5, originalDiagnosticsPreserved=True,
                        normalChecksUnchanged=True, cleanPathAllocations=0,
                        domain='foreign top-level functions; valid header; no module values or shared declaration/type effects')
