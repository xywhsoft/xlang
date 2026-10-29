static uint64_t xl6CompiledHash(const void* data, size_t size)
{
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<size;++i)hash=(hash^((const unsigned char*)data)[i])*UINT64_C(1099511628211);
    return hash;
}

static int xl6CompiledOwnsNode(const XL6_ImportPackageInfo* info,const XL6_AstNode* node)
{
    const char* source=xl6AstSourcePath(node);
    char* absolute=source?xrtPathAbs(source):NULL;
    if(source && !absolute)return -1;
    size_t length=strlen(info->dir);
    int owns=absolute && strlen(absolute)>length &&
#ifdef _WIN32
        _strnicmp(absolute,info->dir,length)==0 &&
#else
        strncmp(absolute,info->dir,length)==0 &&
#endif
        (absolute[length]=='/' || absolute[length]=='\\');
    xrtFree(absolute);return owns;
}

static XL6_AstNode* xl6CompiledDeclaration(XL6_AstNode* node,uint32 symbol)
{
    if(!node)return NULL;
    if(node->semanticSymbolId==symbol && (node->kind==XL6_AST_EXTERN_FUNC ||
        node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC ||
        node->kind==XL6_AST_NATIVE_EXTERN_FUNC || node->kind==XL6_AST_NATIVE_EXTERN_SPECIAL ||
        node->kind==XL6_AST_GET || node->kind==XL6_AST_LET || node->kind==XL6_AST_CREATE))return node;
    for(size_t i=0;i<node->childCount;++i) {
        XL6_AstNode* found=xl6CompiledDeclaration(node->children[i],symbol);
        if(found)return found;
    }
    return NULL;
}

static int xl6CompiledSelect(XL6_CompileUnit* unit,const char* name,unsigned depth)
{
    XL6_ImportPackageInfo* info=NULL;
    if(depth>XL_COMPILED_PACKAGE_MAX_PACKAGES)return 0;
    for(size_t i=0;i<unit->importDeps.packageInfoCount;++i)
        if(!strcmp(unit->importDeps.packageInfos[i].name,name)) {info=&unit->importDeps.packageInfos[i];break;}
    if(!info) {
        xl6DiagAdd(&unit->parse.diags,1,1,"compiledPackage requires a package imported by the host compilation");return 0;
    }
    if(info->compiledRoot==2) {
        xl6DiagAdd(&unit->parse.diags,1,1,"compiled package dependency cycle");return 0;
    }
    if(info->compiledRoot==1)return 1;
    info->compiledRoot=2;
    /* The effective source graph also includes package imports not repeated
     * in the manifest. Keep their interface closure in the same descriptor. */
    for(size_t n=0;n<unit->parse.root->childCount;++n) {
        XL6_AstNode* node=unit->parse.root->children[n];
        if(!node || node->kind!=XL6_AST_IMPORT || !node->text)continue;
        int owns=xl6CompiledOwnsNode(info,node);if(owns<0)return 0;
        if(!owns)continue;
        for(size_t p=0;p<unit->importDeps.packageInfoCount;++p) {
            const char* dependency=unit->importDeps.packageInfos[p].name;
            if(strcmp(node->text,dependency))continue;
            int known=0;
            for(size_t d=0;d<info->dependencies.count;++d)known|=!strcmp(info->dependencies.items[d],dependency);
            if(!known && !xl6PackageStringListAppend(&info->dependencies,dependency))return 0;
        }
    }
    for(size_t d=0;d<info->dependencies.count;++d)
        if(!xl6CompiledSelect(unit,info->dependencies.items[d],depth+1))return 0;
    info->compiledRoot=1;return 1;
}

static int xl6CompiledRequests(XL6_CompileUnit* unit,XL6_AstNode* node)
{
    if(!node)return 1;
    if(node->kind==XL6_AST_CALL && node->semanticTargetSymbolId) {
        XL6_AstNode* target=xl6CompiledDeclaration(unit->parse.root,node->semanticTargetSymbolId);
        if(target && target->kind==XL6_AST_EXTERN_FUNC && target->text &&
            (!strcmp(target->text,"\"xl6_xlang_compiled_package\"") || !strcmp(target->text,"xl6_xlang_compiled_package"))) {
            XL6_AstNode* args=node->childCount>1?node->children[1]:NULL;
            XL6_AstNode* arg=args && args->childCount==1?args->children[0]:NULL;
            XL6_AstNode* value=arg && arg->kind==XL6_AST_ARG && arg->childCount?arg->children[0]:arg;
            if(!value || value->kind!=XL6_AST_STRING || !value->literalDecoded || !value->literalSize ||
                memchr(value->literalData,0,value->literalSize)) {
                xl6DiagAddFile(&unit->parse.diags,xl6AstSourcePath(node),node->line,node->column,
                    "compiledPackage requires one nonempty constant package name");return 0;
            }
            char* name=xrtStrDupN((const char*)value->literalData,value->literalSize);
            int ok=name && xl6CompiledSelect(unit,name,0);xrtFree(name);if(!ok)return 0;
        }
    }
    for(size_t i=0;i<node->childCount;++i)if(!xl6CompiledRequests(unit,node->children[i]))return 0;
    return 1;
}

