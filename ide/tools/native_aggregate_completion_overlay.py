"""Expose current aggregate fields through the original native completion API."""
from native_c_interop_profile import pins_for
from native_final_c_interop_profile import PROJECTION_PINS


def derive(source,frontend):
 if pins_for(frontend)is not PROJECTION_PINS:return source,dict(enabled=False,scope='Historical completion behavior retained')
 signature='static void xl6SemCompletionMembers(XL6_SemContext* ctx,XL6_AstNode* queryNode)\n{'
 guard=' if (!decl||decl->kind!=XL6_AST_CLASS) {q->error="receiver member completion is not supported for this semantic type";goto done;}'
 if source.count(signature)!=1 or source.count(guard)!=1:raise ValueError('native aggregate completion boundary changed')
 source=source.replace(signature,'#include "analysis/frontend_completion_aggregate.inc"\n'+signature,1)
 source=source.replace(guard,' if (decl && !staticClass && xl6AstKindIsAggregate(decl->kind)) {xlsSemCompletionAggregate(ctx,decl,concrete);goto done;}\n'+guard,1)
 return source,dict(enabled=True,seams=2,scope='Only instance aggregate field enumeration; reuse native receiver checking, alias/nullable rules, first-field lookup, declared type clone, canonical emitter and query budgets. Static class and ordinary member rules unchanged.')
