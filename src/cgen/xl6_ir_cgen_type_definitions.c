/* Emit admission at the call site: no runtime thunk, owner allocation or
 * allocator tag. A non-NULL physical return is owned even on a failed call.
 * Observe the error BEFORE strlen; failed producers need not terminate data. */
static void xl6IrCgenEmitOwnedStringAdmission(XL6_Output* out,
    const XL6_IrModule* module, const XL6_IrCallTarget* target,
    const char* place, const char* failed)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->returnType);
    if (failed) {
        xl6OutputPrintf(out,"    if (%s || xrtGetError() != NULL) {\n"
            "        xerror* saved_error = xrtTakeError(); xrtFree((void*)%s.Data);\n"
            "        %s = (xlir_string){0}; xrtSetErrorTake(saved_error);\n"
            "    } else {\n",failed,place,place);
    }
    xl6OutputPrintf(out,"    %s.Size = %s.Data != NULL ? strlen(%s.Data) : 0;\n",place,place,place);
    if (target->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE) {
        xl6OutputPrintf(out,"    if (%s.Size > INT64_MAX) { xrtFree((void*)%s.Data); %s = (xlir_string){0}; xrtSetErrorKind(XERR_RANGE); }\n",place,place,place);
        if (type->kind!=XL6_IR_TYPE_NULLABLE)
            xl6OutputPrintf(out,"    else if (%s.Data == NULL) xrtSetErrorKind(XERR_VALUE);\n",place);
    }
    if(failed)xl6OutputPuts("    }\n",out);
}

/* A no-raise C producer can still require a fallible language copy. Keep
 * the raw pointer until its outcome is known; an absent nullable result and
 * a failed nonempty copy have different outcomes, even without a diagnostic. */
static void xl6IrCgenEmitBorrowedOwnedAdmission(XL6_Output* out,
    const XL6_IrModule* module, XL6_IrTypeId logicalType,XL6_IrCallResultCAbi resultAbi,
    const char* place, const char* raw, const char* scope)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,logicalType);
    if(resultAbi==XL6_IR_CALL_RESULT_C_ABI_CONST_XVALUE_POINTER){
        xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError() == NULL) {\n"
            "        %s = xrtValueRetain(%s != NULL ? %s : xrtValueNull());\n"
            "        if (%s == NULL) xl6ErrorScopeCaptureFailure(&%s);\n    }\n",scope,place,raw,raw,place,scope);
        return;
    }
    if(resultAbi==XL6_IR_CALL_RESULT_C_ABI_BYTES_VIEW){
        xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError() == NULL) {\n"
            "        if ((%s.Data == NULL && %s.Size != 0) || %s.Size > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n"
            "        else if (%s.Data != NULL) { %s = xrtValueBytes(%s); if (%s == NULL) xl6ErrorScopeCaptureFailure(&%s); }\n",
            scope,raw,raw,raw,scope,raw,place,raw,place,scope);
        if(type->kind==XL6_IR_TYPE_NULLABLE)
            xl6OutputPrintf(out,"        else %s = xrtValueRetain(xrtValueNull());\n",place);
        else xl6OutputPrintf(out,"        else { xrtSetErrorKind(XERR_VALUE); xl6ErrorScopeCaptureFailure(&%s); }\n",scope);
        xl6OutputPuts("    }\n",out);return;
    }
    if(resultAbi==XL6_IR_CALL_RESULT_C_ABI_TEXT_VIEW){
        xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError() == NULL) {\n"
            "        if ((%s.Data == NULL && %s.Size != 0) || %s.Size > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); xl6ErrorScopeCaptureFailure(&%s); }\n"
            "        else { %s = xlir_string_copy(%s); if (%s.Data != NULL && %s.Data == NULL) xl6ErrorScopeCaptureFailure(&%s);\n",
            scope,raw,raw,raw,scope,place,raw,raw,place,scope);
        if(type->kind!=XL6_IR_TYPE_NULLABLE)
            xl6OutputPrintf(out,"        else if (%s.Data == NULL) { xrtSetErrorKind(XERR_VALUE); xl6ErrorScopeCaptureFailure(&%s); }\n",raw,scope);
        xl6OutputPuts("        }\n    }\n",out);return;
    }
    xl6OutputPrintf(out,"    if (!%s.failed && xrtGetError() == NULL) {\n"
        "        %s = xlir_string_from_cstr(%s);\n"
        "        if (%s != NULL && %s.Data == NULL) xl6ErrorScopeCaptureFailure(&%s);\n",
        scope,place,raw,raw,place,scope);
    if(type->kind!=XL6_IR_TYPE_NULLABLE)
        xl6OutputPrintf(out,"        else if (%s == NULL) { xrtSetErrorKind(XERR_VALUE); xl6ErrorScopeCaptureFailure(&%s); }\n",raw,scope);
    xl6OutputPuts("    }\n",out);
}
static void xl6IrCgenEmitBorrowedValueAdmission(XL6_Output* out,
    const XL6_IrModule* module,const XL6_IrCallTarget* target,
    const char* place,const char* raw,const char* scope)
{
    xl6IrCgenEmitBorrowedOwnedAdmission(out,module,target->returnType,target->resultCAbi,place,raw,scope);
}

