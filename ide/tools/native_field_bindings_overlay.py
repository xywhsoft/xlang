"""Native FIELD identity, selection and already-resolved field-use facts.

Keep the allocator, traversal, access/type/inheritance rules and AST ABI.
Field result helpers run only after the original type clone succeeds.
"""
from pathlib import Path
import hashlib

def _once(s,old,new):
    if s.count(old)!=1:raise ValueError('Unknown field binding seam: '+old[:120])
    return s.replace(old,new,1)

def _function(s,start,end,change):
    if s.count(start)!=1:raise ValueError('Unknown field producer: '+start)
    first=s.index(start)
    last=s.index(end,first)
    return s[:first]+change(s[first:last])+s[last:]

def _save(source,output,text,seams):
    source,output=Path(source),Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(text,encoding='utf8',newline='\n')
    return dict(source=str(source),sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                compiledSource=str(output),compiledSha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                auditedSeams=seams,extraAstWalks=0,extraFieldTypeQueries=0,astAbiChanged=False)

def semantic(source,output):
    s=Path(source).read_text(encoding='utf8')
    s=_function(s,'static int xl6SemNodeHasEntityIdentity(','static int xl6SemAssignEntitySymbolIds(',
       lambda b:_once(b,'    case XL6_AST_NATIVE_EXTERN_SPECIAL:',
                     '    case XL6_AST_FIELD:\n    case XL6_AST_NATIVE_EXTERN_SPECIAL:'))
    at='static XL6_AstNode* xl6SemCloneStaticClassFieldType(XL6_SemContext* ctx, XL6_AstNode* member)\n{'
    s=_once(s,at,'#include "analysis/frontend_field_bindings.inc"\n'+at)
    s=_function(s,at,'\nstatic XL6_AstNode* xl6SemResolveStaticClassFuncCall(',
       lambda b:_once(b,'return xl6SemCloneTypeNode(field->children[0]);',
                     'return xlsSemBoundFieldType(member,field,xl6SemCloneTypeNode(field->children[0]));'))
    def record(b):
        b=_once(b,'xl6SemCloneRecordFieldType(','xlsSemCloneRecordFieldTypeRef(')
        b=_once(b,'const char* fieldName)','const char* fieldName, XL6_AstNode* reference)')
        b=_once(b,'return field != NULL && field->childCount > 0 ? xl6SemCloneTypeNode(field->children[0]) : NULL;',
            'return field != NULL && field->childCount > 0 ? xlsSemBoundFieldType(reference,field,xl6SemCloneTypeNode(field->children[0])) : NULL;')
        b=_once(b,'return xl6SemSubstituteClassType(ownerClass, typeArgs, field->children[0]);',
            'return xlsSemBoundFieldType(reference,field,xl6SemSubstituteClassType(ownerClass, typeArgs, field->children[0]));')
        b=_once(b,'return xl6SemCloneTypeNode(field->children[0]);',
            'return xlsSemBoundFieldType(reference,field,xl6SemCloneTypeNode(field->children[0]));')
        return b+'\nstatic XL6_AstNode* xl6SemCloneRecordFieldType(XL6_SemContext* ctx,const XL6_AstNode* type,const char* name)\n{return xlsSemCloneRecordFieldTypeRef(ctx,type,name,NULL);}\n'
    s=_function(s,'static XL6_AstNode* xl6SemCloneRecordFieldType(\n',
                '\nstatic const XL6_AstNode* xl6SemFindClassCallableMemberNode(',record)
    s=_once(s,'result = xl6SemCloneRecordFieldType(ctx, receiverType, member->text);',
              'result = xlsSemCloneRecordFieldTypeRef(ctx, receiverType, member->text, member);')
    s=_once(s,'result = xl6SemCloneRecordFieldType(ctx, nullableInner, member->text);',
              'result = xlsSemCloneRecordFieldTypeRef(ctx, nullableInner, member->text, member);')
    s=_once(s,'    if ( implicitField != NULL ) {\n        /*',
              '    if ( implicitField != NULL ) {\n        node->semanticTargetSymbolId=implicitField->semanticSymbolId;\n        /*')
    return _save(source,output,s,10)

def index(source,output):
    s=Path(source).read_text(encoding='utf8')
    s=_function(s,'static const XL6_AstNode* xl6IndexSelection(',
                '\nstatic const char* xl6IndexText(',lambda b:_once(b,
        '    case XL6_AST_NAME: case XL6_AST_PARAM: case XL6_AST_VAR_DECL: case XL6_AST_CONST_DECL:',
        '    case XL6_AST_FIELD:return n->text && *n->text ? n : NULL;\n    case XL6_AST_NAME: case XL6_AST_PARAM: case XL6_AST_VAR_DECL: case XL6_AST_CONST_DECL:'))
    return _save(source,output,s,1)

def ir(source,output):
    s=Path(source).read_text(encoding='utf8')
    s=_once(s,'source->semanticTargetSymbolId && source->inferredType &&',
              '(source->semanticSymbolId==0 || source->semanticSymbolId!=source->semanticTargetSymbolId) &&\n       source->semanticTargetSymbolId && source->inferredType &&')
    marker='static const XL6_IrClassField* xl6IrBuildImplicitClassField(\n'
    s=_once(s,marker,'#include "analysis/frontend_field_ir.inc"\n'+marker)
    s=_function(s,marker,'\n/* Resolve source NAME forms',lambda b:_once(b,
        '        if ( field->ownerType != layout->type ||',
        '        if ( (name->targetSymbol && (!declaration || declaration->symbol!=name->targetSymbol)) ||\n             field->ownerType != layout->type ||'))
    guard='name->targetSymbol != 0 || name->typeValueTarget != XL6_IR_TYPE_INVALID ||'
    if s.count(guard)not in (1,2):raise ValueError('Unknown implicit-name classifier guards')
    s=s.replace(guard,'!xlsIrImplicitTargetAllowed(module,name) || name->typeValueTarget != XL6_IR_TYPE_INVALID ||')
    s=_once(s,'if ( strcmp(name->text, "base") == 0 &&',
              'if ( !name->targetSymbol && strcmp(name->text, "base") == 0 &&')
    s=_once(s,'} else if ( strcmp(name->text, "base") != 0 &&',
              '} else if ( (name->targetSymbol || strcmp(name->text, "base") != 0) &&')
    return _save(source,output,s,6)
