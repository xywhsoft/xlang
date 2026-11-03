/* Exact resident access plans. Logical class owners remain values until the
 * foreign scope is active; raw payloads live only inside these stack scopes. */
static int xl6IrCgenNativeAccessBegin(XL6_Output* out,const XL6_IrModule* module,
    const XL6_IrCallTarget* target,const XL6_IrValueId* operands,const char* scope)
{
    int count=0;
    for(size_t p=0;p<target->parameterCount;++p) {
        if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_HANDLE_ACCESS_BORROW)continue;
        const XL6_IrClassField* field=xl6IrCallTargetNativeAccessField(module,target,p);
        if(!field || !field->nativeHandlePolicyCName)return -1;
        if(!count++)xl6OutputPuts("    { bool native_access_ok = true;\n",out);
        xl6OutputPrintf(out,"    XL6_NativeHandleAccessV1 native_access_%lu = {0};\n"
            "    if (native_access_ok) { xvalue* native_handle = xrtValueObjectGet(",(unsigned long)p);
        xl6IrCgenForeignViewArgument(out,operands,p,NULL);
        xl6OutputPuts(", (xstrview){",out);xl6IrCgenEmitCStringLiteral(out,field->name);
        xl6OutputPuts(",sizeof(",out);xl6IrCgenEmitCStringLiteral(out,field->name);
        xl6OutputPrintf(out,")-1}); if (!native_handle || !xl6NativeHandleAccessBeginV1(native_handle,%s(),%s,&native_access_%lu)) { native_access_ok=false; xl6ErrorScopeCaptureFailure(&%s); } }\n",
            field->nativeHandlePolicyCName,field->nativeDestroySymbol==target->symbol ?
                "XL6_NATIVE_HANDLE_ACCESS_TAKE_V1" : "XL6_NATIVE_HANDLE_ACCESS_CALL_V1",(unsigned long)p,scope);
    }
    if(count)xl6OutputPuts("    if (native_access_ok) {\n",out);
    return count;
}
static void xl6IrCgenNativeAccessEnd(XL6_Output* out,const XL6_IrCallTarget* target,const char* scope)
{
    xl6OutputPuts("    }\n",out);
    for(size_t p=target->parameterCount;p>0;--p)
        if(target->parameterPhysicalAbis[p-1]==XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_HANDLE_ACCESS_BORROW)
            xl6OutputPrintf(out,"    if (native_access_%lu.self && !xl6NativeHandleAccessEndV1(&native_access_%lu)) xl6ErrorScopeCaptureFailure(&%s);\n",
                (unsigned long)(p-1),(unsigned long)(p-1),scope);
    xl6OutputPuts("    }\n",out);
}