static const char* xl6IrCgenBorrowedResultCType(const XL6_IrCallTarget* target)
{
    return target->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_TEXT_VIEW ? "xstrview" :
        target->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_BYTES_VIEW ? "xbytesview" :
        target->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_CONST_XVALUE_POINTER ? "const xvalue*" : "const char*";
}

/* Read the opaque byte value once into a stack view. There is no forwarding
 * thunk or temporary Buffer. A NULL operands slice denotes typed adapter
 * arguments, whose implicit receiver is not a byte value. */
static int xl6IrCgenPrepareBytesViews(XL6_Output* out,const XL6_IrCallTarget* target,
    const XL6_IrValueId* operands)
{
    int present=0;
    for(size_t p=0;p<target->parameterCount;++p)
        if(target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_BYTES_VIEW_BORROW){
            if(!present++)xl6OutputPuts("    {\n",out);
            xl6OutputPrintf(out,"    xbytesview xlir_bytes_arg_%lu = {0};\n",(unsigned long)p);
            if(operands)xl6OutputPrintf(out,"    if (!xrtValueGetBytes(v%u, &xlir_bytes_arg_%lu)) abort();\n",
                (unsigned)operands[p],(unsigned long)p);
            else xl6OutputPrintf(out,"    if (!xrtValueGetBytes(a%lu, &xlir_bytes_arg_%lu)) abort();\n",
                (unsigned long)(p-1),(unsigned long)p);
        }
    return present;
}

/* C strings are explicit terminating-text contracts. Interior NUL is rejected
 * before C entry; length-aware xstrview declarations never perform this scan. */
static int xl6IrCgenPrepareTerminatedStrings(XL6_Output* out,const XL6_IrCallTarget* target,
    const XL6_IrValueId* operands,const char* scope)
{
    int count=0;
    for(size_t p=0;p<target->parameterCount;++p) {
        if(target->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_TERMINATED_STRING_BORROW)continue;
        if(!count++)xl6OutputPuts("    { bool c_strings_ok = true;\n",out);
        xl6OutputPrintf(out,"    const char* xlir_cstr_arg_%lu = xlir_string_cstr(",(unsigned long)p);
        if(operands)xl6OutputPrintf(out,"v%u",(unsigned)operands[p]);
        else xl6OutputPrintf(out,"a%lu",(unsigned long)(p-1));
        xl6OutputPrintf(out,"); if (!xlir_cstr_arg_%lu) { c_strings_ok = false; xl6ErrorScopeCaptureFailure(&%s); }\n",(unsigned long)p,scope);
    }
    if(count)xl6OutputPuts("    if (c_strings_ok) {\n",out);
    return count;
}

static int xl6IrCgenTypeSelected(
    const XL6_IrModule* module, XL6_IrTypeId type,
    const unsigned char* selectedTypes)
{
    if ( selectedTypes == NULL ) return 1;
    return module != NULL && type != XL6_IR_TYPE_INVALID &&
           (size_t)type <= module->types.count && selectedTypes[type - 1] != 0;
}

