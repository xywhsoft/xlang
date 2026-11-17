/* Only closed typed schemas produce stack records; no runtime name dispatch. */
static int xl6IrCgenPackPodProof(XL6_Output* out,const XL6_IrModule* module,XL6_IrTypeId id,size_t depth)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if(!type || depth>16)return 0;
    if(depth && type->kind==XL6_IR_TYPE_C_ARRAY && type->cAbi==XL6_IR_C_ABI_C_ARRAY &&
        type->nativeArrayExtent && type->paramCount==1 && type->params &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL)
        return xl6IrCgenPackPodProof(out,module,type->params[0],depth+1);
    if(type->kind!=XL6_IR_TYPE_C_RECORD)return type->kind==XL6_IR_TYPE_BUILTIN && type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    const XL6_IrAggregateLayout* layout=xl6IrUnionLayoutGet(module,id);
    if(!layout)return 0;
    xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s*)0,struct %s*), \"native packed POD tagged struct\");\n"
        "#ifdef __cplusplus\nXLIR_NATIVE_ASSERT(std::is_trivial<%s>::value && std::is_standard_layout<%s>::value, \"native packed POD trivial C layout\");\n#endif\n",
        type->cName,type->cName,type->cName,type->cName);
    for(size_t f=0;f<layout->fieldCount;++f)
        if(!xl6IrCgenPackPodProof(out,module,module->aggregateFields[layout->firstField+f].type,depth+1))return 0;
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenPackProof(XL6_Output* out,const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(!xl6IrForeignPacksMatch(module,target))return 0;
    for(size_t i=0;i<module->foreignPackCount;++i){const XL6_ForeignPackContract* pack=&module->foreignPacks[i];
        if(pack->declaration!=target->declaration)continue;
        xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s*)0,struct %s*), \"native descriptor tagged struct\");\n"
            "XLIR_NATIVE_ASSERT(sizeof(%s)>0, \"native descriptor completeness\");\n"
            "#ifdef __cplusplus\nXLIR_NATIVE_ASSERT(std::is_trivial<%s>::value && std::is_standard_layout<%s>::value, \"native descriptor trivial layout\");\n#endif\n",
            pack->recordCName,pack->recordCName,pack->recordCName,pack->recordCName,pack->recordCName);
        if(pack->callbackOwnerPhysicalIndexPlusOne)
            xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,void*), \"native callback group userdata field\");\n",
                pack->recordCName,pack->callbackUserField);
        for(size_t f=0;f<module->foreignPackFieldCount;++f){const XL6_ForeignPackFieldContract* field=&module->foreignPackFields[f];
            if(field->declaration!=pack->declaration || field->packPhysicalIndex!=pack->physicalIndex)continue;
            if(field->kind==XL6_FOREIGN_PACK_FIELD_RECORD){
                xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s*)0,struct %s*), \"native nested descriptor tagged struct\");\n"
                    "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&((%s*)0)->%s,%s*), \"native nested descriptor field\");\n"
                    "XLIR_NATIVE_ASSERT(sizeof(%s)>0, \"native nested descriptor completeness\");\n"
                    "#ifdef __cplusplus\nXLIR_NATIVE_ASSERT(std::is_trivial<%s>::value && std::is_standard_layout<%s>::value, \"native nested descriptor trivial layout\");\n#endif\n",
                    field->physicalCName,field->physicalCName,pack->recordCName,field->field,field->physicalCName,
                    field->physicalCName,field->physicalCName,field->physicalCName);continue;
            }
            xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,%s), \"native descriptor field type\");\n",
                pack->recordCName,field->field,field->physicalCName);
            const XL6_IrType* fieldType=xl6IrTypeGet(&module->types,field->logicalType);
            if(field->kind==XL6_FOREIGN_PACK_FIELD_VALUE &&
                (!fieldType || fieldType->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) &&
                !xl6IrCgenPackPodProof(out,module,field->logicalType,0))return 0;
            if(field->kind==XL6_FOREIGN_PACK_FIELD_SIZEOF){
                const char* record=field->recordParentIndexPlusOne ? module->foreignPackFields[field->recordParentIndexPlusOne-1].physicalCName:pack->recordCName;
                xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(sizeof(%s)<=((%s)~(%s)0), \"native descriptor size range\");\n",record,field->physicalCName,field->physicalCName);
            }
        }
    }
    return !xl6OutputHasFailed(out);
}
static int xl6IrCgenForeignPacksBegin(XL6_Output* out,const XL6_IrModule* module,const XL6_IrCallTarget* target,
    const XL6_IrValueId* operands,const char* scope)
{
    if(!xl6IrForeignPacksMatch(module,target))return -1;
    int count=0;
    for(size_t i=0;i<module->foreignPackCount;++i){const XL6_ForeignPackContract* pack=&module->foreignPacks[i];
        if(pack->declaration!=target->declaration)continue;
        if(!count++)xl6OutputPrintf(out,"    { bool native_packs_ok=!%s.failed;\n"
            "    if (xrtGetError()) { native_packs_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",scope,scope);
        xl6OutputPrintf(out,"    %s native_pack_%u={0};\n",pack->recordCName,pack->physicalIndex);
        for(size_t f=0;f<module->foreignPackFieldCount;++f){const XL6_ForeignPackFieldContract* field=&module->foreignPackFields[f];
            if(field->declaration!=pack->declaration || field->packPhysicalIndex!=pack->physicalIndex)continue;
            if(field->kind==XL6_FOREIGN_PACK_FIELD_RECORD)continue;
            size_t p=field->parameter;
            if(field->kind==XL6_FOREIGN_PACK_FIELD_TEXT_SIZE && field->countAbi==XL6_FOREIGN_SPAN_COUNT_INT){
                xl6OutputPuts("    if (native_packs_ok && ",out);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
                xl6OutputPrintf(out,".Size>INT_MAX) { native_packs_ok=false; xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n",scope);
            }
            xl6OutputPrintf(out,"    if (native_packs_ok) native_pack_%u.%s=",pack->physicalIndex,field->field);
            switch(field->kind){
            case XL6_FOREIGN_PACK_FIELD_SIZEOF:
                if(field->recordParentIndexPlusOne)xl6OutputPrintf(out,"sizeof(%s)",module->foreignPackFields[field->recordParentIndexPlusOne-1].physicalCName);
                else xl6OutputPrintf(out,"sizeof(native_pack_%u)",pack->physicalIndex);
                break;
            case XL6_FOREIGN_PACK_FIELD_VALUE:
                if(xl6IrForeignDataSnapshotGet(module,target,p))xl6OutputPrintf(out,"native_snapshot_%lu",(unsigned long)p);
                else xl6IrCgenForeignViewArgument(out,operands,p,NULL);
                break;
            case XL6_FOREIGN_PACK_FIELD_VIEW_RAW:xl6OutputPrintf(out,"(%s)native_view_%lu.raw",field->physicalCName,(unsigned long)p);break;
            case XL6_FOREIGN_PACK_FIELD_C_STRING:xl6OutputPrintf(out,"xlir_cstr_arg_%lu",(unsigned long)p);break;
            case XL6_FOREIGN_PACK_FIELD_TEXT_DATA:xl6IrCgenForeignViewArgument(out,operands,p,NULL);xl6OutputPuts(".Data",out);break;
            case XL6_FOREIGN_PACK_FIELD_TEXT_SIZE:xl6OutputPrintf(out,"(%s)",field->physicalCName);xl6IrCgenForeignViewArgument(out,operands,p,NULL);xl6OutputPuts(".Size",out);break;
            case XL6_FOREIGN_PACK_FIELD_SPAN_DATA:xl6OutputPrintf(out,"(%s)native_span_data_%lu",field->physicalCName,(unsigned long)p);break;
            case XL6_FOREIGN_PACK_FIELD_SPAN_COUNT:xl6OutputPrintf(out,"(%s)native_span_count_%lu",field->physicalCName,(unsigned long)p);break;
            case XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK:
                xl6OutputPrintf(out,"native_callback_%lu.invocation.self ? %s_thunk : NULL",(unsigned long)p,field->physicalCName);break;
            case XL6_FOREIGN_PACK_FIELD_OWNED_CALLBACK:{
                size_t member;const XL6_ForeignPackContract* group=xl6IrForeignPackCallbackMember(module,target,p,&member,NULL);
                if(group!=pack)return -1;
                xl6OutputPrintf(out,"native_callback_group_%u->Members[%lu] ? %s_thunk : NULL",pack->physicalIndex,
                    (unsigned long)member,field->physicalCName);break;
            }
            case XL6_FOREIGN_PACK_FIELD_CALLBACK_USER:
                xl6OutputPrintf(out,"native_callback_%lu.invocation.self ? &native_callback_%lu : NULL",(unsigned long)p,(unsigned long)p);break;
            default:return -1;
            }
            xl6OutputPuts(";\n",out);
        }
        for(size_t g=0;g<module->foreignArrayGroupCount;++g){const XL6_ForeignArrayGroupContract* group=&module->foreignArrayGroups[g];
            if(group->declaration!=pack->declaration || group->packPhysicalIndexPlusOne!=pack->physicalIndex+1)continue;
            xl6OutputPrintf(out,"    if (native_packs_ok) { native_pack_%u.%s=native_array_record_%u; native_pack_%u.%s=(%s)native_array_count_%u; }\n",
                pack->physicalIndex,group->packDataField,group->physicalIndex,pack->physicalIndex,group->packCountField,
                xl6ForeignSpanCountCName(group->countAbi),group->physicalIndex);
        }
        if(pack->callbackOwnerPhysicalIndexPlusOne)
            xl6OutputPrintf(out,"    if (native_packs_ok) native_pack_%u.%s=native_callback_group_%u;\n",
                pack->physicalIndex,pack->callbackUserField,pack->physicalIndex);
    }
    if(count)xl6OutputPuts("    if (native_packs_ok) {\n",out);
    return count;
}
static void xl6IrCgenForeignPacksEnd(XL6_Output* out)
{xl6OutputPuts("    } }\n",out);}
