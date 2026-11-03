static int xl6IrCompiledLinkValid(const char* name)
{
    if(!name || strncmp(name,"xl_pkg_",7))return 0;
    size_t length=0;
    for(;name[length];++length) {
        unsigned char c=(unsigned char)name[length];
        if(length>=95 || !(c=='_' || (c>='a'&&c<='z') ||
            (c>='A'&&c<='Z') || (length && c>='0'&&c<='9')))return 0;
    }
    return length>7;
}

static int xl6IrVerifyCompiledPackages(const XL6_IrModule* module,XL6_DiagList* diags)
{
    size_t lifetimeNodes=0,boundLifetimes=0;
    if(module->compiledPackageCount>64 ||
        (!!module->compiledPackageCount != !!module->compiledPackages))
        return xl6IrDiag(diags,NULL,"XLIR compiled package arena is malformed");
    for(size_t n=0;n<module->nodeCount;++n) {
        const XL6_IrNode* node=&module->nodes[n];
        if(!node->compiledPackageLink) {
            if(node->sourceKind==XL6_AST_PARAM && XL6_COMPILED_PARAM_LIFETIME_VALID(node->compiledPackageError)) {
                ++lifetimeNodes;continue;
            }
            if(node->compiledPackageError)
                return xl6IrDiag(diags,node,"XLIR package error ABI has no host binding");
            continue;
        }
        size_t bodyIndex=node->sourceKind==XL6_AST_CREATE?1u:3u;
        const XL6_IrNode* body=node->childCount>bodyIndex
            ? xl6IrNodeGet(module,module->children[node->firstChild+bodyIndex]):NULL;
        if(!xl6IrCompiledLinkValid(node->compiledPackageLink) || node->compiledPackageRoot ||
            node->kind!=XL6_IR_NODE_FUNC_DECL ||
            (node->sourceKind!=XL6_AST_FUNC && node->sourceKind!=XL6_AST_GET && node->sourceKind!=XL6_AST_LET &&
             node->sourceKind!=XL6_AST_CREATE) ||
            (node->compiledPackageError!=1 && node->compiledPackageError!=2) ||
            !body || body->kind!=XL6_IR_NODE_BLOCK || body->childCount)
            return xl6IrDiag(diags,node,"XLIR compiled package binding has no exact body-free declaration ABI");
        size_t paramsIndex=node->sourceKind==XL6_AST_CREATE?0u:1u;
        const XL6_IrNode* params=node->childCount>paramsIndex
            ? xl6IrNodeGet(module,module->children[node->firstChild+paramsIndex]):NULL;
        if(!params)return xl6IrDiag(diags,node,"XLIR compiled package has no parameter contract");
        for(size_t p=0;p<params->childCount;++p) {
            const XL6_IrNode* parameter=xl6IrNodeGet(module,module->children[params->firstChild+p]);
            if(!parameter || parameter->sourceKind!=XL6_AST_PARAM ||
                !XL6_COMPILED_PARAM_LIFETIME_VALID(parameter->compiledPackageError))
                return xl6IrDiag(diags,node,"XLIR compiled package parameter lifetime is not frozen");
            ++boundLifetimes;
        }
        if(module->stage>=XL6_IR_STAGE_LOWERED) {
            const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,node->symbol);
            if(target && (target->kind!=XL6_IR_CALL_TARGET_INTERNAL ||
                target->errorAbi!=(node->compiledPackageError==2
                    ? XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE)))
                return xl6IrDiag(diags,node,"XLIR compiled package target disagrees with its host ABI");
        }
    }
    if(lifetimeNodes!=boundLifetimes)
        return xl6IrDiag(diags,NULL,"XLIR compiled parameter lifetime has no unique host declaration");
    uint64_t dependencies[64]={0};
    for(size_t p=0;p<module->compiledPackageCount;++p) {
        const XL6_IrCompiledPackage* package=&module->compiledPackages[p];
        if(!package->name || !package->version || !package->abiVersion ||
            !package->interfaceData || package->interfaceSize<40 || package->interfaceSize>4u*1024u*1024u ||
            memcmp(package->interfaceData,"XLP1",4) || package->entryCount>65536 ||
            (package->entryCount && !package->entries) || package->dependencyCount>64 ||
            (package->dependencyCount && !package->dependencies))
            return xl6IrDiag(diags,NULL,"XLIR compiled package descriptor is malformed");
        uint64_t hash=UINT64_C(14695981039346656037);
        for(size_t b=0;b<package->interfaceSize;++b)hash=(hash^package->interfaceData[b])*UINT64_C(1099511628211);
        if(hash!=package->interfaceHash)
            return xl6IrDiag(diags,NULL,"XLIR compiled package interface checksum changed");
        for(size_t q=0;q<p;++q)
            if(!strcmp(package->name,module->compiledPackages[q].name))
                return xl6IrDiag(diags,NULL,"XLIR compiled package identities are not unique");
        for(size_t e=0;e<package->entryCount;++e) {
            const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,package->entries[e]);
            if(!target || target->reachability==XL6_IR_REACHABILITY_UNREACHABLE ||
                (target->kind!=XL6_IR_CALL_TARGET_INTERNAL &&
                 target->kind!=XL6_IR_CALL_TARGET_INTERNAL_ASYNC && target->kind!=XL6_IR_CALL_TARGET_INTERNAL_GENERATOR &&
                 target->kind!=XL6_IR_CALL_TARGET_EXTERN_C && target->kind!=XL6_IR_CALL_TARGET_NATIVE_FACTORY) ||
                ((target->kind==XL6_IR_CALL_TARGET_EXTERN_C || target->kind==XL6_IR_CALL_TARGET_NATIVE_FACTORY) && !target->cName))
                return xl6IrDiag(diags,NULL,"XLIR compiled package entry has no resident callable implementation");
        }
        for(size_t d=0;d<package->dependencyCount;++d) {
            size_t q=package->dependencies[d];
            if(q>=module->compiledPackageCount || (dependencies[p]&(UINT64_C(1)<<q)))
                return xl6IrDiag(diags,NULL,"XLIR compiled package dependency is invalid or duplicated");
            dependencies[p]|=UINT64_C(1)<<q;
        }
    }
    for(size_t q=0;q<module->compiledPackageCount;++q)
        for(size_t p=0;p<module->compiledPackageCount;++p)
            if(dependencies[p]&(UINT64_C(1)<<q))dependencies[p]|=dependencies[q];
    for(size_t p=0;p<module->compiledPackageCount;++p)
        if(dependencies[p]&(UINT64_C(1)<<p))
            return xl6IrDiag(diags,NULL,"XLIR compiled package dependency cycle");
    return 1;
}
