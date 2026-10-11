/* Physical adapters for the verified LOCAL restore graph. No field traversal,
 * semantic assignment, inferred clone, nominal registry or source lookup. */
static int xl6IrCgenEmitRestoreCloneAdapter(XL6_Output* out,const XL6_IrModule* module,
    size_t root,size_t node,XL6_IrTypeId type,XL6_IrErrorAbi error,XL6_SymbolId symbol,
    const char* suffix,XL6_DiagList* diags)
{
    if (error==XL6_IR_ERROR_ABI_MAY_RAISE)
        xl6OutputPrintf(out,"static xvalue* xlir_restore_r%lu_n%lu_%s(const xvalue* value)\n{\n"
            "    %s outcome = xlir_fn_%u((xvalue*)value);\n"
            "    if (!outcome.ok) { xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL; }\n"
            "    return outcome.value;\n}\n",(unsigned long)root,(unsigned long)node,suffix,
            xl6IrCgenErrorResultType(module,type),(unsigned)symbol);
    else if (error==XL6_IR_ERROR_ABI_NO_RAISE)
        xl6OutputPrintf(out,"static xvalue* xlir_restore_r%lu_n%lu_%s(const xvalue* value)\n"
            "{ return xlir_fn_%u((xvalue*)value); }\n",(unsigned long)root,(unsigned long)node,suffix,(unsigned)symbol);
    else return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR restore clone has no physical outcome ABI");
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitRestoreIsolation(XL6_Output* out,const XL6_IrModule* module,
    size_t root,size_t node,XL6_IrTypeId type,XL6_IrPersistenceIsolationAbi abi,unsigned providers,
    const char* suffix,XL6_DiagList* diags)
{
    switch (abi) {
    case XL6_IR_PERSIST_ISOLATION_CHECKED_REJECT: xl6OutputPuts("NULL",out); break;
    case XL6_IR_PERSIST_ISOLATION_RUNTIME_SHALLOW:
    case XL6_IR_PERSIST_ISOLATION_RUNTIME_DEEP: {
        if (providers) {
            xl6OutputPrintf(out,"xlir_restore_r%lu_n%lu_%s",(unsigned long)root,(unsigned long)node,suffix);
            break;
        }
        const char* symbol=xl6IrCgenRestoreIsolationSymbol(module,type,abi);
        if (!symbol) return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR restore copy provider is unresolved");
        xl6OutputPuts(symbol,out); break;
    }
    case XL6_IR_PERSIST_ISOLATION_INTERNAL_CALL:
        xl6OutputPrintf(out,"xlir_restore_r%lu_n%lu_%s",(unsigned long)root,(unsigned long)node,suffix); break;
    default: return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR restore isolation ABI is unresolved");
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenEmitRestoreGraphCallbacks(XL6_Output* out,
    const XL6_IrModule* module,size_t rootIndex,XL6_DiagList* diags)
{
    const XL6_IrPersistenceRestoreRoot* root=&module->persistenceRestoreRoots[rootIndex];
    for (size_t i=0;i<root->nodeCount;++i) {
        const XL6_IrPersistenceRestoreNode* node=&module->persistenceRestoreNodes[root->firstNode+i];
        if (node->abi!=XL6_IR_RESTORE_NODE_CLASS) continue;
        /* Only physical callback adaptation: the verified graph capability
         * is data, and the single resident memo performs all traversal. */
        for (unsigned open=0;open<2;++open) {
            unsigned providers=open ? node->openIsolationGraphProviders : node->isolationGraphProviders;
            if (!providers) continue;
            xl6OutputPrintf(out,"static xvalue* xlir_restore_r%lu_n%lu_%s(const xvalue* value)\n"
                "{ return xl6ValueDeepCloneNativeV1(value,",(unsigned long)rootIndex,(unsigned long)i,
                open ? "open_clone" : "clone");
            if (!xl6IrCgenEmitValueGraphPlan(out,providers,0)) return 0;
            xl6OutputPuts("); }\n",out);
        }
        if (node->openClassLayout!=SIZE_MAX) {
            const XL6_IrPersistenceRestoreSchema* schema=&module->persistenceRestoreSchemas[node->schema];
            int first=1;
            for (size_t j=0;j<i;++j)
                if (module->persistenceRestoreNodes[root->firstNode+j].schema==node->schema) first=0;
            if (first && schema->claimCount) {
                xl6OutputPrintf(out,"static const xstrview xlir_restore_r%lu_s%lu_claims[] = {\n",
                    (unsigned long)rootIndex,(unsigned long)node->schema);
                for (size_t j=0;j<schema->claimCount;++j) {
                    const XL6_IrPersistenceRestoreClaim* claim=&module->persistenceRestoreClaims[schema->firstClaim+j];
                    xl6OutputPuts("    ",out);
                    xl6IrCgenEmitPersistenceByteView(out,claim->key,claim->keyAbi==XL6_IR_PERSIST_PROJECT_KEY_C_LITERAL);
                    xl6OutputPuts(",\n",out);
                }
                xl6OutputPuts("};\n",out);
            }
            if (node->openIsolationAbi==XL6_IR_PERSIST_ISOLATION_INTERNAL_CALL &&
                !xl6IrCgenEmitRestoreCloneAdapter(out,module,rootIndex,i,module->classLayouts[node->openClassLayout].type,
                    node->openCloneErrorAbi,node->openCloneSymbol,"open_clone",diags)) return 0;
        }
        if (node->defaultFunction) {
            xl6OutputPrintf(out,"static xvalue* xlir_restore_r%lu_n%lu_default(void)\n{\n"
                "    %s outcome = xlir_fn_%u();\n"
                "    if (!outcome.ok) { xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL; }\n"
                "    return outcome.value;\n}\n",(unsigned long)rootIndex,(unsigned long)i,
                xl6IrCgenErrorResultType(module,node->payloadType),(unsigned)node->defaultSymbol);
        }
        if (node->isolationAbi==XL6_IR_PERSIST_ISOLATION_INTERNAL_CALL &&
            !xl6IrCgenEmitRestoreCloneAdapter(out,module,rootIndex,i,node->payloadType,
                node->cloneErrorAbi,node->cloneSymbol,"clone",diags)) return 0;
    }
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitRestoreGraphPlan(XL6_Output* out,const XL6_IrModule* module,
    size_t rootIndex,XL6_DiagList* diags)
{
    const XL6_IrPersistenceRestoreRoot* root=&module->persistenceRestoreRoots[rootIndex];
    for (size_t i=0;i<root->nodeCount;++i) {
        const XL6_IrPersistenceRestoreNode* node=&module->persistenceRestoreNodes[root->firstNode+i];
        if (node->openClassLayout==SIZE_MAX) continue;
        const XL6_IrPersistenceRestoreSchema* schema=&module->persistenceRestoreSchemas[node->schema];
        const XL6_IrClassLayout* storage=&module->classLayouts[node->openClassLayout];
        const XL6_IrType* type=xl6IrTypeGet(&module->types,storage->type);
        xl6OutputPrintf(out,"    const XL6_RestoreOpenV1 open_%lu = {UINT64_C(0x%016llX),%u,",
            (unsigned long)i,(unsigned long long)type->typeValueId,(unsigned)storage->runtimeTypeTag);
        xl6IrCgenEmitPersistenceByteView(out,module->classFields[schema->dynamicField].name,0); xl6OutputPuts(",",out);
        xl6IrCgenEmitPersistenceByteView(out,module->classFields[schema->dynamicHandleField].name,0);
        xl6OutputPrintf(out,",%lu,",(unsigned long)schema->claimCount);
        if (schema->claimCount) xl6OutputPrintf(out,"xlir_restore_r%lu_s%lu_claims",(unsigned long)rootIndex,(unsigned long)node->schema);
        else xl6OutputPuts("NULL",out);
        xl6OutputPuts(",",out);
        if (!xl6IrCgenEmitRestoreIsolation(out,module,rootIndex,i,storage->type,node->openIsolationAbi,
            node->openIsolationGraphProviders,"open_clone",diags)) return 0;
        xl6OutputPuts("};\n",out);
    }
    xl6OutputPrintf(out,"    const XL6_RestoreObjectProviderV1* objects = %s();\n"
        "    const XL6_RestoreNodeV1 nodes[%lu] = {\n",xl6IrCgenRestoreObjectProviderSymbol(module,root),
        (unsigned long)root->nodeCount);
    for (size_t i=0;i<root->nodeCount;++i) {
        const XL6_IrPersistenceRestoreNode* node=&module->persistenceRestoreNodes[root->firstNode+i];
        xl6OutputPrintf(out,"        {.kind=%u,.flags=%u,.identity=0x%016llXULL,",
            node->abi==XL6_IR_RESTORE_NODE_SCALAR ? 1u :
            node->abi==XL6_IR_RESTORE_NODE_ARRAY || node->abi==XL6_IR_RESTORE_NODE_LIST ||
            node->abi==XL6_IR_RESTORE_NODE_SET || node->abi==XL6_IR_RESTORE_NODE_DICT ? 2u : 3u,
            (unsigned)node->flags,(unsigned long long)node->identity);
        if (node->abi==XL6_IR_RESTORE_NODE_SCALAR)
            xl6OutputPrintf(out,".as.scalar=%u",(unsigned)node->scalarRule);
        else if (node->abi==XL6_IR_RESTORE_NODE_ARRAY || node->abi==XL6_IR_RESTORE_NODE_LIST ||
                 node->abi==XL6_IR_RESTORE_NODE_SET || node->abi==XL6_IR_RESTORE_NODE_DICT)
            xl6OutputPrintf(out,".as.sequence={%lu,%lu,%s}",(unsigned long)node->seed,(unsigned long)node->element,
                node->abi==XL6_IR_RESTORE_NODE_LIST ? "xl6ListImportProviderV1()" :
                node->abi==XL6_IR_RESTORE_NODE_SET ? "xl6SetImportProviderV1()" :
                node->abi==XL6_IR_RESTORE_NODE_DICT ? "xl6DictImportProviderV1()" : "NULL");
        else if (node->abi==XL6_IR_RESTORE_NODE_CLASS) {
            const XL6_IrPersistenceRestoreSchema* schema=&module->persistenceRestoreSchemas[node->schema];
            const XL6_IrClassLayout* layout=&module->classLayouts[schema->classLayout];
            xl6OutputPuts(".as.object={",out);
            if (node->defaultFunction) xl6OutputPrintf(out,"xlir_restore_r%lu_n%lu_default",(unsigned long)rootIndex,(unsigned long)i);
            else xl6OutputPuts("NULL",out);
            xl6OutputPrintf(out,",%u,%lu,%lu,%s}",(unsigned)layout->runtimeTypeTag,
                (unsigned long)node->firstEdge,(unsigned long)node->edgeCount,
                node->openClassLayout==SIZE_MAX ? "objects" : "xl6RestoreOpenProviderV1()");
            xl6OutputPuts(",.clone=",out);
            if (!xl6IrCgenEmitRestoreIsolation(out,module,rootIndex,i,node->payloadType,node->isolationAbi,
                node->isolationGraphProviders,"clone",diags)) return 0;
            if (node->openClassLayout!=SIZE_MAX) xl6OutputPrintf(out,",.open=&open_%lu",(unsigned long)i);
        } else return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR closed restore node is unresolved");
        xl6OutputPuts("},\n",out);
    }
    xl6OutputPuts("    };\n",out);
    if (root->edgeCount) {
        xl6OutputPrintf(out,"    const XL6_RestoreFieldV1 fields[%lu] = {\n",(unsigned long)root->edgeCount);
        for (size_t i=0;i<root->edgeCount;++i) {
            const XL6_IrPersistenceRestoreEdge* edge=&module->persistenceRestoreEdges[root->firstEdge+i];
            const XL6_IrPersistenceRestoreField* field=&module->persistenceRestoreFields[edge->field];
            const XL6_IrClassField* storage=&module->classFields[field->classField];
            xl6OutputPuts("        {",out);
            xl6IrCgenEmitPersistenceByteView(out,field->sourceKey,field->sourceKeyAbi==XL6_IR_PERSIST_PROJECT_KEY_C_LITERAL);
            xl6OutputPuts(",",out); xl6IrCgenEmitPersistenceByteView(out,storage->name,0);
            xl6OutputPrintf(out,",%lu,%u},\n",(unsigned long)edge->node,
                (field->required ? 1u : 0u) | (field->assignAbi==XL6_IR_PERSIST_RESTORE_CLASS_IN_PLACE ? 2u : 0u));
        }
        xl6OutputPuts("    };\n",out);
    }
    xl6OutputPrintf(out,"    const XL6_RestorePlanV1 plan = {sizeof(plan),%lu,%lu,%lu,nodes,%s,%s,\n"
        "        xlir_ownership_read_boundary.size ? &xlir_ownership_read_boundary : NULL};\n",
        (unsigned long)root->nodeCount,(unsigned long)root->edgeCount,(unsigned long)root->root,
        root->edgeCount ? "fields" : "NULL",root->format==XL6_IR_PERSIST_FORMAT_JSON ? "true" : "false");
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitNativeRestoreHelpers(XL6_Output* out,const XL6_IrModule* module,
    uint32 format,int readUsed,XL6_DiagList* diags)
{
    int header=0;
    const char* name=format==XL6_IR_PERSIST_FORMAT_JSON ? "json" : "xlon";
    const char* parse=format==XL6_IR_PERSIST_FORMAT_JSON ? "xrtJsonParse" : "xrtXlonParse";
    const char* read=format==XL6_IR_PERSIST_FORMAT_JSON ? "xrtJsonParseFile" : "xrtXlonParseFile";
    for (size_t r=0;r<module->persistenceRestoreRootCount;++r) {
        const XL6_IrPersistenceRestoreRoot* root=&module->persistenceRestoreRoots[r];
        if (root->pending || root->format!=format) continue;
        if (!header++) xl6OutputPuts("#include <__xl_runtime/xl6_restore_class.h>\n"
            "extern xvalue* xl6ValueCloneV1(const xvalue*);\n"
            "extern xvalue* xl6ValueDeepCloneV1(const xvalue*);\n",out);
        if (root->seedCount)
            xl6OutputPuts("#include <__xl_runtime/xl6_array_admission.h>\n"
                "#include <__xl_runtime/xl6_array_erased.h>\n",out);
        for (size_t i=0;i<root->nodeCount;++i)
            if (module->persistenceRestoreNodes[root->firstNode+i].abi==XL6_IR_RESTORE_NODE_LIST) {
                xl6OutputPuts("#include <__xl_runtime/xl6_list.h>\n"
                    "#include <__xl_runtime/xl6_list_import_chain.h>\n",out); break;
            }
        for (size_t i=0;i<root->nodeCount;++i)
            if (module->persistenceRestoreNodes[root->firstNode+i].abi==XL6_IR_RESTORE_NODE_SET) {
                xl6OutputPuts("#include <__xl_runtime/xl6_set.h>\n"
                    "#include <__xl_runtime/xl6_set_import_chain.h>\n",out); break;
            }
        for (size_t i=0;i<root->nodeCount;++i)
            if (module->persistenceRestoreNodes[root->firstNode+i].abi==XL6_IR_RESTORE_NODE_DICT) {
                xl6OutputPuts("#include <__xl_runtime/xl6_dict.h>\n"
                    "#include <__xl_runtime/xl6_dict_import_chain.h>\n",out); break;
            }
        for (size_t i=0;i<root->nodeCount;++i)
            if (module->persistenceRestoreNodes[root->firstNode+i].openClassLayout!=SIZE_MAX) {
                xl6OutputPuts("#include <__xl_runtime/xl6_restore_open.h>\n",out); break;
            }
        unsigned isolationProviders=0;
        for (size_t i=0;i<root->nodeCount;++i) {
            const XL6_IrPersistenceRestoreNode* node=&module->persistenceRestoreNodes[root->firstNode+i];
            isolationProviders|=node->isolationGraphProviders|node->openIsolationGraphProviders;
        }
        if (!xl6IrCgenEmitValueGraphHeaders(out,isolationProviders,0) ||
            !xl6IrCgenEmitRestoreGraphCallbacks(out,module,r,diags)) return 0;
        xl6OutputPrintf(out,"static bool xlir_restore_r%lu_take(xvalue* source, xl6_error_result parsed, bool file, xvalue* target, xvalue** result)\n{\n",
            (unsigned long)r);
        if (!xl6IrCgenEmitRestoreGraphPlan(out,module,r,diags)) return 0;
        xl6OutputPuts("    xl6_error_result prepared = {true,NULL};\n",out);
        if (root->seedCount) {
            xl6OutputPrintf(out,"    xl6_error_scope preparation = {0};\n"
                "    xvalue* seeds[%lu] = {0};\n"
                "    if (xl6ErrorScopeBegin(&preparation)) {\n",(unsigned long)root->seedCount);
            for (size_t i=0;i<root->nodeCount;++i) {
                const XL6_IrPersistenceRestoreNode* node=&module->persistenceRestoreNodes[root->firstNode+i];
                if (node->abi!=XL6_IR_RESTORE_NODE_ARRAY && node->abi!=XL6_IR_RESTORE_NODE_LIST &&
                    node->abi!=XL6_IR_RESTORE_NODE_SET && node->abi!=XL6_IR_RESTORE_NODE_DICT) continue;
                char slot[80]; snprintf(slot,sizeof(slot),"seeds[%lu]",(unsigned long)node->seed);
                /* The IR recipe selects the genuine full projected native
                 * seed. A NULL allocation reaches the common consuming path. */
                if (node->abi==XL6_IR_RESTORE_NODE_LIST || node->abi==XL6_IR_RESTORE_NODE_SET ||
                    node->abi==XL6_IR_RESTORE_NODE_DICT) {
                    XL6_IrValueCollectionRecipe recipe={0};
                    if (!xl6IrValueCollectionConstructionRecipe(module,node->payloadType,&recipe) ||
                        xl6IrCgenEmitValueCollectionTargetCreateDepth(out,module,node->payloadType,
                            recipe.family,slot,recipe.admissionIdentitySet,0,NULL,0)<0) return 0;
                } else if (!xl6IrCgenEmitPersistenceSequenceSeed(out,module,node->payloadType,slot,diags)) return 0;
            }
            xl6OutputPuts("    prepared = xl6ErrorScopeEnd(&preparation);\n"
                "    } else { prepared = (xl6_error_result){false,xrtTakeError()}; }\n",out);
        }
        xl6OutputPrintf(out,"    return xl6PersistenceRestoreParsedTakeV1(%s,%lu,target,source,parsed.ok,parsed.error,prepared.ok,prepared.error,file,&plan,result);\n}\n",
            root->seedCount ? "seeds" : "NULL",(unsigned long)root->seedCount);
        for (int file=0;file<=(readUsed ? 1 : 0);++file) {
            xl6OutputPrintf(out,"static bool xlir_persist_%s_%s_t%u(xlir_string text, xvalue* target, xvalue** result)\n{\n"
                "    xl6_error_scope parser = {0};\n"
                "    xvalue* source; xl6_error_result parsed;\n"
                "    if (!xl6ErrorScopeBegin(&parser))\n"
                "        return xlir_restore_r%lu_take(NULL,(xl6_error_result){false,xrtTakeError()},%s,target,result);\n",
                name,file ? "read" : "parse",(unsigned)root->type,(unsigned long)r,file ? "true" : "false");
            if (file) xl6OutputPrintf(out,"    const char* path = xlir_string_cstr(text);\n"
                "    source = path ? %s(path) : NULL;\n",read);
            else xl6OutputPrintf(out,"    source = %s(text);\n",parse);
            xl6OutputPrintf(out,"    parsed = xl6ErrorScopeEnd(&parser);\n"
                "    return xlir_restore_r%lu_take(source,parsed,%s,target,result);\n}\n",
                (unsigned long)r,file ? "true" : "false");
        }
    }
    return !xl6OutputHasFailed(out);
}
