/* Only closed typed schemas produce stack records; no runtime name dispatch. */
static int xl6IrCgenPackProof(XL6_Output* out,const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(!xl6IrForeignPacksMatch(module,target))return 0;
    for(size_t i=0;i<module->foreignPackCount;++i){const XL6_ForeignPackContract* pack=&module->foreignPacks[i];
        if(pack->declaration!=target->declaration)continue;
        xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE((%s*)0,struct %s*), \"native descriptor tagged struct\");\n"
            "XLIR_NATIVE_ASSERT(sizeof(%s)>0, \"native descriptor completeness\");\n",pack->recordCName,pack->recordCName,pack->recordCName);
        for(size_t f=0;f<module->foreignPackFieldCount;++f){const XL6_ForeignPackFieldContract* field=&module->foreignPackFields[f];
            if(field->declaration!=pack->declaration || field->packPhysicalIndex!=pack->physicalIndex)continue;
            xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s,%s), \"native descriptor field type\");\n",
                pack->recordCName,field->field,field->physicalCName);
            if(field->kind==XL6_FOREIGN_PACK_FIELD_SIZEOF)xl6OutputPrintf(out,
                "XLIR_NATIVE_ASSERT(sizeof(%s)<=((%s)~(%s)0), \"native descriptor size range\");\n",pack->recordCName,field->physicalCName,field->physicalCName);
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
            size_t p=field->parameter;
            if(field->kind==XL6_FOREIGN_PACK_FIELD_TEXT_SIZE && field->countAbi==XL6_FOREIGN_SPAN_COUNT_INT){
                xl6OutputPuts("    if (native_packs_ok && ",out);xl6IrCgenForeignViewArgument(out,operands,p,NULL);
                xl6OutputPrintf(out,".Size>INT_MAX) { native_packs_ok=false; xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n",scope);
            }
            xl6OutputPrintf(out,"    if (native_packs_ok) native_pack_%u.%s=",pack->physicalIndex,field->field);
            switch(field->kind){
            case XL6_FOREIGN_PACK_FIELD_SIZEOF:xl6OutputPrintf(out,"sizeof(native_pack_%u)",pack->physicalIndex);break;
            case XL6_FOREIGN_PACK_FIELD_VALUE:xl6IrCgenForeignViewArgument(out,operands,p,NULL);break;
            case XL6_FOREIGN_PACK_FIELD_VIEW_RAW:xl6OutputPrintf(out,"(%s)native_view_%lu.raw",field->physicalCName,(unsigned long)p);break;
            case XL6_FOREIGN_PACK_FIELD_C_STRING:xl6OutputPrintf(out,"xlir_cstr_arg_%lu",(unsigned long)p);break;
            case XL6_FOREIGN_PACK_FIELD_TEXT_DATA:xl6IrCgenForeignViewArgument(out,operands,p,NULL);xl6OutputPuts(".Data",out);break;
            case XL6_FOREIGN_PACK_FIELD_TEXT_SIZE:xl6OutputPrintf(out,"(%s)",field->physicalCName);xl6IrCgenForeignViewArgument(out,operands,p,NULL);xl6OutputPuts(".Size",out);break;
            case XL6_FOREIGN_PACK_FIELD_SPAN_DATA:xl6OutputPrintf(out,"(%s)native_span_data_%lu",field->physicalCName,(unsigned long)p);break;
            case XL6_FOREIGN_PACK_FIELD_SPAN_COUNT:xl6OutputPrintf(out,"(%s)native_span_count_%lu",field->physicalCName,(unsigned long)p);break;
            default:return -1;
            }
            xl6OutputPuts(";\n",out);
        }
    }
    if(count)xl6OutputPuts("    if (native_packs_ok) {\n",out);
    return count;
}
static void xl6IrCgenForeignPacksEnd(XL6_Output* out)
{xl6OutputPuts("    } }\n",out);}