static void xl6CompiledMark(XL6_AstNode* node)
{
    if(!node)return;
    if(node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC ||
        node->kind==XL6_AST_NATIVE_EXTERN_FUNC || node->kind==XL6_AST_NATIVE_EXTERN_SPECIAL ||
        node->kind==XL6_AST_EXTERN_FUNC || node->kind==XL6_AST_GET || node->kind==XL6_AST_LET ||
        node->kind==XL6_AST_CREATE)
        { node->compiledPackageRoot=1; return; }
    for(size_t i=0;i<node->childCount;++i)xl6CompiledMark(node->children[i]);
}

static int xl6CompileUnitSelectCompiledPackages(XL6_CompileUnit* unit)
{
    if(!xl6CompiledRequests(unit,unit->parse.root))return 0;
    for(size_t p=0;p<unit->importDeps.packageInfoCount;++p) {
        XL6_ImportPackageInfo* info=&unit->importDeps.packageInfos[p];
        if(info->compiledRoot!=1)continue;
        for(size_t n=0;n<unit->parse.root->childCount;++n) {
            XL6_AstNode* node=unit->parse.root->children[n];
            int owns=xl6CompiledOwnsNode(info,node);
            if(owns<0)return 0;
            if(owns)xl6CompiledMark(node);
        }
    }
    return 1;
}

typedef struct XL6_CompiledWriter {
    unsigned char* data;
    size_t size,capacity,nodes;
    const XL6_IrModule* module;
    uint64_t binding;
    int failed;
    const char* reason;
    const XL6_IrCallTarget* signature;
} XL6_CompiledWriter;

static void xl6CompiledWrite(XL6_CompiledWriter* w,const void* data,size_t size)
{
    if(w->failed || size>XL_COMPILED_PACKAGE_MAX_INTERFACE-w->size){w->failed=1;return;}
    if(w->size+size>w->capacity) {
        size_t capacity=w->capacity?w->capacity:256;
        while(capacity<w->size+size)capacity*=2;
        unsigned char* next=(unsigned char*)xrtRealloc(w->data,capacity);
        if(!next){w->failed=1;return;}w->data=next;w->capacity=capacity;
    }
    if(size)memcpy(w->data+w->size,data,size);
    w->size+=size;
}

static void xl6CompiledU32(XL6_CompiledWriter* w,uint32_t value)
{
    unsigned char bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=(unsigned char)(value>>(8*i));
    xl6CompiledWrite(w,bytes,4);
}

static int xl6CompiledInterfaceDeclaration(XL6_AstKind kind)
{
    switch(kind) {
    case XL6_AST_IMPORT: case XL6_AST_FUNC: case XL6_AST_ASYNC_FUNC: case XL6_AST_EXTERN_FUNC:
    case XL6_AST_EXPORT_FUNC: case XL6_AST_EXTERN_VAR: case XL6_AST_EXPORT_VAR:
    case XL6_AST_CONST_DECL: case XL6_AST_EXPORT_CONST:
    case XL6_AST_TYPE_ALIAS: case XL6_AST_EXTERN_TYPE_ALIAS:
    case XL6_AST_CLASS: case XL6_AST_PROTOCOL: case XL6_AST_ENUM:
    case XL6_AST_UNION: case XL6_AST_DERIVE: return 1;
    default: return 0;
    }
}

static int xl6CompiledLiteralConstant(const XL6_AstNode* node,unsigned depth)
{
    if(!node || depth>128)return 0;
    switch(node->kind) {
    case XL6_AST_INT: case XL6_AST_FLOAT: case XL6_AST_STRING: case XL6_AST_CHAR:
    case XL6_AST_BYTES: case XL6_AST_TIME: case XL6_AST_BOOL: case XL6_AST_NULL:
    case XL6_AST_UNARY: case XL6_AST_BINARY: case XL6_AST_CONDITIONAL: break;
    default:return 0;
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!xl6CompiledLiteralConstant(node->children[i],depth+1))return 0;
    return 1;
}

