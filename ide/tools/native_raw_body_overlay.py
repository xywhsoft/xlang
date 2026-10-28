"""Compose event/provenance observation around pinned original preprocessors."""
import json,re
from pathlib import Path
def verify_domains(source):
    domains=[]
    for name,count in [('xl6StdlibNodeUsesTypeName',146),('xl6StdlibNodeUsesMemberName',18)]:
        names=set(re.findall(r'\b'+name+r'\s*\([^,;\n]+,\s*"([^"]+)"\s*\)',source))
        if len(names)!=count:raise ValueError('complete stdlib literal query domain changed: '+name)
        domains.extend(names)
    domain=Path(__file__).resolve().parents[1]/'src/analysis/frontend_raw_domains.inc'
    body=domain.read_text('utf8').split('rawFeatureNames[]={',1)[1].split('};',1)[0]
    actual=[json.loads(v) for v in re.findall(r'"[^"\n]+"',body)]
    if actual!=sorted(set(domains)):raise ValueError('raw body feature index does not match complete pinned native stdlib query domains')
    return source
def once(source,old,new):
    if source.count(old)!=1:raise ValueError('native raw body seam changed: '+old)
    return source.replace(old,new)
def imports(source):
    source='#include "analysis/frontend_raw_body.h"\n'+source
    signature='int xl6ImportsExpandModule(XL6_AstNode* root, const char* sourcePath, XL6_DiagList* diags, XL6_ImportDeps* deps)\n{'
    source=once(source,signature,'static '+signature.replace('xl6ImportsExpandModule','xlsRawImportsExpandOriginal'))
    return source+'''\nint xl6ImportsExpandModule(XL6_AstNode* root,const char* path,XL6_DiagList* diags,XL6_ImportDeps* deps)
{
    xlsRawBodyRecordEnvironment(path,root);
    return xlsRawImportsExpandOriginal(root,path,diags,deps);
}
'''
def stdlib(source):
    source='#include "analysis/frontend_raw_body.h"\n'+source
    source=once(source,'            /* 同名函数可能有多个重载，名称被使用时必须全部注入。 */','            xlsRawBodyStdlibMark();\n            /* 同名函数可能有多个重载，名称被使用时必须全部注入。 */')
    source=once(source,'        ok = xl6StdlibRewriteRefsScoped(body, used, diags, &functionScope);',
        '        xls_raw_stdlib_frame rawBody;\n        xlsRawBodyStdlibBegin(&rawBody,node);\n        ok = xl6StdlibRewriteRefsScoped(body, used, diags, &functionScope);\n        xlsRawBodyStdlibEnd(&rawBody);')
    return source


def sized_ast(source):
    """Derive only a measured variant of the original source attachment.

    The original functions remain byte-identical. The shared payload predicate
    counts during the already-required walk;unvisited declared types mark the
    optional proof incomplete and use the existing complete generic fallback.
    """
    def body(signature):
        if source.count(signature)!=1:raise ValueError('native source-size seam changed: '+signature)
        start=source.index(signature);at=source.index('{',start);depth=0
        for end in range(at,len(source)):
            if source[end]=='{':depth+=1
            elif source[end]=='}':
                depth-=1
                if not depth:return source[start:end+1]
        raise ValueError('unterminated original source helper')
    attach=body('static void xl6AstAttachSourceTree(XL6_AstNode* node, XL6_AstSource* source)')
    setter=body('int xl6AstSetSourceTree(XL6_AstNode* node, const char* path)')
    if attach.count('xl6AstAttachSourceTree(')!=3:raise ValueError('audit every original attachment edge')
    attach=once(attach,'static void xl6AstAttachSourceTree(XL6_AstNode* node, XL6_AstSource* source)',
        'static void xlsAstAttachSourceTreeSized(XL6_AstNode* node,XL6_AstSource* source,size_t depth,xls_ast_source_size* size)')
    attach=once(attach,'    if (!node) return;','    if (!node) return;\n    if(size->known && (node->semanticDeclaredType || !xlsSyntaxNodeStorage(node,depth,&size->bytes,SIZE_MAX)))size->known=0;')
    attach=once(attach,'xl6AstAttachSourceTree(node->children[i], source)','xlsAstAttachSourceTreeSized(node->children[i],source,depth+1,size)')
    attach=once(attach,'xl6AstAttachSourceTree(node->inferredType, source)','xlsAstAttachSourceTreeSized(node->inferredType,source,depth+1,size)')
    setter=once(setter,'int xl6AstSetSourceTree(XL6_AstNode* node, const char* path)',
        'int xlsAstSetSourceTreeSized(XL6_AstNode* node,const char* path,size_t* bytes)')
    setter=once(setter,'    XL6_AstSource* source;','    XL6_AstSource* source;\n    xls_ast_source_size size={0,1};if(bytes)*bytes=0;\n    if(!bytes)return xl6AstSetSourceTree(node,path);')
    setter=once(setter,'    source->references = 1;',
        '    source->references = 1;\n    size.known=xlsSyntaxStorageAdd(&size.bytes,sizeof(*source),SIZE_MAX) && xlsSyntaxStorageAdd(&size.bytes,strlen(source->path)+1,SIZE_MAX);')
    setter=once(setter,'xl6AstAttachSourceTree(node, source)','xlsAstAttachSourceTreeSized(node,source,0,&size)')
    setter=once(setter,'    xl6AstReleaseSource(source);','    xl6AstReleaseSource(source);\n    if(size.known)*bytes=size.bytes;')
    return source+'\n#include "analysis/syntax_node_storage.h"\ntypedef struct xls_ast_source_size {size_t bytes;int known;} xls_ast_source_size;\n'+attach+'\n'+setter+'\n'
