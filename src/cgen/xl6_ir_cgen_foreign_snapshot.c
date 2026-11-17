/* All language arguments have already been evaluated. Snapshots are scoped
 * owned locals, prepared once in parameter order before native entry. */
static int xl6IrCgenEmitValueGraphPlan(XL6_Output*,unsigned,int);
static int xl6IrCgenEmitOwnedPlaceRelease(XL6_Output*,const XL6_IrModule*,XL6_IrTypeId,const char*,XL6_DiagList*);
static int xl6IrCgenDataSnapshotsBegin(XL6_Output* out,const XL6_IrModule* m,
    const XL6_IrCallTarget* target,const XL6_IrValueId* operands,const char* scope)
{
    int count=0;
    for(size_t p=0;p<target->parameterCount;++p){
        const XL6_ForeignDataSnapshotContract* row=xl6IrForeignDataSnapshotGet(m,target,p);
        if(!row)continue;
        if(!count++)xl6OutputPrintf(out,"    { bool native_snapshots_ok=!%s.failed;\n"
            "    if(xrtGetError()) { native_snapshots_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",scope,scope);
        xl6OutputPrintf(out,"    xvalue* native_snapshot_%lu=NULL;\n"
            "    if(native_snapshots_ok) { native_snapshot_%lu=xl6PersistenceSnapshotNativeV1(",
            (unsigned long)p,(unsigned long)p);
        xl6IrCgenForeignViewArgument(out,operands,p,NULL);
        xl6OutputPrintf(out,", &xlir_persist_plan_%u, ",XL6_IR_PERSIST_FORMAT_PUBLIC);
        unsigned providers=xl6IrValueGraphProvidersForType(m,row->logicalType);
        if(providers){if(!xl6IrCgenEmitValueGraphPlan(out,providers,1))return -1;}
        else xl6OutputPuts("NULL",out);
        xl6OutputPrintf(out,"); if(!native_snapshot_%lu || xrtGetError()) { native_snapshots_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",
            (unsigned long)p,scope);
    }
    if(count)xl6OutputPuts("    if(native_snapshots_ok) {\n",out);
    return count;
}
static void xl6IrCgenDataSnapshotsEnd(XL6_Output* out,const XL6_IrModule* m,
    const XL6_IrCallTarget* target,const char* scope)
{
    xl6OutputPrintf(out,"    }\n    if(xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n",scope);
    for(size_t p=target->parameterCount;p;--p)
        if(xl6IrForeignDataSnapshotGet(m,target,p-1))
            xl6OutputPrintf(out,"    xrtValueRelease(native_snapshot_%lu); if(xrtGetError()) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)p-1,scope);
    xl6OutputPuts("    }\n",out);
}

/* An owned logical carrier is not published to the language caller until the
 * foreign boundary succeeds. Keep every original while staging all copies,
 * then retire originals in reverse order. A failed conversion/retirement
 * disposes the complete carrier, including unmarked owned tuple fields. */
static int xl6IrCgenResultDataSnapshots(XL6_Output* out,const XL6_IrModule* m,
    const XL6_IrCallTarget* target,const char* place,const char* scope)
{
    const XL6_ForeignDataSnapshotContract* rows[64]={0};size_t count=0;
    for(size_t ordinal=0;ordinal<64;++ordinal){
        rows[ordinal]=xl6IrForeignResultDataSnapshotGet(m,target,ordinal);
        if(rows[ordinal])++count;
    }
    if(!count)return 1;
    const XL6_IrType* result=xl6IrTypeGet(&m->types,target->returnType);
    if(!result || !place || !scope)return 0;
    xl6OutputPrintf(out,"    { bool native_result_snapshots_ok=!%s.failed;\n"
        "    if(xrtGetError()) { native_result_snapshots_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",scope,scope);
    for(size_t ordinal=0;ordinal<64;++ordinal)if(rows[ordinal]){
        xl6OutputPrintf(out,"    xvalue* native_result_snapshot_%lu=NULL;\n"
            "    if(native_result_snapshots_ok) { native_result_snapshot_%lu=xl6PersistenceSnapshotNativeV1(%s",
            (unsigned long)ordinal,(unsigned long)ordinal,place);
        if(result->kind==XL6_IR_TYPE_TUPLE)xl6OutputPrintf(out,".e%lu",(unsigned long)ordinal);
        xl6OutputPrintf(out,", &xlir_persist_plan_%u, ",XL6_IR_PERSIST_FORMAT_PUBLIC);
        unsigned providers=xl6IrValueGraphProvidersForType(m,rows[ordinal]->logicalType);
        if(providers){if(!xl6IrCgenEmitValueGraphPlan(out,providers,1))return 0;}
        else xl6OutputPuts("NULL",out);
        xl6OutputPrintf(out,"); if(!native_result_snapshot_%lu || xrtGetError()) { native_result_snapshots_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",
            (unsigned long)ordinal,scope);
    }
    xl6OutputPuts("    if(native_result_snapshots_ok) {\n",out);
    for(size_t reverse=64;reverse;--reverse)if(rows[reverse-1]){
        size_t ordinal=reverse-1;char item[128];
        if(result->kind==XL6_IR_TYPE_TUPLE)snprintf(item,sizeof(item),"%s.e%lu",place,(unsigned long)ordinal);
        else snprintf(item,sizeof(item),"%s",place);
        xl6OutputPrintf(out,"    { xvalue* native_result_original=%s; %s=native_result_snapshot_%lu;\n"
            "      native_result_snapshot_%lu=NULL; xrtValueRelease(native_result_original);\n"
            "      if(xrtGetError()) { native_result_snapshots_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",
            item,item,(unsigned long)ordinal,(unsigned long)ordinal,scope);
    }
    xl6OutputPuts("    }\n",out);
    for(size_t reverse=64;reverse;--reverse)if(rows[reverse-1])
        xl6OutputPrintf(out,"    xrtValueRelease(native_result_snapshot_%lu);\n"
            "    if(xrtGetError()) { native_result_snapshots_ok=false; xl6ErrorScopeCaptureFailure(&%s); }\n",
            (unsigned long)(reverse-1),scope);
    xl6OutputPrintf(out,"    if(!native_result_snapshots_ok || %s.failed) {\n",scope);
    if(!xl6IrCgenEmitOwnedPlaceRelease(out,m,target->returnType,place,NULL))return 0;
    xl6OutputPrintf(out,"    if(xrtGetError())xl6ErrorScopeCaptureFailure(&%s);\n    } }\n",scope);
    return !xl6OutputHasFailed(out);
}