static int xl6IrCgenEmitForeignArrayType(XL6_Output* out, const XL6_IrModule* module,
    XL6_IrTypeId id, unsigned char* states, XL6_DiagList* diags)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, id);
    if (!type || type->cAbi != XL6_IR_C_ABI_C_ARRAY) return 1;
    if (states[id - 1] == 2) return 1;
    if (states[id - 1] == 1) return xl6IrCgenDiag(diags, module, 0, "recursive fixed C array value");
    states[id - 1] = 1;
    if (!xl6IrCgenEmitForeignArrayType(out, module, type->params[0], states, diags)) return 0;
    const char* element = xl6IrCgenType(module, type->params[0]);
    if (!element) return xl6IrCgenDiag(diags, module, 0, "fixed C array has no exact element type");
    xl6OutputPrintf(out, "#ifndef XL6_C_ARRAY_DEFINED_%s\n#define XL6_C_ARRAY_DEFINED_%s 1\n"
        "typedef struct { %s Elements[%llu]; } %s;\n#endif\n",
        type->cName, type->cName, element, (unsigned long long)type->nativeArrayExtent, type->cName);
    states[id - 1] = 2;
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitForeignArrayTypes(XL6_Output* out, const XL6_IrModule* module,
    const unsigned char* selectedTypes, XL6_DiagList* diags)
{
    int needed = 0;
    for (size_t i = 0; i < module->types.count; ++i)
        if (module->types.items[i].cAbi == XL6_IR_C_ABI_C_ARRAY &&
            xl6IrCgenTypeSelected(module, (XL6_IrTypeId)(i + 1), selectedTypes)) needed = 1;
    if (!needed) return 1;
    /* Authority headers define the C record elements before any inline-array
     * value carrier is instantiated. No second definition of those records. */
    for (size_t i = 0; i < module->types.count; ++i) {
        const XL6_IrType* type = &module->types.items[i];
        if (type->foreignHeader && xl6IrCgenTypeSelected(module, (XL6_IrTypeId)(i + 1), selectedTypes))
            xl6OutputPrintf(out, "#include <%s>\n", type->foreignHeader);
    }
    unsigned char* states = xrtCalloc(module->types.count, 1);
    if (!states) return xl6IrCgenDiag(diags, module, 0, "cannot order fixed C array declarations");
    int okay = 1;
    for (size_t i = 0; okay && i < module->types.count; ++i)
        if (xl6IrCgenTypeSelected(module, (XL6_IrTypeId)(i + 1), selectedTypes))
            okay = xl6IrCgenEmitForeignArrayType(out, module, (XL6_IrTypeId)(i + 1), states, diags);
    xrtFree(states);
    return okay;
}

static int xl6IrCgenEmitForeignFieldSchema(XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrAggregateField* field, XL6_DiagList* diags)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, field->type);
    uint64 extents[64]; size_t count = 0;
    while (type && type->cAbi == XL6_IR_C_ABI_C_ARRAY) {
        if (count == 64) return xl6IrCgenDiag(diags, module, field->declaration, "C array rank exceeds the verified type depth");
        extents[count++] = type->nativeArrayExtent;
        type = xl6IrTypeGet(&module->types, type->params[0]);
    }
    const char* name = type ? xl6IrCgenType(module, (XL6_IrTypeId)(type - module->types.items + 1)) : NULL;
    if (!name) return xl6IrCgenDiag(diags, module, field->declaration, "C field has no exact native element");
    xl6OutputPrintf(out, "    %s %s", name, field->cName);
    for (size_t i = 0; i < count; ++i) xl6OutputPrintf(out, "[%llu]", (unsigned long long)extents[i]);
    xl6OutputPuts(";\n", out);
    return !xl6OutputHasFailed(out);
}

/* The public header keeps the same ABI proofs in C11 and C++11. C enum
 * compatibility is normalized for scalar values and callback signatures;
 * qualifiers, array extents and nominal record identity remain exact. */
