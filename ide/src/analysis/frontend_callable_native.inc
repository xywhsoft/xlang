/* Included once after the original checker. No new lookup or mutation. */
#include "analysis/frontend_callable_native.h"
#include "analysis/callable.h"
const char* xlsNativeCallableName(const XL6_AstNode* n,int global,unsigned* kind)
{
 if(kind)*kind=0;
 if(!kind)return NULL;
 if(global){if(!xl6SemIsFuncDecl(n))return NULL;*kind=XLS_CALLABLE_OVERLOAD;return xl6SemFuncName(n);}
 if(xl6SemIsClassPropertyMember(n)){*kind=XLS_CALLABLE_PROPERTY;return xl6SemMemberName(n);}
 if(!xl6SemMembersShareOverloadGroup(n,n))return NULL;
 *kind=XLS_CALLABLE_OVERLOAD;return xl6SemMemberName(n);
}
int xlsNativeCallableRelated(const XL6_AstNode* a,const XL6_AstNode* b,int global)
{
 if(global)return xl6SemIsFuncDecl(a) && xl6SemIsFuncDecl(b) && xl6TextEquals(xl6SemFuncName(a),xl6SemFuncName(b));
 if(xl6SemMembersShareOverloadGroup(a,b))return 1;
 return xl6SemIsClassPropertyMember(a) && xl6SemIsClassPropertyMember(b) && xl6TextEquals(xl6SemMemberName(a),xl6SemMemberName(b));
}
