static int xl6IrCgenCompiledPackagePrototypes(XL6_Output* out,
    const XL6_IrModule* module, XL6_DiagList* diags)
{
    for (size_t i = 0; i < module->callTargetCount; ++i) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        const XL6_IrNode* node = xl6IrNodeGet(module, target->declaration);
        if (!node || !node->compiledPackageLink || target->reachability == XL6_IR_REACHABILITY_UNREACHABLE) continue;
        const char* result = target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
            ? xl6IrCgenErrorResultType(module, target->returnType)
            : target->returnType ? xl6IrCgenType(module, target->returnType) : "void";
        if (!result || target->kind != XL6_IR_CALL_TARGET_INTERNAL)
            return xl6IrCgenDiag(diags,module,target->declaration,"compiled package entry has no exact language ABI");
        xl6OutputPrintf(out,"#define xlir_fn_%u %s\nextern %s %s(",
            (unsigned)target->symbol,node->compiledPackageLink,result,node->compiledPackageLink);
        if (!target->parameterCount) xl6OutputPuts("void",out);
        for(size_t p=0;p<target->parameterCount;++p) {
            const char* type=xl6IrCgenType(module,target->parameterTypes[p]);
            if(!type)return 0;
            xl6OutputPrintf(out,"%s%s%s",p?", ":"",type,
                xl6IrCgenParameterAbiIsIndirect(target->parameterAbis[p])?"*":"");
        }
        xl6OutputPuts(");\n",out);
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenCompiledPackageDefinitions(XL6_Output* out,
    const XL6_IrModule* module, int xlangRuntime, int processResident, XL6_DiagList* diags)
{
    if (!xlangRuntime && !module->compiledPackageCount) return 1;
    if(module->compiledPackageCount && !processResident)
        return xl6IrCgenDiag(diags,module,module->root,
            "compiled package providers require a process-resident executable deployment");
    for (size_t p=0;p<module->compiledPackageCount;++p) {
        const XL6_IrCompiledPackage* package=&module->compiledPackages[p];
        xl6OutputPrintf(out,"static const XL_CompiledPackage xlir_package_%lu;\n",(unsigned long)p);
        xl6OutputPrintf(out,"static const unsigned char xlir_package_interface_%lu[] = {",(unsigned long)p);
        for(size_t b=0;b<package->interfaceSize;++b)xl6OutputPrintf(out,"%s%u",b?",":"",(unsigned)package->interfaceData[b]);
        xl6OutputPuts("};\n",out);
        if(package->entryCount) {
            xl6OutputPrintf(out,"static const XL_PackageEntry xlir_package_entries_%lu[] = {\n",(unsigned long)p);
            for(size_t e=0;e<package->entryCount;++e) {
                const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,package->entries[e]);
                if(!target)return xl6IrCgenDiag(diags,module,module->root,"compiled package target is missing");
                char name[96];
                if(target->kind==XL6_IR_CALL_TARGET_INTERNAL || target->kind==XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
                    target->kind==XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) {
                    snprintf(name,sizeof(name),"xl_pkg_%016llx_%u",(unsigned long long)package->bindingId,(unsigned)target->symbol);
                    if(target->kind==XL6_IR_CALL_TARGET_INTERNAL)
                        xl6OutputPrintf(out,"    {\"%s\", (const void*)xlir_fn_%u},\n",name,(unsigned)target->symbol);
                    else xl6OutputPrintf(out,"    {\"%s\", (const void*)xlir_%s_start_%u},\n",name,
                        target->kind==XL6_IR_CALL_TARGET_INTERNAL_ASYNC?"async":"generator",(unsigned)target->symbol);
                } else {
                    xl6OutputPuts("    {",out);xl6IrCgenEmitCStringLiteral(out,target->cName);
                    xl6OutputPrintf(out,", (const void*)%s},\n",target->cName);
                }
            }
            xl6OutputPuts("};\n",out);
        }
    }
    for(size_t p=0;p<module->compiledPackageCount;++p) {
        const XL6_IrCompiledPackage* package=&module->compiledPackages[p];
        if(package->dependencyCount) {
            xl6OutputPrintf(out,"static const XL_CompiledPackage* const xlir_package_deps_%lu[] = {",(unsigned long)p);
            for(size_t d=0;d<package->dependencyCount;++d)
                xl6OutputPrintf(out,"%s&xlir_package_%lu",d?",":"",(unsigned long)package->dependencies[d]);
            xl6OutputPuts("};\n",out);
        }
        xl6OutputPrintf(out,"static const XL_CompiledPackage xlir_package_%lu = {sizeof(XL_CompiledPackage), XL_COMPILED_PACKAGE_VERSION, sizeof(void*), XL_COMPILED_PACKAGE_COMPILER_ABI, (const void*)xrtValueRelease, ",(unsigned long)p);
        xl6IrCgenEmitCStringLiteral(out,package->name);xl6OutputPuts(", ",out);
        xl6IrCgenEmitCStringLiteral(out,package->version);xl6OutputPuts(", ",out);
        xl6IrCgenEmitCStringLiteral(out,package->abiVersion);
        xl6OutputPrintf(out,", xlir_package_interface_%lu, %lu, UINT64_C(%llu), ",
            (unsigned long)p,(unsigned long)package->interfaceSize,(unsigned long long)package->interfaceHash);
        if(package->entryCount)xl6OutputPrintf(out,"xlir_package_entries_%lu, %lu, ",(unsigned long)p,(unsigned long)package->entryCount);
        else xl6OutputPuts("NULL, 0, ",out);
        if(package->dependencyCount)xl6OutputPrintf(out,"xlir_package_deps_%lu, %lu};\n",(unsigned long)p,(unsigned long)package->dependencyCount);
        else xl6OutputPuts("NULL, 0};\n",out);
    }
    xl6OutputPuts("static const XL_CompiledPackage* xlir_compiled_package_lookup(const char* name)\n{\n",out);
    /* Only the executable deployment contract certifies process residency.
     * Static executables also have semantic code owners; owner.size alone
     * cannot distinguish them from unloadable JIT or shared-library hosts. */
    for(size_t p=0;processResident && p<module->compiledPackageCount;++p) {
        xl6OutputPuts("    if (name && !strcmp(name, ",out);
        xl6IrCgenEmitCStringLiteral(out,module->compiledPackages[p].name);
        xl6OutputPrintf(out,")) return &xlir_package_%lu;\n",(unsigned long)p);
    }
    xl6OutputPuts("    (void)name; return NULL;\n}\n",out);
    return !xl6OutputHasFailed(out);
}