static void xl6IrCgenEmitNativeAbiProofs(XL6_Output* out,const XL6_IrModule* module)
{
    bool needed=module->foreignFunctionHeaderCount!=0;
    for(size_t i=0;i<module->types.count;++i)if(module->types.items[i].foreignHeader){needed=true;break;}
    if(!needed)return;
    xl6OutputPuts(
        "#ifndef XLIR_NATIVE_ABI_PROOFS_V1\n"
        "#define XLIR_NATIVE_ABI_PROOFS_V1 1\n"
        "#ifdef __cplusplus\n"
        "extern \"C++\" {\n"
        "#include <type_traits>\n"
        "namespace xlir_native_abi_v1 {\n"
        "template<class T,bool E=std::is_enum<T>::value> struct scalar { typedef T type; };\n"
        "template<class T> struct scalar<T,true> { typedef typename std::underlying_type<T>::type type; };\n"
        "template<class T> struct canonical : scalar<T> {};\n"
        "template<class T> struct canonical<const T> { typedef const typename canonical<T>::type type; };\n"
        "template<class T> struct canonical<volatile T> { typedef volatile typename canonical<T>::type type; };\n"
        "template<class T> struct canonical<const volatile T> { typedef const volatile typename canonical<T>::type type; };\n"
        "template<class R,class... A> struct canonical<R(A...)> { typedef typename canonical<R>::type type(typename canonical<A>::type...); };\n"
        "template<class R,class... A> struct canonical<R(A...,...)> { typedef typename canonical<R>::type type(typename canonical<A>::type...,...); };\n"
        "template<class R,class... A> struct canonical<R(*)(A...)> { typedef typename canonical<R(A...)>::type* type; };\n"
        "template<class R,class... A> struct canonical<R(*)(A...,...)> { typedef typename canonical<R(A...,...)>::type* type; };\n"
        "#if defined(__cpp_noexcept_function_type)\n"
        "template<class R,class... A> struct canonical<R(A...) noexcept> : canonical<R(A...)> {};\n"
        "template<class R,class... A> struct canonical<R(A...,...) noexcept> : canonical<R(A...,...)> {};\n"
        "template<class R,class... A> struct canonical<R(*)(A...) noexcept> : canonical<R(*)(A...)> {};\n"
        "template<class R,class... A> struct canonical<R(*)(A...,...) noexcept> : canonical<R(*)(A...,...)> {};\n"
        "#endif\n"
        "template<class A,class B> struct same : std::is_same<typename canonical<typename std::decay<A>::type>::type,typename canonical<B>::type> {};\n"
        "}\n"
        "}\n"
        "#define XLIR_NATIVE_ASSERT(Expr,Message) static_assert(Expr,Message)\n"
        "#define XLIR_NATIVE_ALIGNOF(Type) alignof(Type)\n"
        "#define XLIR_NATIVE_IS_TYPE(Expr,Type) (xlir_native_abi_v1::same<decltype(Expr),Type>::value)\n"
        "#define XLIR_NATIVE_IS_POINTER(Type) (std::is_pointer<Type>::value)\n"
        "#else\n"
        "#define XLIR_NATIVE_ASSERT(Expr,Message) _Static_assert(Expr,Message)\n"
        "#define XLIR_NATIVE_ALIGNOF(Type) _Alignof(Type)\n"
        "#define XLIR_NATIVE_IS_TYPE(Expr,Type) _Generic((Expr),Type:1,default:0)\n"
        "#define XLIR_NATIVE_IS_POINTER(Type) _Generic((Type)0,bool:0,char:0,signed char:0,unsigned char:0,short:0,unsigned short:0,int:0,unsigned int:0,long:0,unsigned long:0,long long:0,unsigned long long:0,float:0,double:0,long double:0,default:1)\n"
        "#endif\n"
        "#endif\n"
        ,out);
}

