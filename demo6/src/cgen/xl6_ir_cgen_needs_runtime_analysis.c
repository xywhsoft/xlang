/* Complete feature discovery before emitting declarations or runtime helpers. */
static int xl6IrCgenAnalyzeRuntimeNeeds(
    const XL6_IrModule* module, XL6_IrCgenNeeds* needs,
    XL6_DiagList* diags)
{
    size_t i;
    for (i=0;i<module->dynamicMethodCount;++i)
        if (xl6IrCgenDynamicMethodNeeded(module,i) && module->dynamicMethods[i].variadicCarrierAbi==
                XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1)
            needs->needsCallableArrayFrameProducer=1;
    for ( i = 0; i < module->instructionCount; ++i ) {
        if ( !xl6IrCgenInstructionIsReachable(module, i) ) continue;
        if (xl6IrCgenInstructionUsesListMethods(module,&module->instructions[i]))
            needs->needsListMethodsRuntime=1;
        if (xl6IrCgenListIteratorCreateSymbol(module,&module->instructions[i]))
            needs->needsListValueIteratorRuntime=1;
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            module->instructions[i].runtimeOp==XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR) {
            needs->needsCallableNativeScalarCodec=1;
            needs->needsXValueRuntime=1;
        }
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            module->instructions[i].runtimeOp==XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY) {
            const XL6_IrType* type=xl6IrTypeGet(&module->types,module->instructions[i].type);
            if (type && xl6IrNativeScalarProfileForCAbi(type->cAbi)!=XL6_NATIVE_SCALAR_NONE)
                needs->needsCallableNativeScalarCodec=1;
        }
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            module->instructions[i].runtimeOp==XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY &&
            xl6IrNativeScalarProfileForCAbi(xl6IrNullableScalarElementAbi(module,module->instructions[i].type))!=
                XL6_NATIVE_SCALAR_NONE) needs->needsCallableNativeScalarCodec=1;
        if (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
            xl6IrCgenSpecialF64RuntimeOpIs(module->instructions[i].runtimeOp))
            needs->needsSpecialF64Runtime = 1;
        if ( (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              module->instructions[i].runtimeOp >= XL6_IR_RUNTIME_STRING_DUP &&
              module->instructions[i].runtimeOp <= XL6_IR_RUNTIME_STRING_ORDER_GE) ||
             (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_FROM_UTF8 ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_COUNT ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_AT ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_CONTAINS)) ||
             (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
               (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_MESSAGE_COPY ||
                 module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_FILE_COPY ||
                 module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_DOMAIN_COPY ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_OPERATION_COPY ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_DATA_COPY ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_KIND_COPY)) ||
             (module->instructions[i].kind == XL6_IR_INSTR_RELEASE &&
              xl6IrTypeGet(&module->types, module->instructions[i].type) != NULL &&
              xl6IrTypeGet(&module->types, module->instructions[i].type)->cAbi == XL6_IR_C_ABI_MANAGED_STRING) ) {
            needs->needsStringRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_CHAR_TO_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_SCALAR_AT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_SCALAR_SET_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT_CHAR_RIGHT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT_CHAR_LEFT) ) {
            needs->needsStringScalarRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsStringViewOperations = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_FROM_DATA ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_RETAIN ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_CONCAT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_EQUAL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_GT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_GE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_LENGTH ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_AT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_SET_COPY) )
            needs->needsBytesRuntime = 1;
        if ( (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              module->instructions[i].runtimeOp >= XL6_IR_RUNTIME_XVALUE_NULL &&
              module->instructions[i].runtimeOp <= XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64) ||
             (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_FROM_DATA ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CHAR ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_RETAIN ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_CONCAT ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_EQUAL ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LT ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_LE ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_GT ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_ORDER_GE ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_LENGTH ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_AT ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BYTES_SET_COPY ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_U64 ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_U64 ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64 ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64 ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64 ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME ||
                 module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER ||
                 module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW ||
                 module->instructions[i].runtimeOp ==
                      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW ||
                 module->instructions[i].runtimeOp ==
                      XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE ||
                  xl6IrCgenTypedDictLiteralRuntimeOpIs(
                     module->instructions[i].runtimeOp) ||
                 module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_ADD ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SUB ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MUL ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_DIV ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MOD ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHL ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHR ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_POS ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_NEG ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT ||
                module->instructions[i].runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX ||
                module->instructions[i].runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE ||
                module->instructions[i].runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY ||
                module->instructions[i].runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW ||
                module->instructions[i].runtimeOp ==
                    XL6_IR_RUNTIME_XVALUE_CONTAINS)) ||
             (module->instructions[i].kind == XL6_IR_INSTR_RELEASE &&
              xl6IrTypeGet(&module->types, module->instructions[i].type) != NULL &&
              (xl6IrTypeGet(&module->types, module->instructions[i].type)->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
               xl6IrTypeGet(&module->types, module->instructions[i].type)->cAbi == XL6_IR_C_ABI_MANAGED_BYTES)) )
            needs->needsXValueRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_ADD ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SUB ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MUL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_DIV ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_MOD ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_SHR ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_POS ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_NEG ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT) ) {
            needs->needsXValueBinaryRuntime = 1;
            needs->needsStringViews = 1;
            /* Arithmetic consumes boxes directly. Array/count and managed
             * string rendering belong to their actual RuntimeOps; dynamic
             * addition requests its independent resident data adapters. */
        }
        if (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
            module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_BINARY_ADD)
            needs->needsXValueAddRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCollectionAlgebraRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            XL6_IrRuntimeOp collectionOperation =
                module->instructions[i].runtimeOp;
            needs->needsCollectionAlgebraRuntime = 1;
            if ( collectionOperation ==
                     XL6_IR_RUNTIME_XVALUE_LIST_MERGE_RIGHT )
                needs->needsCollectionListMergeRuntime = 1;
            else if ( collectionOperation ==
                          XL6_IR_RUNTIME_XVALUE_LIST_WITHOUT_I64_KEY )
                needs->needsCollectionListWithoutRuntime = 1;
            else if ( collectionOperation ==
                          XL6_IR_RUNTIME_XVALUE_DICT_MERGE_RIGHT )
                needs->needsCollectionDictMergeRuntime = 1;
            else if ( collectionOperation ==
                          XL6_IR_RUNTIME_XVALUE_DICT_WITHOUT_STRING_KEY ) {
                needs->needsCollectionDictWithoutRuntime = 1;
                needs->needsStringViews = 1;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_I64_APPROX ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_U64_APPROX ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_F64_APPROX) )
            needs->needsScalarApproxRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_APPROX ) {
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsStringSimilarityOperation = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_EQUAL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ORDER_LT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ORDER_LE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ORDER_GT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ORDER_GE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_APPROX) ) {
            needs->needsXValueRuntime = 1;
            needs->needsXValueCompareRuntime = 1;
            needs->needsScalarApproxRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsStringComparisonOperation = 1;
            needs->needsStringSimilarityOperation = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_TRUTHY )
            needs->needsXValueRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_NULL ) {
            const XL6_IrType* nullType = xl6IrTypeGet(
                &module->types, module->instructions[i].type);
            if ( nullType != NULL && nullType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL )
                needs->needsXValueRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_NULL_TEST &&
             module->instructions[i].operandCount == 1 ) {
            const XL6_IrInstruction* tested = xl6IrCgenValueDefinition(
                module, module->operands[module->instructions[i].firstOperand], NULL);
            const XL6_IrType* testedType = tested != NULL
                ? xl6IrTypeGet(&module->types, tested->type) : NULL;
            if ( testedType != NULL && testedType->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL )
                needs->needsXValueRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW) )
            needs->needsXValueArrayRuntime = 1;
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            xl6IrOwnedIndexReadRuntimeOpIs(module->instructions[i].runtimeOp)) {
            /* The leaf header is declaration-only; native requirements come
             * from the exact resident symbol rather than a generic library bit. */
            needs->needsXValueRuntime=1;
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_LIST_READ_VALUE_COPY ||
                module->instructions[i].runtimeOp==XL6_IR_RUNTIME_LIST_READ_VALUE_OR_NULL_COPY ||
                module->instructions[i].runtimeOp==XL6_IR_RUNTIME_LIST_READ_VALUE_REQUIRED_COPY)
                needs->needsListOwnedReadRuntime=1;
            else needs->needsXValueArrayOwnedReadRuntime=1;
        }
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            module->instructions[i].runtimeOp==XL6_IR_RUNTIME_XVALUE_CLONE &&
            xl6IrCgenCollectionReceiverIsList(module,&module->instructions[i]))
            needs->needsListAlgebraRuntime=1;
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            xl6IrListTypedReadRuntimeOpIs(module->instructions[i].runtimeOp)) {
            needs->needsXValueRuntime=1; needs->needsListTypedReadRuntime=1;
        }
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            module->instructions[i].runtimeOp==XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE) {
            needs->needsXValueRuntime=1; needs->needsListWriteRuntime=1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64 ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL) )
            needs->needsXValueArrayReadRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL )
            needs->needsXValueCountRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY ) {
            needs->needsXValueDynamicKeyIndexReadRuntime = 1;
            needs->needsXValueArrayReadRuntime = 1;
            needs->needsXValueCountRuntime = 1;
            needs->needsXValueIntMapRuntime = 1;
            needs->needsXValueIntMapReadRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW ) {
            needs->needsXValueDynamicKeyIndexWriteRuntime = 1;
            needs->needsXValueArraySetRuntime = 1;
            needs->needsXValueIntMapRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CONTAINS ) {
            if (!xl6IrCgenCollectionReceiverIsList(module,&module->instructions[i]))
                needs->needsXValueContainsRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE )
            needs->needsXValueArrayMethodsRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD ) {
            XL6_IrTypedCollectionOp typedCollectionOp =
                module->instructions[i].typedCollectionOp;
            const XL6_IrType* resultType = xl6IrTypeGet(
                &module->types, module->instructions[i].type);
            if (module->instructions[i].collectionAbi == XL6_IR_COLLECTION_ARRAY &&
                xl6IrCgenArrayMethodSymbol(typedCollectionOp))
                needs->needsXValueArrayMethodsRuntime = 1;
            else needs->needsXValueTypedCollectionBaseRuntime = 1;
            if ( typedCollectionOp == XL6_IR_TYPED_COLLECTION_RESIZE )
                needs->needsXValueTypedArrayResizeRuntime = 1;
            if (typedCollectionOp==XL6_IR_TYPED_COLLECTION_POP || typedCollectionOp==XL6_IR_TYPED_COLLECTION_TAKE) {
                needs->needsArrayExtractRuntime=1;
                if (resultType && resultType->cAbi==XL6_IR_C_ABI_MANAGED_STRING) {
                    needs->needsStringRuntime=1; needs->needsStringViews=1;
                }
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD ||
              (module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL &&
               module->instructions[i].dynamicCollectionOp !=
                   XL6_IR_DYNAMIC_COLLECTION_INVALID)) ) {
            needs->needsXValueDynamicCollectionRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER ) {
            needs->needsXValueArrayReadRuntime = 1;
            needs->needsXValueArrayIntegerReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_U64 ) {
            needs->needsXValueArrayReadRuntime = 1;
            needs->needsXValueArrayU64ReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL ) {
            needs->needsXValueArrayReadRuntime = 1;
            needs->needsXValueArrayBoolReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64 ) {
            needs->needsXValueArrayReadRuntime = 1;
            needs->needsXValueArrayF64ReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY ) {
            needs->needsXValueArrayStringReadRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_COUNT )
            needs->needsXValueCountRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL) ) {
            needs->needsXValueCountRuntime = 1;
            needs->needsXValueSafeCountRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW )
            needs->needsXValueArraySetRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64 ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64 ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64 ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW) )
            needs->needsXValueIntMapRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64 ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL) )
            needs->needsXValueIntMapReadRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER ) {
            needs->needsXValueIntMapReadRuntime = 1;
            needs->needsXValueIntMapIntegerReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64 ) {
            needs->needsXValueIntMapReadRuntime = 1;
            needs->needsXValueIntMapU64ReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL ) {
            needs->needsXValueIntMapReadRuntime = 1;
            needs->needsXValueIntMapBoolReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64 ) {
            needs->needsXValueIntMapReadRuntime = 1;
            needs->needsXValueIntMapF64ReadRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY ) {
            needs->needsXValueIntMapReadRuntime = 1;
            needs->needsXValueIntMapStringReadRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_NEW ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_SET_HAS) )
            needs->needsXValueSetRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE ) {
            needs->needsXValueCollectionRemoveRuntime = 1;
            /* The declaration block contains the dict-key signature even in
             * an array-only module, so its canonical view typedef is part of
             * this runtime surface unconditionally. */
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64 ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64 ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64 ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE ||
               xl6IrCgenTypedDictLiteralRuntimeOpIs(
                   module->instructions[i].runtimeOp)) )
            needs->needsXValueObjectRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW) )
        {
            needs->needsXValueDynamicFieldRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_NEW )
            needs->needsXValueClassRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp >= XL6_IR_RUNTIME_TAGGED_ENUM_NEW &&
             module->instructions[i].runtimeOp <= XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT ) {
            const XL6_IrInstruction* instruction = &module->instructions[i];
            needs->needsRuntimeTrap = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueObjectRuntime = 1;
            if ( instruction->runtimeOp == XL6_IR_RUNTIME_TAGGED_ENUM_NEW ) {
                needs->needsXValueClassRuntime = 1;
                needs->needsTaggedEnumConstruction = 1;
            }
            if ( instruction->runtimeOp == XL6_IR_RUNTIME_TAGGED_ENUM_NEW ||
                 instruction->runtimeOp == XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL )
                needs->needsXValueObjectLiteralI64Runtime = 1;
            if ( instruction->runtimeOp == XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT &&
                 instruction->enumPayload < module->enumPayloadCount ) {
                const XL6_IrType* payloadType = xl6IrTypeGet(
                    &module->types,
                    module->enumPayloads[instruction->enumPayload].type);
                if ( payloadType != NULL ) {
                    switch ( payloadType->cAbi ) {
                    case XL6_IR_C_ABI_BOOL:
                        needs->needsXValueObjectLiteralBoolRuntime = 1;
                        break;
                    case XL6_IR_C_ABI_I64:
                        needs->needsXValueObjectLiteralI64Runtime = 1;
                        break;
                    case XL6_IR_C_ABI_U64:
                    case XL6_IR_C_ABI_TYPE_ID:
                        needs->needsXValueObjectLiteralU64Runtime = 1;
                        break;
                    case XL6_IR_C_ABI_F64:
                        needs->needsXValueObjectLiteralF64Runtime = 1;
                        break;
                    case XL6_IR_C_ABI_TIME_I64:
                        needs->needsXValueObjectLiteralTimeRuntime = 1;
                        break;
                    case XL6_IR_C_ABI_MANAGED_STRING:
                        needs->needsXValueObjectLiteralStringRuntime = 1;
                        needs->needsStringRuntime = 1;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64 ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_I64) )
            needs->needsXValueObjectLiteralI64Runtime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64 ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_U64) )
            needs->needsXValueObjectLiteralU64Runtime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_BOOL) )
            needs->needsXValueObjectLiteralBoolRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64 ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_F64) )
            needs->needsXValueObjectLiteralF64Runtime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME )
            needs->needsXValueObjectLiteralTimeRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER )
            needs->needsXValueObjectLiteralRawPointerRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW )
            needs->needsXValueObjectLiteralNativePointerRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW )
            needs->needsXValueObjectLiteralNativeHandleRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE )
            needs->needsXValueObjectLiteralNativeHandleTakeRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE) )
            needs->needsXValueTaggedNullableRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_STRING_COPY) ) {
            needs->needsXValueObjectLiteralStringRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE) ) {
            needs->needsXValueClosureFieldRuntime = 1;
            needs->needsXValueClosureBridgeRuntime = 1;
            if ( module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE &&
                 xl6IrCgenRuntimeCallableHasClosureResult(
                     module, module->instructions[i].calleeType) )
                needs->needsXValueClosureProjectionRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_EQUAL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_LT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_LE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_GT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_GE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_STRING_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW) ) {
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_CONCAT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_EQUAL) )
            needs->needsStringViewOperations = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_LT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_LE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_GT ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_ORDER_GE) )
            needs->needsStringComparisonOperation = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_COUNT ) {
            needs->needsStringUtf8CountOperation = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_UTF8_AT ) {
            needs->needsStringUtf8AtOperation = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_CONTAINS ) {
            needs->needsStringContainsOperation = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_I64_RANGE_CONTAINS )
            needs->needsI64RangeContainsRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BOOL_TO_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_I64_TO_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_U64_TO_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_F64_TO_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_TIME_TO_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_TYPE_ID_TO_STRING ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_STRING) ) {
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            if ( module->instructions[i].runtimeOp == XL6_IR_RUNTIME_BOOL_TO_STRING )
                needs->needsBoolToStringRuntime = 1;
            else if ( module->instructions[i].runtimeOp == XL6_IR_RUNTIME_I64_TO_STRING )
                needs->needsI64ToStringRuntime = 1;
            else if ( module->instructions[i].runtimeOp == XL6_IR_RUNTIME_U64_TO_STRING )
                needs->needsU64ToStringRuntime = 1;
            else if ( module->instructions[i].runtimeOp == XL6_IR_RUNTIME_F64_TO_STRING )
                needs->needsF64ToStringRuntime = 1;
            else if ( module->instructions[i].runtimeOp == XL6_IR_RUNTIME_TIME_TO_STRING )
                needs->needsTimeToStringRuntime = 1;
            else if ( module->instructions[i].runtimeOp ==
                      XL6_IR_RUNTIME_TYPE_ID_TO_STRING )
                needs->needsTypeIdToStringRuntime = 1;
            else if ( module->instructions[i].runtimeOp ==
                      XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING ) {
                needs->needsClassDerivedToStringRuntime = 1;
                needs->needsXValueRuntime = 1;
            }
            else {
                needs->needsXValueToStringRuntime = 1;
                needs->needsXValueRuntime = 1;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_NEW ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT) ) {
            needs->needsXValueClassFinalizerRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_NEW) ) {
            needs->needsClassValueIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
        }
    }
    if ( needs->needsXCallRuntime ) {
        for ( i = 0; i < module->types.count; ++i ) {
            XL6_IrTypeId signatureType = (XL6_IrTypeId)(i + 1);
            int forward = xl6IrCgenRuntimeCallableAdapterNeeded(
                module, signatureType, 0);
            int reverse = xl6IrCgenRuntimeCallableAdapterNeeded(
                module, signatureType, 1);
            const XL6_IrType* signature = xl6IrTypeGet(&module->types, signatureType);
            size_t parameter;
            int errorParameter = xl6IrCgenRuntimeCallableHasErrorParameter(
                module, signatureType);
            int errorResult = xl6IrCgenRuntimeCallableHasErrorResult(
                module, signatureType);
            if ((forward || reverse) && signature != NULL &&
                signature->kind == XL6_IR_TYPE_FUNCTION) {
                const XL6_IrCallableBridge* bridge = xl6IrCallableBridgeGet(module, signatureType);
                if (!bridge) return 0;
                if (forward && bridge->variadicCarrierAbi == XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1)
                    needs->needsCallableArrayFrameProducer = 1;
                for (parameter = 0; parameter < bridge->parameterCount; ++parameter)
                    if (module->callableBridgeParameters[bridge->firstParameter + parameter].nativeScalarProfile != XL6_NATIVE_SCALAR_NONE)
                        needs->needsCallableNativeScalarCodec = 1;
                for (parameter = 0; parameter < bridge->resultCount; ++parameter)
                    if (module->callableBridgeResults[bridge->firstResult + parameter].nativeScalarProfile != XL6_NATIVE_SCALAR_NONE)
                        needs->needsCallableNativeScalarCodec = 1;
                for (parameter = 0; parameter < bridge->parameterCount; ++parameter)
                    if (module->callableBridgeParameters[bridge->firstParameter + parameter].integerDomain.rule != XL6_VALUE_ADMIT_NONE)
                        needs->needsCallableIntegerCodec = 1;
                for (parameter = 0; parameter < bridge->resultCount; ++parameter)
                    if (module->callableBridgeResults[bridge->firstResult + parameter].integerDomain.rule != XL6_VALUE_ADMIT_NONE)
                        needs->needsCallableIntegerCodec = 1;
                if (forward) for (parameter = 0; parameter < bridge->parameterCount; ++parameter)
                    if (module->callableBridgeParameters[bridge->firstParameter + parameter].decodeAbi == XL6_IR_CALLABLE_BRIDGE_F32)
                        needs->needsCallableF32Reader = 1;
                if (reverse) for (parameter = 0; parameter < bridge->resultCount; ++parameter)
                    if (module->callableBridgeResults[bridge->firstResult + parameter].decodeAbi == XL6_IR_CALLABLE_BRIDGE_F32)
                        needs->needsCallableF32Reader = 1;
                for (parameter = 0; parameter < signature->paramCount; ++parameter) {
                    const XL6_IrType* value = xl6IrTypeGet(
                        &module->types, signature->params[parameter]);
                    if (value != NULL && value->cAbi == XL6_IR_C_ABI_MANAGED_STRING)
                        needs->needsStringRuntime = 1;
                }
                for (parameter = 0; parameter < signature->returnCount; ++parameter) {
                    const XL6_IrType* value = xl6IrTypeGet(
                        &module->types, signature->returns[parameter]);
                    if (value != NULL && value->cAbi == XL6_IR_C_ABI_MANAGED_STRING)
                        needs->needsStringRuntime = 1;
                }
            }
            if ( (forward && errorResult) || (reverse && errorParameter) )
                needs->needsXValueErrorBoxRuntime = 1;
            if ( (forward && errorParameter) || (reverse && errorResult) )
                needs->needsXValueCheckedCastRuntime = 1;
            if ( ((forward || reverse) &&
                  (errorParameter || errorResult)) ) {
                needs->needsXValueHandleRuntime = 1;
                needs->needsXValueTypeIdentityRuntime = 1;
                needs->needsXValueRuntime = 1;
                needs->needsErrorRuntime = 1;
            }
        }
    }
    if ( needs->needsPersistenceRestoreFormats != XL6_IR_PERSIST_FORMAT_NONE ) {
        for ( i = 0; i < module->classLayoutCount; ++i ) {
            const XL6_IrClassLayout* layout = &module->classLayouts[i];
            if ( layout->persistenceObjectAbi !=
                     XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_SCHEMA ) continue;
            if ( ((needs->needsPersistenceRestoreFormats & XL6_IR_PERSIST_FORMAT_JSON) != 0 &&
                  xl6IrCgenPersistenceLayoutSupported(
                      module, layout->type, XL6_IR_PERSIST_FORMAT_JSON)) ||
                 ((needs->needsPersistenceRestoreFormats & XL6_IR_PERSIST_FORMAT_XLON) != 0 &&
                  xl6IrCgenPersistenceLayoutSupported(
                      module, layout->type, XL6_IR_PERSIST_FORMAT_XLON)) ) {
                needs->needsPersistenceDynamicFields = 1;
                break;
            }
        }
    }
    if ( needs->needsPersistenceDynamicFields || needs->needsXValueDynamicFieldRuntime ) {
        for ( i = 0; i < module->classLayoutCount; ++i ) {
            const XL6_IrClassLayout* layout = &module->classLayouts[i];
            if ( layout->persistenceObjectAbi !=
                     XL6_IR_PERSISTENCE_OBJECT_OPEN_DYNAMIC_ROOT ) continue;
            if ( layout->persistenceDynamicField >= module->classFieldCount ||
                 layout->persistenceDynamicHandleField >=
                     module->classFieldCount )
                return xl6IrCgenDiag(diags, module, layout->declaration,
                    "XLIR C emitter open dynamic record has no exact storage path");
            needs->openDynamicField = &module->classFields[
                layout->persistenceDynamicField];
            needs->openDynamicHandleField = &module->classFields[
                layout->persistenceDynamicHandleField];
            break;
        }
        if ( needs->needsPersistenceDynamicFields &&
             (needs->openDynamicField == NULL || needs->openDynamicHandleField == NULL) )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter dynamic record operation has no open-object storage ABI");
    }
    return 1;
}