static void xl6CompiledNode(XL6_CompiledWriter* w,const XL6_AstNode* node,unsigned depth,int generic)
{
    char link[96]="";unsigned error=0;int stub=0;
    if(!node){w->failed=1;return;}
    if(depth>128 || ++w->nodes>65536){w->failed=1;return;}
    if(node->kind==XL6_AST_FIELD && (node->flags&XL6_AST_FLAG_STATIC)) {
        w->reason="compiled packages require accessors for mutable static fields";w->failed=1;return;
    }
    int classTemplate=node->kind==XL6_AST_CLASS && node->childCount>2 &&
        node->children[2] && node->children[2]->childCount;
    int funcTemplate=(node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC) &&
        node->childCount>4 && node->children[4] && node->children[4]->childCount;
    generic=generic || classTemplate || funcTemplate;
    int asyncDeclaration=!generic && node->kind==XL6_AST_ASYNC_FUNC;
    const XL6_IrCallTarget* bindingTarget=!generic?xl6IrCallTargetGet(w->module,node->semanticSymbolId):NULL;
    int normalizeAsync=asyncDeclaration && (!bindingTarget || bindingTarget->kind!=XL6_IR_CALL_TARGET_INTERNAL_GENERATOR);
    if(node->kind==XL6_AST_PARAM && w->signature) {
        if(!w->signature->parameterLifetimes){w->failed=1;return;}
        for(size_t p=0;p<w->signature->parameterCount;++p) {
            const XL6_IrParameterLifetime* lifetime=&w->signature->parameterLifetimes[p];
            if(lifetime->sourceSymbol!=node->semanticSymbolId)continue;
            error=XL6_COMPILED_PARAM_LIFETIME_TAG | (lifetime->rebindable?1u:0u) |
                (lifetime->borrowReadOnly?2u:0u) | ((unsigned)lifetime->cleanupEffect<<2);
            if(!XL6_COMPILED_PARAM_LIFETIME_VALID(error))w->failed=1;
            break;
        }
        if(!error)w->failed=1;
    }
    if(!generic && (node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC ||
        node->kind==XL6_AST_GET || node->kind==XL6_AST_LET || node->kind==XL6_AST_CREATE) &&
        !(node->flags&(XL6_AST_FLAG_EXTERN|XL6_AST_FLAG_PROTOCOL_REQUIREMENT))) {
        const XL6_IrCallTarget* target=xl6IrCallTargetGet(w->module,node->semanticSymbolId);
        if(!target || (target->kind!=XL6_IR_CALL_TARGET_INTERNAL &&
            target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC && target->kind!=XL6_IR_CALL_TARGET_INTERNAL_GENERATOR)) {
            w->failed=1;return;
        }
        snprintf(link,sizeof(link),"xl_pkg_%016llx_%u",(unsigned long long)w->binding,(unsigned)target->symbol);
        error=target->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE?2:1;stub=1;
    }
    size_t textSize=node->text?strlen(node->text):0;
    XL6_AstKind kind=node->kind==XL6_AST_EXPORT_FUNC || asyncDeclaration?XL6_AST_FUNC:node->kind;
    xl6CompiledU32(w,(uint32_t)kind);xl6CompiledU32(w,(node->flags & (asyncDeclaration?~XL6_AST_FLAG_ASYNC:~0u)) |
        ((node->kind==XL6_AST_EXPORT_FUNC || node->kind==XL6_AST_EXPORT_CONST)?XL6_AST_FLAG_PUBLIC:0));
    xl6CompiledU32(w,(uint32_t)textSize);xl6CompiledU32(w,(uint32_t)node->literalSize);
    xl6CompiledU32(w,(uint32_t)node->childCount);xl6CompiledU32(w,(uint32_t)strlen(link));
    xl6CompiledU32(w,error);xl6CompiledU32(w,node->literalScalar);xl6CompiledU32(w,(uint32_t)node->literalDecoded);
    xl6CompiledWrite(w,node->text,textSize);xl6CompiledWrite(w,node->literalData,node->literalSize);
    xl6CompiledWrite(w,link,strlen(link));
    const XL6_IrCallTarget* parameterScope=w->signature;
    if(node->kind==XL6_AST_PARAM)w->signature=NULL;
    for(size_t i=0;i<node->childCount;++i) {
        if(stub && i==(node->kind==XL6_AST_CREATE?0u:1u)) {
            const XL6_IrCallTarget* previous=w->signature;w->signature=bindingTarget;
            xl6CompiledNode(w,node->children[i],depth+1,0);w->signature=previous;
        } else if(stub && i==(node->kind==XL6_AST_CREATE?1u:3u)) {
            XL6_AstNode empty={0};empty.kind=XL6_AST_BLOCK;xl6CompiledNode(w,&empty,depth+1,0);
        } else if(normalizeAsync && i==2) {
            /* Publish the host launcher as an ordinary Future-returning ABI;
             * no worker, frame program or scheduler is duplicated in scripts. */
            const XL6_AstNode* returns=node->children[i];
            if(returns && returns->childCount>1){w->failed=1;return;}
            XL6_AstNode nullType={0},args={0},future={0},result={0};
            nullType.kind=XL6_AST_TYPE;nullType.text="null";
            XL6_AstNode* item=returns && returns->childCount?returns->children[0]:&nullType;
            args.kind=XL6_AST_LIST;args.children=&item;args.childCount=1;
            XL6_AstNode* argList=&args;
            future.kind=XL6_AST_TYPE;future.text="future";future.children=&argList;future.childCount=1;
            XL6_AstNode* futureType=&future;
            result.kind=XL6_AST_RETURN_TYPES;result.children=&futureType;result.childCount=1;
            xl6CompiledNode(w,&result,depth+1,0);
        } else xl6CompiledNode(w,node->children[i],depth+1,generic);
    }
    if(node->kind==XL6_AST_PARAM)w->signature=parameterScope;
}

