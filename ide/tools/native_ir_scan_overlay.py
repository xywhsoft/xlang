"""Candidate enumeration overlays for separately pinned IR cohorts.

Original predicates and physical order are preserved. The current compiler
already uses a bounded ancestor walk for region containment; retain it.
"""


def derive(source: str, *, current=False):
    changes = {
        '#include <wchar.h>': '#include <wchar.h>\n#include "analysis/frontend_ir.inc"',
        'static const XL6_AstNode* xl6IrFindTypeDecl(const XL6_AstNode* root, const XL6_AstNode* type)\n{':
            'static const XL6_AstNode* xlsIrFindTypeDeclOriginal(const XL6_AstNode* root, const XL6_AstNode* type)\n{',
        'int xl6IrBuildModuleFromAst(XL6_IrModule* module, const XL6_AstNode* root, XL6_DiagList* diags)\n{':
            'static int xlsIrBuildModuleOriginal(XL6_IrModule* module, const XL6_AstNode* root, XL6_DiagList* diags)\n{',
        'int xl6IrRefreshExitPlans(XL6_IrModule* module)\n{': 'static int xlsIrRefreshExitPlansOriginal(XL6_IrModule* module)\n{',
        'int xl6IrVerifyModule(const XL6_IrModule* module, XL6_DiagList* diags)\n{': 'static int xlsIrVerifyModuleOriginal(const XL6_IrModule* module, XL6_DiagList* diags)\n{',
        'static int xl6IrBuildImplicitConversionAbis(XL6_IrModule* module)\n{':
            'static int xlsIrBuildImplicitConversionAbisOriginal(XL6_IrModule* module)\n{',
        'XL6_IrNodeId origin = (XL6_IrNodeId)(node - module->nodes + 1);\n'
        '    for (i = 0; i < module->callSiteCount; ++i) {\n'
        '        const XL6_IrCallSite* site = &module->callSites[i];\n'
        '        const XL6_IrNode *declaration, *parameters = NULL, *receiver;':
            'XL6_IrNodeId origin = (XL6_IrNodeId)(node - module->nodes + 1);\n'
            '    for (i = xlsIrFirstCallCandidate(module, origin); i < module->callSiteCount;\n'
            '         i = xlsIrNextCallCandidate(module, origin, i)) {\n'
            '        const XL6_IrCallSite* site = &module->callSites[i];\n'
            '        const XL6_IrNode *declaration, *parameters = NULL, *receiver;',
        'for ( prior = 0; prior < i; ++prior )\n'
        '                if ( module->callSites[prior].origin == site->origin )':
            'for ( prior = xlsIrFirstCallCandidate(module, site->origin); prior < i;\n'
            '                  prior = xlsIrNextCallCandidate(module, site->origin, prior) )\n'
            '                if ( module->callSites[prior].origin == site->origin )',
        'for ( prior = 0; prior < module->callSiteCount; ++prior )\n'
        '                if ( module->callSites[prior].origin == site->origin )':
            'for ( prior = xlsIrFirstCallCandidate(module, site->origin); prior < module->callSiteCount;\n'
            '                  prior = xlsIrNextCallCandidate(module, site->origin, prior) )\n'
            '                if ( module->callSites[prior].origin == site->origin )',
        'static int xl6IrVerifyScopeExitActions(\n    const XL6_IrModule* module, const XL6_IrExitPlan* plan)\n{':
            'static int xlsIrVerifyScopeExitActionsOriginal(\n    const XL6_IrModule* module, const XL6_IrExitPlan* plan)\n{',
        'for ( i = afterScope ? scope->childCount + 1 : (size_t)plan->origin - 1; i > 0; --i ) {':
            'for ( i = afterScope ? scope->childCount + 1 : xlsIrPreviousCandidate(module, owner, plan->origin, routed); i > 0;\n'
            '          i = afterScope ? i - 1 : xlsIrPreviousCandidate(module, owner, (XL6_IrNodeId)i, routed) ) {',
        'for (child = afterScope ? block->childCount + 1 : (size_t)plan->origin - 1; child > 0; --child) {':
            'for (child = afterScope ? block->childCount + 1 : xlsIrPreviousCandidate(module, owner, plan->origin, routed); child > 0;\n'
            '         child = afterScope ? child - 1 : xlsIrPreviousCandidate(module, owner, (XL6_IrNodeId)child, routed)) {',
        '    for ( i = 0; i < module->nodeCount; ++i ) {\n'
        '        const XL6_IrNode* scope = &module->nodes[i];\n'
        '        XL6_IrNodeId scopeId = (XL6_IrNodeId)(i + 1);':
            '    for ( i = xlsIrFirstRegionBlock(module,candidate->region); i < module->nodeCount;\n'
            '          i = xlsIrNextRegionBlock(module,candidate->region,i) ) {\n'
            '        const XL6_IrNode* scope = &module->nodes[i];\n'
            '        XL6_IrNodeId scopeId = (XL6_IrNodeId)(i + 1);',
        'for ( j = 0; j < module->exitPlanCount; ++j ) {\n'
        '            if ( module->exitPlans[j].origin == (XL6_IrNodeId)(i + 1) ) {':
            'for ( j = xlsIrFirstPlanCandidate(module, (XL6_IrNodeId)(i + 1), module->exitPlanCount);\n'
            '              j < module->exitPlanCount;\n'
            '              j = xlsIrNextPlanCandidate(module, (XL6_IrNodeId)(i + 1), j, module->exitPlanCount) ) {\n'
            '            if ( module->exitPlans[j].origin == (XL6_IrNodeId)(i + 1) ) {',
        'for (previous = 0; previous < j; ++previous)\n'
        '                        if (module->exitPlans[previous].origin == module->exitPlans[j].origin &&':
            'for (previous = xlsIrFirstPlanCandidate(module, (XL6_IrNodeId)(i + 1), j); previous < j;\n'
            '                         previous = xlsIrNextPlanCandidate(module, (XL6_IrNodeId)(i + 1), previous, j))\n'
            '                        if (module->exitPlans[previous].origin == module->exitPlans[j].origin &&',
    }
    if current:
        del changes['    for ( i = 0; i < module->nodeCount; ++i ) {\n        const XL6_IrNode* scope = &module->nodes[i];\n        XL6_IrNodeId scopeId = (XL6_IrNodeId)(i + 1);']
        # Construction effects now query call origins for every node. Reuse
        # the same physical-order index in the two immutable build phases
        # and in the existing verifier scope; keep all argument predicates.
        changes.update({
            'const XL6_IrCallSite* xl6IrCallSiteGet(const XL6_IrModule* module, XL6_IrNodeId origin)\n{':
                'static const XL6_IrCallSite* xlsIrCallSiteGetOriginal(const XL6_IrModule* module, XL6_IrNodeId origin)\n{',
            'static int xl6IrBuildMemberAbis(XL6_IrModule* module)\n{':
                'static int xlsIrBuildMemberAbisOriginal(XL6_IrModule* module)\n{',
            'int xl6IrRefreshLoopAbis(XL6_IrModule* module)\n{':
                'static int xlsIrRefreshLoopAbisOriginal(XL6_IrModule* module)\n{',
        })
    for old,new in changes.items():
        if source.count(old)!=1:
            raise ValueError("shared IR scan seam changed; audit its native overlay: "+old)
        source=source.replace(old,new)
    return source,len(changes)