static int xl6IrCgenEmitUnionTypes(
    XL6_Output* out, const XL6_IrModule* module,
    const unsigned char* selectedTypes, XL6_DiagList* diags,
    XL6_IrCgenHeaderBindingVisitor visitor, void* context)
{
    unsigned char* emitted;
    size_t emittedCount = 0;
    size_t requiredCount = 0;
    size_t pass;
    size_t layoutIndex;
    xl6IrCgenEmitNativeAbiProofs(out,module);
    if (!xl6IrCgenEmitForeignArrayTypes(out, module, selectedTypes, diags)) return 0;
    for ( layoutIndex = 0; layoutIndex < module->aggregateLayoutCount; ++layoutIndex )
        if ( xl6IrCgenTypeSelected(
                 module, module->aggregateLayouts[layoutIndex].type, selectedTypes) )
            requiredCount += 1;
    if ( requiredCount == 0 ) return 1;
    emitted = (unsigned char*)xrtCalloc(
        module->aggregateLayoutCount, sizeof(*emitted));
    if ( emitted == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                             "XLIR union declaration ordering allocation failed");
    for ( pass = 0; pass < module->aggregateLayoutCount &&
                    emittedCount < requiredCount; ++pass ) {
        size_t before = emittedCount;
        for ( layoutIndex = 0; layoutIndex < module->aggregateLayoutCount; ++layoutIndex ) {
            const XL6_IrAggregateLayout* layout = &module->aggregateLayouts[layoutIndex];
            const XL6_IrType* unionType = xl6IrTypeGet(&module->types, layout->type);
            size_t fieldOffset;
            int ready = 1;
            if ( emitted[layoutIndex] ||
                 !xl6IrCgenTypeSelected(module, layout->type, selectedTypes) )
                continue;
            for ( fieldOffset = 0; fieldOffset < layout->fieldCount; ++fieldOffset ) {
                const XL6_IrAggregateField* field =
                    &module->aggregateFields[layout->firstField + fieldOffset];
                const XL6_IrType* fieldType = xl6IrTypeGet(&module->types, field->type);
                size_t dependency;
                if ( fieldType == NULL ) { ready = 0; break; }
                if ( !xl6IrCAbiIsAggregate(fieldType->cAbi) ) continue;
                for ( dependency = 0; dependency < module->aggregateLayoutCount; ++dependency )
                    if ( module->aggregateLayouts[dependency].type == field->type ) break;
                if ( dependency == module->aggregateLayoutCount ||
                     !xl6IrCgenTypeSelected(
                         module, module->aggregateLayouts[dependency].type,
                         selectedTypes) ||
                     !emitted[dependency] ) {
                    ready = 0; break;
                }
            }
            if ( !ready ) continue;
            if ( unionType == NULL || !xl6IrTypeKindIsAggregate(unionType->kind) ||
                 !xl6IrCAbiIsAggregate(unionType->cAbi) ||
                 unionType->cName[0] == '\0' ) {
                xrtFree(emitted);
                return xl6IrCgenDiag(diags, module, layout->declaration,
                    "XLIR union type has no deterministic nominal C ABI");
            }
            if (unionType->kind == XL6_IR_TYPE_C_RECORD) {
                if (!unionType->foreignHeader) {
                    xrtFree(emitted);
                    return xl6IrCgenDiag(diags, module, layout->declaration,
                        "C record has no authoritative declaration header");
                }
                xl6OutputPrintf(out, "#include <%s>\n#ifndef XL6_RECORD_CHECK_%s\n"
                    "#define XL6_RECORD_CHECK_%s 1\ntypedef struct {\n",
                    unionType->foreignHeader, unionType->cName, unionType->cName);
                for (fieldOffset = 0; fieldOffset < layout->fieldCount; ++fieldOffset) {
                    const XL6_IrAggregateField* field =
                        &module->aggregateFields[layout->firstField + fieldOffset];
                    const char* cType = xl6IrCgenType(module, field->type);
                    if (!cType) {
                        xrtFree(emitted);
                        return xl6IrCgenDiag(diags, module, field->declaration,
                            "C record field has no exact physical type");
                    }
                    if (!xl6IrCgenEmitForeignFieldSchema(out, module, field, diags)) { xrtFree(emitted); return 0; }
                }
                xl6OutputPrintf(out, "} xlir_record_schema_%s;\n"
                    "XLIR_NATIVE_ASSERT(sizeof(%s) == sizeof(xlir_record_schema_%s), \"C record size\");\n"
                    "XLIR_NATIVE_ASSERT(XLIR_NATIVE_ALIGNOF(%s) == XLIR_NATIVE_ALIGNOF(xlir_record_schema_%s), \"C record alignment\");\n",
                    unionType->cName, unionType->cName, unionType->cName,
                    unionType->cName, unionType->cName);
                for (fieldOffset = 0; fieldOffset < layout->fieldCount; ++fieldOffset) {
                    const XL6_IrAggregateField* field =
                        &module->aggregateFields[layout->firstField + fieldOffset];
                    const char* cType = xl6IrCgenType(module, field->type);
                    xl6OutputPrintf(out,
                        "XLIR_NATIVE_ASSERT(offsetof(%s, %s) == offsetof(xlir_record_schema_%s, %s), \"C record field offset\");\n",
                        unionType->cName, field->cName, unionType->cName, field->cName);
                    const XL6_IrType* fieldType = xl6IrTypeGet(&module->types, field->type);
                    if (fieldType->cAbi == XL6_IR_C_ABI_C_ARRAY) {
                        const char* element = xl6IrCgenType(module, fieldType->params[0]);
                        xl6OutputPrintf(out,
                            "XLIR_NATIVE_ASSERT(sizeof(((%s*)0)->%s) / sizeof(((%s*)0)->%s[0]) == %llu, \"C array extent\");\n"
                            "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&((%s*)0)->%s[0], %s*), \"C array element type\");\n",
                            unionType->cName, field->cName, unionType->cName, field->cName,
                            (unsigned long long)fieldType->nativeArrayExtent,
                            unionType->cName, field->cName, element);
                    } else xl6OutputPrintf(out,
                        "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(((%s*)0)->%s, %s), \"C record field type\");\n",
                        unionType->cName, field->cName, cType);
                }
                xl6OutputPuts("#endif\n", out);
                if(!xl6IrCgenOutputSizeProof(out,module,layout)){xrtFree(emitted);return 0;}
                emitted[layoutIndex] = 1;
                emittedCount += 1;
                continue;
            }
            xl6OutputPrintf(out,
                "#ifndef XL6_ABI_DEFINED_%s\n"
                "#define XL6_ABI_DEFINED_%s 1\n"
                "typedef union ",
                unionType->cName, unionType->cName);
            {
                size_t startByte = out->buffer.Size;
                xl6OutputPuts(unionType->cName, out);
                if (visitor != NULL && !visitor(context, unionType->nominalSymbol,
                        unionType->cName, "type", startByte, out->buffer.Size)) {
                    xrtFree(emitted);
                    return xl6IrCgenDiag(diags, module, layout->declaration,
                                         "cannot collect bounded C union tag binding");
                }
            }
            xl6OutputPuts(" {\n", out);
            for ( fieldOffset = 0; fieldOffset < layout->fieldCount; ++fieldOffset ) {
                size_t fieldIndex = layout->firstField + fieldOffset;
                const XL6_IrAggregateField* field = &module->aggregateFields[fieldIndex];
                const char* cType = xl6IrCgenType(module, field->type);
                if ( cType == NULL || field->cName[0] == '\0' ) {
                    xrtFree(emitted);
                    return xl6IrCgenDiag(diags, module, field->declaration,
                        "XLIR union field has no exact C value ABI");
                }
                xl6OutputPrintf(out, "    %s %s;\n", cType, field->cName);
            }
            xl6OutputPuts("} ", out);
            {
                size_t startByte = out->buffer.Size;
                xl6OutputPuts(unionType->cName, out);
                if (visitor != NULL && !visitor(context, unionType->nominalSymbol,
                        unionType->cName, "type", startByte, out->buffer.Size)) {
                    xrtFree(emitted);
                    return xl6IrCgenDiag(diags, module, layout->declaration,
                                         "cannot collect bounded C union typedef binding");
                }
            }
            xl6OutputPuts(";\n#endif\n", out);
            emitted[layoutIndex] = 1;
            emittedCount += 1;
        }
        if ( emittedCount == before ) break;
    }
    xrtFree(emitted);
    if ( emittedCount != requiredCount )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR union layouts contain an incomplete or recursive by-value dependency");
    xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}

