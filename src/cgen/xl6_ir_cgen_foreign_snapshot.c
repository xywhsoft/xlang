/* All language arguments have already been evaluated. Snapshots are scoped
 * owned locals, prepared once in parameter order before native entry. */
static int xl6IrCgenEmitValueGraphPlan(XL6_Output*,unsigned,int);
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
