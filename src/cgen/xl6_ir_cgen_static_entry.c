/* Executable deployment glue only. The compiler supplies a verified exact
 * capability list; graph algorithms and checked lifecycle stay resident. */
static int xl6IrCgenEntryIdentifier(const char* name)
{
    if(!name || !((*name>='a' && *name<='z') || (*name>='A' && *name<='Z') || *name=='_'))return 0;
    for(++name;*name;++name)
        if(!((*name>='a' && *name<='z') || (*name>='A' && *name<='Z') || (*name>='0' && *name<='9') || *name=='_'))return 0;
    return 1;
}
static int xl6IrCgenVerifyEntryContract(const XL6_IrModule* module,XL6_DiagList* diags,const XL6_IrCgenOptions* options)
{
    if(!options || !options->emitMainWrapper)return 1;
    const XL6_IrCgenEntryContractV1* entry=options->entryContract;
    if(!entry || entry->size!=sizeof(*entry) || (entry->domainCount && !entry->domains) ||
        (!entry->managed && (entry->domainCount || entry->discoverSymbol)) ||
        (entry->managed && (module->abi.codeOwnerAbi!=XL6_IR_MODULE_CODE_OWNER_BIND_BEFORE_INIT ||
          module->abi.retirementAbi==XL6_IR_MODULE_RETIREMENT_NONE)) ||
        (entry->discoverSymbol && !xl6IrCgenEntryIdentifier(entry->discoverSymbol)))
        return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR executable deployment contract is unresolved");
    for(size_t i=0;i<entry->domainCount;++i){
        if(!xl6IrCgenEntryIdentifier(entry->domains[i]))
            return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR executable capability identifier is invalid");
        for(size_t j=0;j<i;++j)if(!strcmp(entry->domains[i],entry->domains[j]))
            return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR executable capability is duplicated");
    }
    return 1;
}
static int xl6IrCgenEmitStaticEntry(XL6_Output* out,const XL6_IrModule* module,const XL6_IrCgenEntryContractV1* entry)
{
    xl6OutputPuts("#include <__xl_runtime/xl6_static_module.h>\n"
        "#include <__xl_runtime/xl6_ownership_catalog.h>\n"
        "static XL6_StaticModule* xlir_entry_module;\n"
        "static XL6_OwnershipCatalog* xlir_entry_catalog;\n",out);
    for(size_t i=0;i<entry->domainCount;++i)
        xl6OutputPrintf(out,"extern const XL6_OwnershipDomainV1* %s(void);\n",entry->domains[i]);
    if(entry->discoverSymbol)
        xl6OutputPrintf(out,"extern bool %s(const XL6_CodeOwner*, XL6_ModuleOwnershipVisit, void*);\n",entry->discoverSymbol);
    if(module->abi.readBoundaryAbi==XL6_IR_MODULE_READ_BOUNDARY_RESIDENT_HOST_BIND_V1)
        xl6OutputPuts("static int xlir_entry_read_begin(XL6_OwnershipReadScopeV1* scope)\n"
            "{ return xl6StaticModuleReadBeginV1(xlir_entry_module, scope); }\n"
            "static bool xlir_entry_read_admits(const XL6_OwnershipReadScopeV1* scope, xrtownershipref ref)\n"
            "{ return xl6StaticModuleReadAdmitsV1(xlir_entry_module, scope, ref); }\n"
            "static void xlir_entry_read_end(XL6_OwnershipReadScopeV1* scope)\n"
            "{ xl6StaticModuleReadEndV1(xlir_entry_module, scope); }\n",out);
    xl6OutputPuts("int main(int argc, char** argv)\n{\n"
        "    xvalue* arg = NULL; int code = 1, ok = 0;\n"
        "    xl6_error_scope cleanup = {0}; xl6_error_result outcome;\n",out);
    if(entry->domainCount){
        xl6OutputPuts("    const XL6_OwnershipDomainV1* domains[] = {\n",out);
        for(size_t i=0;i<entry->domainCount;++i)xl6OutputPrintf(out,"        %s(),\n",entry->domains[i]);
        xl6OutputPuts("    };\n    xlir_entry_catalog = xl6OwnershipCatalogCreateV1(domains, sizeof(domains)/sizeof(*domains));\n",out);
    }else xl6OutputPuts("    xlir_entry_catalog = xl6OwnershipCatalogCreateV1(NULL, 0);\n",out);
    xl6OutputPuts("    if (!xlir_entry_catalog) goto entry_cleanup;\n"
        "    XL6_StaticModuleContractV1 deployment = {sizeof(deployment),\n"
        "        &xlir_module_retirement_graph, xlir_host_prepare_retirement, ",out);
    xl6OutputPuts(module->abi.retirementAbi==XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4
        ? "xl_script_module_retire_step,\n" : "NULL,\n",out);
    xl6OutputPuts("        xl6OwnershipCatalogResolveV1, xlir_entry_catalog,\n"
        "        xl_script_module_ownership_read_v1(), xl6OwnershipCatalogAdmitsReadV1, ",out);
    xl6OutputPrintf(out,"%s};\n",entry->discoverSymbol ? entry->discoverSymbol : "NULL");
    xl6OutputPuts("    const XL6_StaticModuleContractV2 checked_deployment = {sizeof(checked_deployment), &deployment, xl6OwnershipCatalogProbeV1};\n"
        "    xlir_entry_module = xl6StaticModuleCreateV2(&checked_deployment);\n"
        "    if (!xlir_entry_module || !xl_script_bind_code_owner(xl6StaticModuleOwnerV1(xlir_entry_module))) goto entry_cleanup;\n",out);
    if(module->abi.readBoundaryAbi==XL6_IR_MODULE_READ_BOUNDARY_RESIDENT_HOST_BIND_V1)
        xl6OutputPuts("    const XL6_OwnershipReadBoundaryV1 read = {sizeof(read),\n"
            "        xlir_entry_read_begin, xlir_entry_read_admits, xlir_entry_read_end};\n"
            "    if (!xl_script_bind_ownership_read_v1(&read)) goto entry_cleanup;\n",out);
    if(module->abi.entryKind==XL6_IR_MODULE_ENTRY_XLANG_MAIN){
        const XL6_IrType* argument=xl6IrTypeGet(&module->types,module->abi.entryArgumentType);
        xl6OutputPuts("    extern xvalue* xl6ArrayOsArgvCreateV1(uint64_t, uint64_t, int, char* const*);\n",out);
        xl6OutputPrintf(out,"    arg = xl6ArrayOsArgvCreateV1(0x%016llXULL, 0x%016llXULL, argc, argv);\n",
            (unsigned long long)argument->typeValueId,(unsigned long long)module->abi.entryArgumentElementIdentity);
        xl6OutputPuts("    if (!arg) goto entry_cleanup;\n",out);
    }else xl6OutputPuts("    (void)argc; (void)argv;\n",out);
    xl6OutputPuts("    ok = xl_script_main_checked(arg, &code);\n"
        "entry_cleanup:\n"
        "    if (!xl6ErrorScopeBeginCleanup(&cleanup, !ok)) return 1;\n"
        "    xrtValueRelease(arg);\n"
        "    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "    if (xlir_entry_module) {\n",out);
    /* Process exit owns the application's service shutdown. Settle accepted
     * work before inspecting the final graph; a module unload/read preflight
     * retains its separate, non-destructive ownership contract. */
    if(module->abi.retirementAbi==XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4)
        xl6OutputPuts("        if (xlir_module_prepare_resources(true, true) != XL6_MODULE_PREPARE_READY) { xl6ErrorScopeCaptureFailure(&cleanup); goto entry_cleanup_done; }\n",out);
    xl6OutputPuts(
        "        XL6_StaticModuleResultV1 retired = xl6StaticModuleDrainForV1(xlir_entry_module, -1);\n"
        "        if (!retired.ready || !retired.ok) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "        if (retired.ready && !xl6StaticModuleDestroyV1(&xlir_entry_module)) xl6ErrorScopeCaptureFailure(&cleanup);\n"
        "    }\n",out);
    if(module->abi.retirementAbi==XL6_IR_MODULE_RETIREMENT_RETRYABLE_SERVICES_V4)
        xl6OutputPuts("entry_cleanup_done:;\n",out);
    xl6OutputPuts(
        "    if (!xlir_entry_module) { xl6OwnershipCatalogDestroyV1(xlir_entry_catalog); xlir_entry_catalog = NULL; }\n"
        "    outcome = xl6ErrorScopeEnd(&cleanup); xrtSetErrorTake(outcome.error);\n"
        "    if (!outcome.ok) { xl_script_print_uncaught(); xl_script_clear_error(); return 1; }\n"
        "    return code;\n}\n",out);
    return !xl6OutputHasFailed(out);
}