static const char* xl6IrCgenCellKind(const XL6_IrType* element)
{
    if (element == NULL) return NULL;
    const XL6_IrNativeValuePlan* native=xl6IrOwnedTransferPlanForType(element);
    if(native)return native->graphCapture ? "XL6_CELL_NATIVE_GRAPH_V3" : "XL6_CELL_NATIVE_LEAF_V2";
    switch (element->cAbi) {
    case XL6_IR_C_ABI_BOOL: return "XL6_CELL_BOOL";
    case XL6_IR_C_ABI_CHAR_U32: return "XL6_CELL_CHAR";
    case XL6_IR_C_ABI_I64: return "XL6_CELL_I64";
    case XL6_IR_C_ABI_U64: return "XL6_CELL_U64";
    case XL6_IR_C_ABI_TYPE_ID: return "XL6_CELL_TYPE_ID";
    case XL6_IR_C_ABI_F64: return "XL6_CELL_F64";
    case XL6_IR_C_ABI_MANAGED_STRING: return "XL6_CELL_STRING";
    case XL6_IR_C_ABI_MANAGED_BYTES:
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR: return "XL6_CELL_XVALUE";
    case XL6_IR_C_ABI_TAGGED_NULLABLE:
        return element->cName[0] != '\0' ? "XL6_CELL_TRIVIAL_BYTES" : NULL;
    default: return NULL;
    }
}