static void xl6CompiledPlansFree(XL6_IrCompiledPackage* packages,size_t count)
{
    for(size_t i=0;i<count;++i) {
        xrtFree(packages[i].name);xrtFree(packages[i].version);xrtFree(packages[i].abiVersion);
        xrtFree(packages[i].interfaceData);xrtFree(packages[i].entries);xrtFree(packages[i].dependencies);
    }
    xrtFree(packages);
}

int xl6CompileUnitPrepareCompiledPackages(XL6_CompileUnit* unit)
{
    if(unit->ir.compiledPackages)return 1;
    size_t count=0;
    for(size_t i=0;i<unit->importDeps.packageInfoCount;++i)count+=unit->importDeps.packageInfos[i].compiledRoot==1;
    if(!count)return 1;
    if(count>XL_COMPILED_PACKAGE_MAX_PACKAGES) {
        xl6DiagAdd(&unit->parse.diags,1,1,"compiled package count exceeds its bound");return 0;
    }
    XL6_IrCompiledPackage* packages=(XL6_IrCompiledPackage*)xrtCalloc(count,sizeof(*packages));
    if(!packages)return 0;
    size_t index=0;
    for(size_t i=0;i<unit->importDeps.packageInfoCount;++i) {
        const XL6_ImportPackageInfo* info=&unit->importDeps.packageInfos[i];
        if(info->compiledRoot!=1)continue;
        XL6_IrCompiledPackage* out=&packages[index++];
        out->name=xrtStrDup(info->name);out->version=xrtStrDup(info->version);out->abiVersion=xrtStrDup(info->abiVersion);
        if(!out->name || !out->version || !out->abiVersion)goto failed;
        out->bindingId=xl6CompiledHash(info->name,strlen(info->name));
        XL6_CompiledWriter writer={0};writer.module=&unit->ir;writer.binding=out->bindingId;
        xl6CompiledWrite(&writer,"XLP1",4);
        XL6_AstNode root={0};root.kind=XL6_AST_MODULE;
        for(size_t n=0;n<unit->parse.root->childCount;++n) {
            XL6_AstNode* node=unit->parse.root->children[n];
            int owns=xl6CompiledOwnsNode(info,node);
            if(owns<0){writer.failed=1;break;}
            if(!owns)continue;
            if(node->kind==XL6_AST_IMPORT && (!node->text ||
                (strcmp(node->text,"xlang") && strcmp(node->text,"tcc") &&
                 strcmp(node->text,"xlang.compiler") && strcmp(node->text,"concurrent"))))continue;
            if(node->kind==XL6_AST_VAR_DECL || node->kind==XL6_AST_MULTI_VAR_DECL) {
                if(node->flags&XL6_AST_FLAG_PUBLIC) {
                    writer.reason="compiled packages require accessors for mutable globals";writer.failed=1;
                }
                continue;
            }
            if(node->kind==XL6_AST_EXPORT_VAR || node->kind==XL6_AST_EXTERN_VAR) {
                writer.reason="compiled packages require accessors for mutable globals";writer.failed=1;continue;
            }
            if(node->kind==XL6_AST_CONST_DECL || node->kind==XL6_AST_EXPORT_CONST) {
                size_t valueIndex=node->kind==XL6_AST_EXPORT_CONST?2u:1u;
                if(node->childCount<=valueIndex || !xl6CompiledLiteralConstant(node->children[valueIndex],0)) {
                    if(node->kind==XL6_AST_EXPORT_CONST || (node->flags&XL6_AST_FLAG_PUBLIC)) {
                        writer.reason="compiled packages require literal immutable public constants";writer.failed=1;
                    }
                    continue;
                }
            }
            /* Package initialization ran in the host. Never replay executable
             * top-level statements in each importing script environment. */
            if(!xl6CompiledInterfaceDeclaration(node->kind))continue;
            size_t before=root.childCount;xl6AstAddChild(&root,node);
            if(root.childCount!=before+1)writer.failed=1;
        }
        /* Dependency imports are declarations, not filesystem snapshots. */
        size_t ownCount=root.childCount;
        for(size_t d=0;d<info->dependencies.count;++d) {
            XL6_AstNode* imp=xl6AstNewText(XL6_AST_IMPORT,info->dependencies.items[d],strlen(info->dependencies.items[d]),NULL);
            if(!imp || !imp->text){xl6AstFree(imp);writer.failed=1;break;}
            size_t before=root.childCount;xl6AstAddChild(&root,imp);
            if(root.childCount!=before+1){xl6AstFree(imp);writer.failed=1;break;}
        }
        xl6CompiledNode(&writer,&root,0,0);
        for(size_t d=ownCount;d<root.childCount;++d)xl6AstFree(root.children[d]);
        xrtFree(root.children);
        if(writer.failed || !xl6CompiledPackageInterfaceValid(writer.data,writer.size)) {
            xrtFree(writer.data);
            xl6DiagAdd(&unit->parse.diags,1,1,writer.reason?writer.reason:
                "compiled package interface is unsupported or exceeds its bound");goto failed;
        }
        out->interfaceData=writer.data;out->interfaceSize=writer.size;out->interfaceHash=xl6CompiledHash(writer.data,writer.size);
        for(size_t t=0;t<unit->ir.callTargetCount;++t) {
            const XL6_IrCallTarget* target=&unit->ir.callTargets[t];
            const XL6_AstNode* declaration=xl6CompiledDeclaration(unit->parse.root,target->symbol);
            if(!declaration) {
                for(size_t n=0;n<unit->parse.root->childCount && !declaration;++n) {
                    const XL6_AstNode* candidate=unit->parse.root->children[n];
                    if(candidate->semanticSymbolId==target->symbol)declaration=candidate;
                }
            }
            if(!declaration || target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
            int owns=xl6CompiledOwnsNode(info,declaration);
            if(owns<0)goto failed;
            if(!owns)continue;
            int duplicate=0;
            if(target->cName)for(size_t e=0;e<out->entryCount;++e) {
                const XL6_IrCallTarget* old=xl6IrCallTargetGet(&unit->ir,out->entries[e]);
                if(old->cName && !strcmp(old->cName,target->cName))duplicate=1;
            }
            if(duplicate)continue;
            unsigned int* entries=(unsigned int*)xrtRealloc(out->entries,(out->entryCount+1)*sizeof(*entries));
            if(!entries)goto failed;
            out->entries=entries;entries[out->entryCount++]=target->symbol;
        }
    }
    for(size_t p=0;p<count;++p) {
        const XL6_ImportPackageInfo* info=NULL;
        for(size_t i=0;i<unit->importDeps.packageInfoCount;++i)
            if(!strcmp(unit->importDeps.packageInfos[i].name,packages[p].name))info=&unit->importDeps.packageInfos[i];
        if(info->dependencies.count) {
            packages[p].dependencies=(size_t*)xrtCalloc(info->dependencies.count,sizeof(size_t));
            if(!packages[p].dependencies)goto failed;
            for(size_t d=0;d<info->dependencies.count;++d) {
                size_t q=0;for(;q<count;++q)if(!strcmp(packages[q].name,info->dependencies.items[d]))break;
                if(q==count)goto failed;
                packages[p].dependencies[packages[p].dependencyCount++]=q;
            }
        }
    }
    unit->ir.compiledPackages=packages;unit->ir.compiledPackageCount=count;
    return 1;
failed:
    xl6CompiledPlansFree(packages,count);
    return 0;
}