static int xl6IrCgenEmitProductTypes(
    XL6_Output* out, const XL6_IrModule* module,
    const unsigned char* selectedTypes, XL6_DiagList* diags)
{
    size_t typeIndex;
    int emitted = 0;
    int scalarCarriersIncluded = 0;
    for(size_t t=0;t<module->callTargetCount;++t){
        const XL6_IrCallTarget* target=&module->callTargets[t];
        const XL6_ForeignFunctionHeaderContract* row=xl6IrForeignFunctionHeaderGet(module,target);
        if(!row || target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        bool seen=false;
        for(size_t before=0;before<t && !seen;++before){
            const XL6_IrCallTarget* prior=&module->callTargets[before];
            const XL6_ForeignFunctionHeaderContract* previous=xl6IrForeignFunctionHeaderGet(module,prior);
            if(previous && prior->reachability!=XL6_IR_REACHABILITY_UNREACHABLE && !strcmp(previous->header,row->header))seen=true;
        }
        if(!seen)xl6OutputPrintf(out,"#include <%s>\n",row->header);
    }
    for(size_t t=0;t<module->callTargetCount;++t) {
        const XL6_IrCallTarget* target=&module->callTargets[t];
        if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        for(size_t p=0;p<target->parameterCount;++p) {
            const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,p);
            if(row && row->header[0]) {
                bool seen=false;
                for(size_t before=0;before<t && !seen;++before) {
                    const XL6_IrCallTarget* prior=&module->callTargets[before];
                    if(prior->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
                    for(size_t q=0;q<prior->parameterCount;++q) {
                        const XL6_ForeignParameterContract* previous=xl6IrForeignParameterGet(module,prior,q);
                        if(previous && !strcmp(previous->header,row->header)){seen=true;break;}
                    }
                }
                for(size_t q=0;q<p && !seen;++q) {
                    const XL6_ForeignParameterContract* previous=xl6IrForeignParameterGet(module,target,q);
                    if(previous && !strcmp(previous->header,row->header))seen=true;
                }
                if(!seen)xl6OutputPrintf(out,"#include <%s>\n",row->header);
            }
        }
    }
    /* C owns the representation of declared external handles. Only selected
     * physical types bring in their authoritative declaration header. */
    for (typeIndex = 0; typeIndex < module->types.count; ++typeIndex) {
        const XL6_IrType* type = &module->types.items[typeIndex];
        if (!type->foreignHeader || (type->cAbi != XL6_IR_C_ABI_NATIVE_HANDLE && !type->foreignPolicy && type->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLBACK) ||
            !xl6IrCgenTypeSelected(module,
                (XL6_IrTypeId)(typeIndex + 1), selectedTypes)) continue;
        int seen = 0, seenType = 0;
        for (size_t earlier = 0; earlier < typeIndex; ++earlier) {
            const XL6_IrType* previous = &module->types.items[earlier];
            if (previous->foreignHeader && xl6IrCgenTypeSelected(module,
                    (XL6_IrTypeId)(earlier + 1), selectedTypes) &&
                !strcmp(previous->foreignHeader, type->foreignHeader)) {
                seen = 1;
                if (!strcmp(previous->cName, type->cName)) { seenType = 1; break; }
            }
        }
        if (!seen) xl6OutputPrintf(out, "#include <%s>\n", type->foreignHeader);
        if(type->foreignDomain){
            xl6OutputPuts("#include <__xl_runtime/xl6_ownership_catalog.h>\n",out);
            xl6OutputPrintf(out,"XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&%s, const XL6_OwnershipDomainV1*), \"native ownership domain type\");\n",type->foreignDomain);
        }
        if (seenType) continue;
        xl6OutputPrintf(out,
            "XLIR_NATIVE_ASSERT(sizeof(%s) == sizeof(void*), \"external handle width\");\n",
            type->cName);
        xl6OutputPrintf(out,
            "XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_POINTER(%s), \"external handle must be a C pointer type\");\n", type->cName);
    }
    /* Native record declarations precede every product which may contain one.
     * The native header owns layout and checked-result ABI; no duplicate body. */
    for (typeIndex=0;typeIndex<module->types.count;++typeIndex) {
        const XL6_IrType* type=&module->types.items[typeIndex];
        if (type->cAbi != XL6_IR_C_ABI_NATIVE_VALUE ||
            !xl6IrCgenTypeSelected(module,(XL6_IrTypeId)(typeIndex+1),selectedTypes)) continue;
        const XL6_IrNativeValuePlan* plan=xl6IrNativeValuePlanForType(type);
        if (!plan) return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,
            "XLIR native value type has no closed resident ABI");
        xl6OutputPrintf(out,"#include <%s>\n",plan->header);
    }
    for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex ) {
        const XL6_IrType* product = &module->types.items[typeIndex];
        size_t elementIndex;
        if ( !xl6IrCgenTypeSelected(
                 module, (XL6_IrTypeId)(typeIndex + 1), selectedTypes) )
            continue;
        if ( product->cAbi == XL6_IR_C_ABI_TAGGED_NULLABLE ) {
            const XL6_IrType* element = product->kind == XL6_IR_TYPE_NULLABLE &&
                product->paramCount == 1 && product->params != NULL
                    ? xl6IrTypeGet(&module->types, product->params[0]) : NULL;
            const char* cType = xl6IrCgenNullableScalarCarrier(module, (XL6_IrTypeId)(typeIndex + 1));
            XL6_IrTypeId unionElement=xl6IrNullableUnionElementType(module,(XL6_IrTypeId)(typeIndex+1));
            if (unionElement) {
                xl6OutputPrintf(out,"typedef struct %s { bool hasValue; %s value; } %s;\n",
                    product->cName,xl6IrCgenType(module,unionElement),product->cName);
                emitted=1; continue;
            }
            if ( product->cName[0] == '\0' || element == NULL || cType == NULL ||
                 element->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
                 element->cAbi == XL6_IR_C_ABI_PRODUCT )
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                     "XLIR nullable type has no deterministic tagged C ABI");
            if ( !scalarCarriersIncluded ) {
                xl6OutputPuts("#include <__xl_runtime/xl6_product_abi.h>\n", out);
                scalarCarriersIncluded = 1;
            }
            xl6OutputPrintf(out,
                "typedef %s %s;\n", cType, product->cName);
            emitted = 1;
            continue;
        }
        if ( product->cAbi != XL6_IR_C_ABI_PRODUCT ) continue;
        if ( product->kind != XL6_IR_TYPE_TUPLE || product->cName[0] == '\0' ||
             product->paramCount < 2 || product->params == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                 "XLIR product type has no deterministic structural C ABI");
        if (product->nativeProductAbi != XL6_IR_NATIVE_PRODUCT_NONE) {
            const char* guard = xl6IrNativeProductAbiGuard(product->nativeProductAbi);
            if (!guard) return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,"XLIR native product has no closed C ABI profile");
            xl6OutputPrintf(out,"#ifndef %s\n#define %s\n",guard,guard);
        }
        xl6OutputPrintf(out, "typedef struct %s {\n", product->cName);
        for ( elementIndex = 0; elementIndex < product->paramCount; ++elementIndex ) {
            const XL6_IrType* element = xl6IrTypeGet(&module->types, product->params[elementIndex]);
            const char* cType = element != NULL && element->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
                ? "struct xvalue*"
                : element != NULL && element->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR
                ? "struct xerror*"
                : element != NULL && element->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR
                ? "struct xfuture*"
                : element != NULL && element->cAbi == XL6_IR_C_ABI_CLOSURE_PTR
                ? "struct xlir_closure*"
                : xl6IrCgenType(module, product->params[elementIndex]);
            if ( element == NULL || cType == NULL || element->cAbi == XL6_IR_C_ABI_PRODUCT )
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                     "XLIR product element has no structural C value ABI");
            xl6OutputPrintf(out, "    %s e%lu;\n", cType, (unsigned long)elementIndex);
        }
        xl6OutputPrintf(out, "} %s;\n", product->cName);
        if (product->nativeProductAbi != XL6_IR_NATIVE_PRODUCT_NONE) xl6OutputPuts("#endif\n",out);
        emitted = 1;
    }
    if ( emitted ) xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}

/* Compiler-owned carriers are emitted from the type-pool spelling rather
 * than selected by C ABI in the backend.  Fixed scalar/runtime carriers stay
 * in the shared prelude below for compatibility; tagged nullable and tuple
 * carriers are exact per-TypeId products and therefore live beside their
 * value type. */
static int xl6IrCgenEmitStructuralErrorResultTypes(
    XL6_Output* out, const XL6_IrModule* module, XL6_DiagList* diags,int emitCOwned)
{
    size_t typeIndex;
    int emitted = 0;
    for ( typeIndex = 0; typeIndex < module->types.count; ++typeIndex ) {
        const XL6_IrType* type = &module->types.items[typeIndex];
        const char* valueType;
        if(xl6IrTypeIsCOwned(type) && !emitCOwned)continue;
        if ( type->cAbi != XL6_IR_C_ABI_TAGGED_NULLABLE &&
             type->cAbi != XL6_IR_C_ABI_PRODUCT &&
             type->cAbi != XL6_IR_C_ABI_C_ARRAY &&
             !xl6IrCAbiIsAggregate(type->cAbi) && !xl6IrTypeIsCOwned(type) &&
             !(type->foreignHeader && type->cAbi == XL6_IR_C_ABI_NATIVE_HANDLE)) continue;
        valueType = xl6IrCgenType(module, (XL6_IrTypeId)(typeIndex + 1));
        if ( valueType == NULL || type->errorResultCName[0] == '\0' )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                                 "XLIR structural error-result type has no canonical ABI");
        xl6OutputPrintf(out,
            "typedef struct %s { bool ok; %s value; } %s;\n",
            type->errorResultCName, valueType, type->errorResultCName);
        emitted = 1;
    }
    if ( emitted ) xl6OutputPuts("\n", out);
    return !xl6OutputHasFailed(out);
}

/* Storage linkage is already fixed by XLIR.  A public global is the only
 * storage whose C spelling is not the private s<symbol> label; instruction
 * emission must use this single mapping rather than rediscover export syntax. */
