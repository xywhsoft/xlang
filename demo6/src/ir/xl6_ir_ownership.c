/* Included by xl6_ir.c after the canonical value/storage ownership rules.
 * No AST, source names, or C emitter decisions participate in this analysis. */

XL6_IrOperandOwnership xl6IrRuntimeOperandOwnership(
    XL6_IrRuntimeOp operation, size_t operand)
{
    if(xl6IrSlotMapResourceOpIs(xl6IrSlotMapResourceOp(operation)))
        return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    if(xl6IrSlotMapIteratorOpIs(operation))
        return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    switch (operation) {
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_READ:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_READ:
        return operand == 1 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME
                            : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE:
    case XL6_IR_RUNTIME_STACK_ERASE:
    case XL6_IR_RUNTIME_NATIVE_CONTAINER_PROJECT_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST:
    case XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND:
    case XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_LIST_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_SET_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_DICT_IMPORT_CHAIN:
    case XL6_IR_RUNTIME_XVALUE_TYPE_BIND:
    case XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED:
    case XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND:
    case XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT:
        return operand == 0 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME
            : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_XVALUE_ARRAY_APPEND_NEW:
    case XL6_IR_RUNTIME_XVALUE_SET_ADD_NEW:
    case XL6_IR_RUNTIME_CLASS_FIELD_INIT_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_NEW:
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_SET_NEW:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_LITERAL_NEW:
    case XL6_IR_RUNTIME_NAMED_SET_LITERAL_TAKE:
        return operand == 1 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME
            : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_XVALUE_ARRAY_SET_I64_NEW:
    case XL6_IR_RUNTIME_LIST_SET_VALUE_TAKE:
    case XL6_IR_RUNTIME_DICT_SET_VALUE_TAKE:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_SET_I64_NEW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_STRING_NEW:
    case XL6_IR_RUNTIME_XVALUE_INDEX_SET_XVALUE_KEY_NEW:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_SET_STRING_NEW:
        return operand == 2 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME
            : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_COLLECTION_METHOD:
        return operand != 0 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME
            : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    /* This is an exhaustive catalogue, not a default-borrow fallback.
     * New RuntimeOps must declare their input effects before verification. */
    case XL6_IR_RUNTIME_STRING_DUP:
    case XL6_IR_RUNTIME_STRING_COPY:
    case XL6_IR_RUNTIME_NATIVE_VALUE_COPY:
    case XL6_IR_RUNTIME_NATIVE_VALUE_EQUAL:
    case XL6_IR_RUNTIME_NATIVE_VALUE_BOX:
    case XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED:
    case XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY:
    case XL6_IR_RUNTIME_F32_AS_CHECKED:
    case XL6_IR_RUNTIME_STRING_CONCAT:
    case XL6_IR_RUNTIME_STRING_EQUAL:
    case XL6_IR_RUNTIME_NULLABLE_STRING_EQUAL:
    case XL6_IR_RUNTIME_NULLABLE_SCALAR_EQUAL:
    case XL6_IR_RUNTIME_STRING_ORDER_LT:
    case XL6_IR_RUNTIME_STRING_ORDER_LE:
    case XL6_IR_RUNTIME_STRING_ORDER_GT:
    case XL6_IR_RUNTIME_STRING_ORDER_GE:
    case XL6_IR_RUNTIME_BOOL_TO_STRING:
    case XL6_IR_RUNTIME_I64_TO_STRING:
    case XL6_IR_RUNTIME_F64_TO_STRING:
    case XL6_IR_RUNTIME_XVALUE_TO_STRING:
    case XL6_IR_RUNTIME_TYPE_ID_TO_STRING:
    case XL6_IR_RUNTIME_CLASS_DERIVED_TO_STRING:
    case XL6_IR_RUNTIME_CLASS_DERIVED_EQUAL:
    case XL6_IR_RUNTIME_STRING_TO_F64_CHECKED:
    case XL6_IR_RUNTIME_STRING_TO_F32_EXPLICIT:
    case XL6_IR_RUNTIME_XVALUE_TO_F32_EXPLICIT:
    case XL6_IR_RUNTIME_F64_TO_F32_EXPLICIT:
    case XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED:
    case XL6_IR_RUNTIME_TIME_TO_STRING:
    case XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_NULL:
    case XL6_IR_RUNTIME_XVALUE_BOOL:
    case XL6_IR_RUNTIME_XVALUE_I64:
    case XL6_IR_RUNTIME_XVALUE_F64:
    case XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR:
    case XL6_IR_RUNTIME_XVALUE_TIME:
    case XL6_IR_RUNTIME_XVALUE_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_FUTURE_REF:
    case XL6_IR_RUNTIME_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_WEAK_CREATE:
    case XL6_IR_RUNTIME_XVALUE_WEAK_LOCK:
    case XL6_IR_RUNTIME_XVALUE_CLONE:
    case XL6_IR_RUNTIME_XVALUE_DEEP_CLONE:
    case XL6_IR_RUNTIME_XVALUE_COUNT:
    case XL6_IR_RUNTIME_ARRAY_CREATE:
    case XL6_IR_RUNTIME_LIST_CREATE:
    case XL6_IR_RUNTIME_DICT_CREATE:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_NEW:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64:
    case XL6_IR_RUNTIME_ARRAY_READ_VALUE_COPY:
    case XL6_IR_RUNTIME_ARRAY_READ_VALUE_OR_NULL_COPY:
    case XL6_IR_RUNTIME_INDEX_READ_VALUE_COPY:
    case XL6_IR_RUNTIME_NAMED_READ_LITERAL_COPY:
    case XL6_IR_RUNTIME_LIST_READ_VALUE_COPY:
    case XL6_IR_RUNTIME_LIST_READ_VALUE_OR_NULL_COPY:
    case XL6_IR_RUNTIME_LIST_READ_VALUE_REQUIRED_COPY:
    case XL6_IR_RUNTIME_DICT_READ_VALUE_COPY:
    case XL6_IR_RUNTIME_DICT_READ_VALUE_OR_NULL_COPY:
    case XL6_IR_RUNTIME_DICT_READ_VALUE_REQUIRED_COPY:
    case XL6_IR_RUNTIME_DICT_READ_SLOT:
    case XL6_IR_RUNTIME_DICT_SET_SLOT:
    case XL6_IR_RUNTIME_LIST_READ_I64:
    case XL6_IR_RUNTIME_LIST_READ_U64:
    case XL6_IR_RUNTIME_LIST_READ_BOOL:
    case XL6_IR_RUNTIME_LIST_READ_F64:
    case XL6_IR_RUNTIME_LIST_READ_STRING:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_INTEGER:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_BOOL:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_F64:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_NEW:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_INTEGER:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_BOOL:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_F64:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_SET_NEW:
    case XL6_IR_RUNTIME_XVALUE_SET_HAS:
    case XL6_IR_RUNTIME_XVALUE_COLLECTION_REMOVE:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_REMOVE_VALUE:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_CONCAT:
    case XL6_IR_RUNTIME_XVALUE_LIST_MERGE_RIGHT:
    case XL6_IR_RUNTIME_XVALUE_LIST_WITHOUT_I64_KEY:
    case XL6_IR_RUNTIME_XVALUE_DICT_MERGE_RIGHT:
    case XL6_IR_RUNTIME_XVALUE_DICT_WITHOUT_STRING_KEY:
    case XL6_IR_RUNTIME_XVALUE_SET_UNION:
    case XL6_IR_RUNTIME_XVALUE_SET_INTERSECTION:
    case XL6_IR_RUNTIME_XVALUE_SET_DIFFERENCE:
    case XL6_IR_RUNTIME_XVALUE_SET_SYMMETRIC_DIFFERENCE:
    case XL6_IR_RUNTIME_XVALUE_SET_SUBSET:
    case XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUBSET:
    case XL6_IR_RUNTIME_XVALUE_SET_SUPERSET:
    case XL6_IR_RUNTIME_XVALUE_SET_PROPER_SUPERSET:
    case XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_NEW:
    case XL6_IR_RUNTIME_XVALUE_CLASS_NEW:
    case XL6_IR_RUNTIME_CLASS_DEFAULT_INVOKE:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_STRING:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE:
    case XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_BORROW:
    case XL6_IR_RUNTIME_ERROR_MESSAGE_COPY:
    case XL6_IR_RUNTIME_ERROR_CODE:
    case XL6_IR_RUNTIME_ERROR_DOMAIN_COPY:
    case XL6_IR_RUNTIME_ERROR_OPERATION_COPY:
    case XL6_IR_RUNTIME_ERROR_DATA_COPY:
    case XL6_IR_RUNTIME_ERROR_SYSTEM_CODE:
    case XL6_IR_RUNTIME_ERROR_CAUSE_REF:
    case XL6_IR_RUNTIME_ERROR_KIND_COPY:
    case XL6_IR_RUNTIME_ERROR_KIND_IS:
    case XL6_IR_RUNTIME_XVALUE_ERROR_REF:
    case XL6_IR_RUNTIME_XVALUE_CLASS_IS:
    case XL6_IR_RUNTIME_XVALUE_CLASS_AS:
    case XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE:
    case XL6_IR_RUNTIME_FUTURE_CANCEL:
    case XL6_IR_RUNTIME_FUTURE_WAIT:
    case XL6_IR_RUNTIME_FUTURE_WAIT_TIMEOUT:
    case XL6_IR_RUNTIME_FUTURE_WAIT_UNTIL:
    case XL6_IR_RUNTIME_FUTURE_IS_DONE:
    case XL6_IR_RUNTIME_FUTURE_IS_PENDING:
    case XL6_IR_RUNTIME_FUTURE_IS_CANCELLED:
    case XL6_IR_RUNTIME_FUTURE_IS_FAILED:
    case XL6_IR_RUNTIME_FUTURE_STATE:
    case XL6_IR_RUNTIME_FUTURE_STATUS:
    case XL6_IR_RUNTIME_FUTURE_ERROR_REF:
    case XL6_IR_RUNTIME_FUTURE_ALL_XVALUES:
    case XL6_IR_RUNTIME_FUTURE_RACE:
    case XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY:
    case XL6_IR_RUNTIME_FUTURE_RACE_ARRAY:
    case XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX:
    case XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY:
    case XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE:
    case XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE:
    case XL6_IR_RUNTIME_U64_TO_STRING:
    case XL6_IR_RUNTIME_XVALUE_U64:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_U64:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_U64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE:
    case XL6_IR_RUNTIME_XVALUE_RAW_POINTER_BORROW:
    case XL6_IR_RUNTIME_XVALUE_BINARY_ADD:
    case XL6_IR_RUNTIME_XVALUE_BINARY_SUB:
    case XL6_IR_RUNTIME_XVALUE_BINARY_MUL:
    case XL6_IR_RUNTIME_XVALUE_BINARY_DIV:
    case XL6_IR_RUNTIME_XVALUE_BINARY_MOD:
    case XL6_IR_RUNTIME_XVALUE_BINARY_BIT_AND:
    case XL6_IR_RUNTIME_XVALUE_BINARY_BIT_OR:
    case XL6_IR_RUNTIME_XVALUE_BINARY_BIT_XOR:
    case XL6_IR_RUNTIME_XVALUE_BINARY_SHL:
    case XL6_IR_RUNTIME_XVALUE_BINARY_SHR:
    case XL6_IR_RUNTIME_XVALUE_UNARY_POS:
    case XL6_IR_RUNTIME_XVALUE_UNARY_NEG:
    case XL6_IR_RUNTIME_XVALUE_UNARY_BIT_NOT:
    case XL6_IR_RUNTIME_I64_APPROX:
    case XL6_IR_RUNTIME_U64_APPROX:
    case XL6_IR_RUNTIME_F64_APPROX:
    case XL6_IR_RUNTIME_STRING_APPROX:
    case XL6_IR_RUNTIME_XVALUE_EQUAL:
    case XL6_IR_RUNTIME_XVALUE_ORDER_LT:
    case XL6_IR_RUNTIME_XVALUE_ORDER_LE:
    case XL6_IR_RUNTIME_XVALUE_ORDER_GT:
    case XL6_IR_RUNTIME_XVALUE_ORDER_GE:
    case XL6_IR_RUNTIME_XVALUE_APPROX:
    case XL6_IR_RUNTIME_XVALUE_TRUTHY:
    case XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE:
    case XL6_IR_RUNTIME_XVALUE_INDEX_GET_XVALUE_KEY:
    case XL6_IR_RUNTIME_XVALUE_CONTAINS:
    case XL6_IR_RUNTIME_STRING_UTF8_COUNT:
    case XL6_IR_RUNTIME_STRING_UTF8_AT:
    case XL6_IR_RUNTIME_STRING_CONTAINS:
    case XL6_IR_RUNTIME_I64_RANGE_CONTAINS:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ADVANCE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_ORDINAL_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_I64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_U64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BOOL:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_F64:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE:
    case XL6_IR_RUNTIME_XVALUE_TYPE_ID:
    case XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING:
    case XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_BORROW:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_STRING_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_CLOSURE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_CLOSURE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NATIVE_HANDLE_BORROW:
    case XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX:
    case XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX:
    case XL6_IR_RUNTIME_STRING_NULLABLE_COPY:
    case XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64_OR_NULL:
    case XL6_IR_RUNTIME_XVALUE_INT_MAP_AT_I64_OR_NULL:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_I64:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_U64:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_BOOL:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_F64:
    case XL6_IR_RUNTIME_XVALUE_DICT_GET_LITERAL_STRING_COPY:
    case XL6_IR_RUNTIME_INTEGER_DIV_CHECKED:
    case XL6_IR_RUNTIME_INTEGER_MOD_CHECKED:
    case XL6_IR_RUNTIME_INTEGER_SHL_CHECKED:
    case XL6_IR_RUNTIME_INTEGER_SHR_CHECKED:
    case XL6_IR_RUNTIME_I64_POW:
    case XL6_IR_RUNTIME_TIME_APPROX:
    case XL6_IR_RUNTIME_F64_NAN:
    case XL6_IR_RUNTIME_F64_POSITIVE_INFINITY:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_CONFIG_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_CONFIG_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_JSONL_CONFIG_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_XLONL_CONFIG_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_CONFIG_WRITE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_CONFIG_WRITE:
    case XL6_IR_RUNTIME_PERSISTENCE_JSONL_CONFIG_WRITE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLONL_CONFIG_WRITE:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_CONFIG_SINK:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_CONFIG_SINK:
    case XL6_IR_RUNTIME_PERSISTENCE_JSONL_CONFIG_SINK:
    case XL6_IR_RUNTIME_PERSISTENCE_XLONL_CONFIG_SINK:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITER_VALUE:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITER_NAME:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITER_VALUE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITER_NAME:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITER_INDEX:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_PROJECT:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_PROJECT:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_STRINGIFY:
    case XL6_IR_RUNTIME_PERSISTENCE_JSON_WRITE:
    case XL6_IR_RUNTIME_PERSISTENCE_XLON_WRITE:
    case XL6_IR_RUNTIME_XVALUE_UNION_BOX:
    case XL6_IR_RUNTIME_XVALUE_UNION_UNBOX:
    case XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME:
    case XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE:
    case XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL:
    case XL6_IR_RUNTIME_ERROR_FILE_COPY:
    case XL6_IR_RUNTIME_ERROR_LINE:
    case XL6_IR_RUNTIME_ERROR_COLUMN:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY:
    case XL6_IR_RUNTIME_FUTURE_SET_DEBUG_NAME:
    case XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY:
    case XL6_IR_RUNTIME_FUTURE_CREATE_TIME_MS:
    case XL6_IR_RUNTIME_FUTURE_COMPLETE_TIME_MS:
    case XL6_IR_RUNTIME_FUTURE_PENDING_CONTINUATIONS:
    case XL6_IR_RUNTIME_XVALUE_CAST_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED:
    case XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING:
    case XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST:
    case XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE:
    case XL6_IR_RUNTIME_THREAD_CREATE:
    case XL6_IR_RUNTIME_THREAD_WAIT:
    case XL6_IR_RUNTIME_THREAD_WAIT_TIMEOUT:
    case XL6_IR_RUNTIME_THREAD_EXIT_CODE:
    case XL6_IR_RUNTIME_THREAD_STATE:
    case XL6_IR_RUNTIME_THREAD_STOP:
    case XL6_IR_RUNTIME_CHANNEL_CREATE:
    case XL6_IR_RUNTIME_CHANNEL_SEND:
    case XL6_IR_RUNTIME_CHANNEL_TRY_SEND:
    case XL6_IR_RUNTIME_CHANNEL_RECV_INTO:
    case XL6_IR_RUNTIME_CHANNEL_TRY_RECV_INTO:
    case XL6_IR_RUNTIME_CHANNEL_RECV_TIMEOUT_INTO:
    case XL6_IR_RUNTIME_CHANNEL_CLOSE:
    case XL6_IR_RUNTIME_CHANNEL_IS_CLOSED:
    case XL6_IR_RUNTIME_CHANNEL_COUNT:
    case XL6_IR_RUNTIME_MUTEX_CREATE:
    case XL6_IR_RUNTIME_MUTEX_LOCK:
    case XL6_IR_RUNTIME_MUTEX_TRY_LOCK:
    case XL6_IR_RUNTIME_MUTEX_UNLOCK:
    case XL6_IR_RUNTIME_SEM_CREATE:
    case XL6_IR_RUNTIME_SEM_WAIT:
    case XL6_IR_RUNTIME_SEM_TRY_WAIT:
    case XL6_IR_RUNTIME_SEM_WAIT_TIMEOUT:
    case XL6_IR_RUNTIME_SEM_POST:
    case XL6_IR_RUNTIME_SEM_POST_MULTIPLE:
    case XL6_IR_RUNTIME_COND_CREATE:
    case XL6_IR_RUNTIME_COND_WAIT:
    case XL6_IR_RUNTIME_COND_WAIT_TIMEOUT:
    case XL6_IR_RUNTIME_COND_SIGNAL:
    case XL6_IR_RUNTIME_COND_BROADCAST:
    case XL6_IR_RUNTIME_RWLOCK_CREATE:
    case XL6_IR_RUNTIME_RWLOCK_READ_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_TRY_READ_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_READ_UNLOCK:
    case XL6_IR_RUNTIME_RWLOCK_WRITE_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_TRY_WRITE_LOCK:
    case XL6_IR_RUNTIME_RWLOCK_WRITE_UNLOCK:
    case XL6_IR_RUNTIME_RWLOCK_DOWNGRADE:
    case XL6_IR_RUNTIME_RWLOCK_UPGRADE:
    case XL6_IR_RUNTIME_SEM_CREATE_DEFAULT:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_POP_INTO:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_INTO:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_TIMEOUT_INTO:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CLOSE:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_COUNT:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_CAPACITY:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_RESET:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_CLOSED:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED:
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_TRY_POP:
    case XL6_IR_RUNTIME_WAIT_QUEUE_POP:
    case XL6_IR_RUNTIME_WAIT_QUEUE_POP_TIMEOUT:
    case XL6_IR_RUNTIME_CONCURRENT_QUEUE_POP_BATCH:
    case XL6_IR_RUNTIME_STACK_CREATE:
    case XL6_IR_RUNTIME_STACK_PUSH:
    case XL6_IR_RUNTIME_STACK_PUSH_BATCH:
    case XL6_IR_RUNTIME_STACK_POP_BATCH:
    case XL6_IR_RUNTIME_STACK_PEEK_BATCH:
    case XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT:
    case XL6_IR_RUNTIME_STACK_POP:
    case XL6_IR_RUNTIME_STACK_PEEK:
    case XL6_IR_RUNTIME_STACK_PEEK_AT:
    case XL6_IR_RUNTIME_STACK_TRY_POP:
    case XL6_IR_RUNTIME_STACK_TRY_PEEK:
    case XL6_IR_RUNTIME_STACK_TRY_PEEK_AT:
    case XL6_IR_RUNTIME_STACK_RESERVE:
    case XL6_IR_RUNTIME_STACK_CLONE:
    case XL6_IR_RUNTIME_STACK_EQUALS:
    case XL6_IR_RUNTIME_STACK_TRIM:
    case XL6_IR_RUNTIME_STACK_CLEAR:
    case XL6_IR_RUNTIME_STACK_COUNT:
    case XL6_IR_RUNTIME_STACK_CAPACITY:
#define XL6_TREE_QUERY_OP(id,name,search,pair,required,native) case XL6_IR_RUNTIME_TREE_QUERY_##id:
#include "src/abi/xl6_tree_query_ops.inc"
#undef XL6_TREE_QUERY_OP
#define XL6_CHANNEL_RECEIVE_OP(Id,...) case XL6_IR_RUNTIME_CHANNEL_RECEIVE_##Id:
#include "src/abi/xl6_channel_receive_ops.inc"
#undef XL6_CHANNEL_RECEIVE_OP
    case XL6_IR_RUNTIME_AVL_TREE_CREATE:
    case XL6_IR_RUNTIME_AVL_TREE_SET:
    case XL6_IR_RUNTIME_AVL_TREE_CONTAINS:
    case XL6_IR_RUNTIME_AVL_TREE_REMOVE:
    case XL6_IR_RUNTIME_AVL_TREE_TRIM_TO:
    case XL6_IR_RUNTIME_AVL_TREE_EQUALS:
    case XL6_IR_RUNTIME_AVL_TREE_MERGE:
    case XL6_IR_RUNTIME_AVL_TREE_CLEAR:
    case XL6_IR_RUNTIME_AVL_TREE_COUNT:
    case XL6_IR_RUNTIME_AVL_TREE_KEYS:
    case XL6_IR_RUNTIME_AVL_TREE_VALUES:
    case XL6_IR_RUNTIME_AVL_TREE_TRIM:
    case XL6_IR_RUNTIME_AVL_TREE_CLONE:
    case XL6_IR_RUNTIME_TAGGED_ENUM_NEW:
    case XL6_IR_RUNTIME_TAGGED_ENUM_TAG_EQUAL:
    case XL6_IR_RUNTIME_TAGGED_ENUM_PAYLOAD_PROJECT:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH:
    case XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_BATCH_INTO:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_DYNAMIC_FIELD_GET_STRING:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_ERROR_REF:
    case XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL:
    case XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_FUTURE_CAST_CHECKED:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_FUTURE_COPY:
    case XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_FUTURE_COPY:
    case XL6_IR_RUNTIME_STRING_FROM_UTF8:
    case XL6_IR_RUNTIME_BYTES_FROM_DATA:
    case XL6_IR_RUNTIME_CHAR_TO_STRING:
    case XL6_IR_RUNTIME_XVALUE_CHAR:
    case XL6_IR_RUNTIME_BYTES_RETAIN:
    case XL6_IR_RUNTIME_BYTES_CONCAT:
    case XL6_IR_RUNTIME_BYTES_EQUAL:
    case XL6_IR_RUNTIME_BYTES_ORDER_LT:
    case XL6_IR_RUNTIME_BYTES_ORDER_LE:
    case XL6_IR_RUNTIME_BYTES_ORDER_GT:
    case XL6_IR_RUNTIME_BYTES_ORDER_GE:
    case XL6_IR_RUNTIME_BYTES_LENGTH:
    case XL6_IR_RUNTIME_BYTES_AT:
    case XL6_IR_RUNTIME_BYTES_SET_COPY:
    case XL6_IR_RUNTIME_STRING_SCALAR_AT:
    case XL6_IR_RUNTIME_STRING_SCALAR_SET_COPY:
    case XL6_IR_RUNTIME_STRING_CONCAT_CHAR_RIGHT:
    case XL6_IR_RUNTIME_STRING_CONCAT_CHAR_LEFT:
        return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_RUNTIME_NONE:
    default:
        return XL6_IR_OPERAND_OWNERSHIP_INVALID;
    }
}

XL6_IrOperandOwnership xl6IrInstructionOperandOwnership(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction, size_t operand)
{
    const XL6_IrCallTarget* target;
    const XL6_IrType* signature;
    size_t i;
    if (module == NULL || instruction == NULL || operand >= instruction->operandCount)
        return XL6_IR_OPERAND_OWNERSHIP_INVALID;
    switch (instruction->kind) {
    case XL6_IR_INSTR_RUNTIME_CALL:
        return xl6IrRuntimeOperandOwnership(instruction->runtimeOp, operand);
    case XL6_IR_INSTR_STORE:
    case XL6_IR_INSTR_PRODUCT_STORE_MOVE:
    case XL6_IR_INSTR_VALUE_RELEASE:
    case XL6_IR_INSTR_CELL_INIT:
    case XL6_IR_INSTR_CELL_STORE:
    case XL6_IR_INSTR_CELL_EXCHANGE:
    case XL6_IR_INSTR_PRODUCT_MAKE:
    case XL6_IR_INSTR_RETURN:
    case XL6_IR_INSTR_YIELD:
    case XL6_IR_INSTR_ERROR_RAISE:
    case XL6_IR_INSTR_ERROR_SET_TAKE:
    case XL6_IR_INSTR_DROP:
        return XL6_IR_OPERAND_OWNERSHIP_CONSUME;
    case XL6_IR_INSTR_VALUE:
    case XL6_IR_INSTR_NULLABLE_WRAP:
    case XL6_IR_INSTR_NULLABLE_UNWRAP:
        return XL6_IR_OPERAND_OWNERSHIP_ALIAS;
    case XL6_IR_INSTR_MOVE:
        return XL6_IR_OPERAND_OWNERSHIP_DETACH;
    case XL6_IR_INSTR_PRODUCT_TAKE:
    case XL6_IR_INSTR_PRODUCT_DROP:
        return operand == 0 ? XL6_IR_OPERAND_OWNERSHIP_CONSUME_FIELD
            : XL6_IR_OPERAND_OWNERSHIP_INVALID;
    case XL6_IR_INSTR_CALL:
    case XL6_IR_INSTR_ASYNC_START:
    case XL6_IR_INSTR_GENERATOR_START:
        target = xl6IrCallTargetGet(module, instruction->targetSymbol);
        if (target == NULL) return XL6_IR_OPERAND_OWNERSHIP_INVALID;
        if (operand >= target->parameterCount)
            return target->variadicAbi == XL6_IR_CALL_VARIADIC_C
                ? XL6_IR_OPERAND_OWNERSHIP_OBSERVE : XL6_IR_OPERAND_OWNERSHIP_INVALID;
        if (target->parameterAbis == NULL) return XL6_IR_OPERAND_OWNERSHIP_INVALID;
        return target->parameterAbis[operand] == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_INSTR_CLOSURE_CALL:
        if (operand == 0) return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
        signature = xl6IrTypeGet(&module->types, instruction->calleeType);
        if (signature == NULL || operand - 1 >= signature->paramCount || signature->parameterAbis == NULL)
            return XL6_IR_OPERAND_OWNERSHIP_INVALID;
        return signature->parameterAbis[operand - 1] == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    case XL6_IR_INSTR_VIRTUAL_CALL: {
        const XL6_IrClassDispatchSlot* slot;
        if (instruction->dispatchSlot == 0 || instruction->dispatchSlot > module->classDispatchSlotCount)
            return XL6_IR_OPERAND_OWNERSHIP_INVALID;
        if (operand == 0) return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
        slot = &module->classDispatchSlots[instruction->dispatchSlot - 1];
        if (operand - 1 >= slot->parameterCount || slot->parameterAbis == NULL)
            return XL6_IR_OPERAND_OWNERSHIP_INVALID;
        return slot->parameterAbis[operand - 1] == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    }
    case XL6_IR_INSTR_PROTOCOL_CALL: {
        const XL6_IrProtocolDispatchSlot* slot;
        if (operand == 0) return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
        if (instruction->dispatchSlot == 0 || instruction->dispatchSlot > module->protocolDispatchSlotCount)
            return XL6_IR_OPERAND_OWNERSHIP_INVALID;
        slot = &module->protocolDispatchSlots[instruction->dispatchSlot - 1];
        if (operand - 1 >= slot->parameterCount || slot->parameterAbis == NULL)
            return XL6_IR_OPERAND_OWNERSHIP_INVALID;
        return slot->parameterAbis[operand - 1] == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    }
    case XL6_IR_INSTR_CLOSURE_MAKE:
        for (i = 0; i < module->closureTargetCount; ++i) {
            const XL6_IrClosureTarget* closure = &module->closureTargets[i];
            if (closure->targetSymbol != instruction->targetSymbol) continue;
            if (operand >= closure->captureCount || closure->firstCapture > module->closureCaptureCount ||
                closure->captureCount > module->closureCaptureCount - closure->firstCapture)
                return XL6_IR_OPERAND_OWNERSHIP_INVALID;
            /* Cell retain is explicit before construction; other captures
             * copy/retain inside the verified environment transaction. */
            return module->closureCaptures[closure->firstCapture + operand].abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_CELL
                ? XL6_IR_OPERAND_OWNERSHIP_CONSUME : XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
        }
        return XL6_IR_OPERAND_OWNERSHIP_INVALID;
    case XL6_IR_INSTR_AWAIT:
    case XL6_IR_INSTR_GENERATOR_NEXT:
    case XL6_IR_INSTR_GENERATOR_PREPARE_YIELD:
    case XL6_IR_INSTR_GENERATOR_ITEM_GET:
    case XL6_IR_INSTR_GENERATOR_ITEM_TAKE:
    case XL6_IR_INSTR_RETAIN:
    case XL6_IR_INSTR_CELL_RETAIN:
    case XL6_IR_INSTR_RELEASE:
    case XL6_IR_INSTR_CELL_RELEASE:
    case XL6_IR_INSTR_LOAD:
    case XL6_IR_INSTR_STORAGE_TAKE:
    case XL6_IR_INSTR_CELL_LOAD:
    case XL6_IR_INSTR_NULL_TEST:
    case XL6_IR_INSTR_BINARY:
    case XL6_IR_INSTR_UNARY:
    case XL6_IR_INSTR_DISCARD:
    case XL6_IR_INSTR_SCALAR_CONVERT:
    case XL6_IR_INSTR_NATIVE_HANDLE_BIND:
    case XL6_IR_INSTR_NATIVE_HANDLE_PROJECT:
    case XL6_IR_INSTR_PRODUCT_GET:
    case XL6_IR_INSTR_TRAP_IF_FALSE:
    case XL6_IR_INSTR_ERROR_TEST:
    case XL6_IR_INSTR_BRANCH:
    case XL6_IR_INSTR_SWITCH:
    case XL6_IR_INSTR_UNION_GET:
    case XL6_IR_INSTR_UNION_SET:
    case XL6_IR_INSTR_FOREIGN_RESULT_ROLLBACK:
        return XL6_IR_OPERAND_OWNERSHIP_OBSERVE;
    default:
        return XL6_IR_OPERAND_OWNERSHIP_INVALID;
    }
}

/* Transparent SSA views share one token. A storage transfer creates a new
 * token only at its exact LOAD/MOVE; following a STORE back to its initializer
 * here would falsely count the two sides of a legitimate transfer twice. */
static size_t xl6IrConsumptionRoot(const XL6_IrModule* module, XL6_IrValueId value)
{
    size_t depth, index = SIZE_MAX;
    for (depth = 0; depth <= module->instructionCount; ++depth) {
        const XL6_IrInstruction* definition = xl6IrValueDefinition(module, value, &index);
        if (definition == NULL) return SIZE_MAX;
        if (definition->kind != XL6_IR_INSTR_VALUE &&
            definition->kind != XL6_IR_INSTR_NULLABLE_WRAP &&
            definition->kind != XL6_IR_INSTR_NULLABLE_UNWRAP) return index;
        if (definition->targetValue != 0 || definition->operandCount != 1 ||
            definition->firstOperand >= module->operandCount) return SIZE_MAX;
        value = module->operands[definition->firstOperand];
    }
    return SIZE_MAX;
}

/* A failed result carrier/checked pointer has no language owner. Foreign
 * calls deliberately do not get this rule: their raw result may still need
 * rollback, independently of the callback/error scope. */
static int xl6IrOwnerAbsentOnFailure(
    const XL6_IrModule* module, const XL6_IrInstruction* producer)
{
    switch (xl6IrCallInstructionErrorPhysicalAbi(module, producer)) {
    case XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT:
    case XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER:
    case XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT:
        return 1;
    default:
        if (producer->kind == XL6_IR_INSTR_CLOSURE_CALL ||
            producer->kind == XL6_IR_INSTR_AWAIT) return 1;
        if (producer->kind != XL6_IR_INSTR_RUNTIME_CALL) return 0;
        /* Checked casts/constructors leave an empty output on failure even
         * where the older physical observer still uses the error slot. */
        switch (producer->runtimeOp) {
        case XL6_IR_RUNTIME_DICT_READ_SLOT:
            return 1;
        case XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD:
            /* Checked extraction destroys a prepared result if commit or
             * cleanup fails. A successful nullable null is distinct: this
             * rule applies only to the explicit failure CFG edge. */
            return producer->typedCollectionOp == XL6_IR_TYPED_COLLECTION_POP ||
                producer->typedCollectionOp == XL6_IR_TYPED_COLLECTION_TAKE;
        case XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED:
        case XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED:
        case XL6_IR_RUNTIME_XVALUE_CAST_CHECKED:
        case XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING:
        case XL6_IR_RUNTIME_XVALUE_BINARY_DIV:
        case XL6_IR_RUNTIME_XVALUE_BINARY_MOD:
        case XL6_IR_RUNTIME_XVALUE_BINARY_SHL:
        case XL6_IR_RUNTIME_XVALUE_BINARY_SHR:
        case XL6_IR_RUNTIME_XVALUE_WEAK_CREATE:
        case XL6_IR_RUNTIME_CHANNEL_CREATE:
        case XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE:
        case XL6_IR_RUNTIME_STACK_CREATE:
        case XL6_IR_RUNTIME_STACK_CLONE:
        case XL6_IR_RUNTIME_STACK_POP_BATCH:
        case XL6_IR_RUNTIME_STACK_PEEK_BATCH:
        case XL6_IR_RUNTIME_STACK_PEEK_BATCH_AT:
        case XL6_IR_RUNTIME_STACK_POP:
        case XL6_IR_RUNTIME_STACK_PEEK:
        case XL6_IR_RUNTIME_STACK_PEEK_AT:
        case XL6_IR_RUNTIME_STACK_TRY_POP:
        case XL6_IR_RUNTIME_STACK_TRY_PEEK:
        case XL6_IR_RUNTIME_STACK_TRY_PEEK_AT:
        case XL6_IR_RUNTIME_AVL_TREE_CREATE:
        case XL6_IR_RUNTIME_AVL_TREE_CLONE:
        case XL6_IR_RUNTIME_AVL_TREE_KEYS:
        case XL6_IR_RUNTIME_AVL_TREE_VALUES:
        case XL6_IR_RUNTIME_MUTEX_CREATE:
        case XL6_IR_RUNTIME_SEM_CREATE:
        case XL6_IR_RUNTIME_SEM_CREATE_DEFAULT:
        case XL6_IR_RUNTIME_COND_CREATE:
        case XL6_IR_RUNTIME_RWLOCK_CREATE:
            return 1;
        default:
            return 0;
        }
    }
}

/* Per dynamic acquisition, explore (block, token-state), not source/arena
 * order or total use count. Each state is visited at most once per block.
 * Mutually exclusive consumers are valid. A loop may consume a newly produced
 * token on every iteration, but may not reuse an owner acquired outside it.
 * State 0 = absent/consumed, 1 = borrowed LOAD, 2 = independently owned,
 * 3 = empty failed acquisition (one explicit null-safe sink is allowed).
 * Enumerate producers even when no consumer exists. Every owned token must
 * be discharged on every finite exit and before its SSA definition repeats.
 * A managed product is a vector of field tokens, verified independently using
 * this same CFG engine. Whole transfer/detach affects every field; TAKE/DROP
 * affects only elementIndex. No exponential aggregate-state enumeration or
 * arena-wide use counts are needed. Storage liveness is a separate proof. */
int xl6IrVerifyOwnerConsumption(const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t f, i, p, failedInstruction = 0, failedField = SIZE_MAX;
    size_t* operandRoots = NULL;
    unsigned char* roots = NULL;
    const char* reason = "unresolved operand effect";
    int result = 0;
    operandRoots = (size_t*)xrtCalloc(module->operandCount != 0 ? module->operandCount : 1, sizeof(*operandRoots));
    roots = (unsigned char*)xrtCalloc(module->instructionCount != 0 ? module->instructionCount : 1, 1);
    if (operandRoots == NULL || roots == NULL) { reason = "analysis allocation failed"; goto done; }
    for (f = 0; f < module->functionCount; ++f) {
        const XL6_IrFunction* function = &module->functions[f];
        size_t limit = function->firstInstruction + function->instructionCount, root;
        unsigned char* seen = NULL;
        size_t* queue = NULL;
        for (i = function->firstInstruction; i < limit; ++i) {
            const XL6_IrInstruction* instruction = &module->instructions[i];
            failedInstruction = i;
            if (instruction->result != 0 &&
                xl6IrValueOwnershipGet(module, instruction->result) == XL6_IR_OWNERSHIP_MOVE &&
                instruction->kind != XL6_IR_INSTR_NULL &&
                !(instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                  instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_NULL) &&
                xl6IrConsumptionRoot(module, instruction->result) == i) {
                const XL6_IrType* ownedType = xl6IrTypeGet(&module->types, instruction->type);
                if (ownedType != NULL && ownedType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED)
                    roots[i] = 1;
            }
            if (instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
                xl6IrRuntimeOperandOwnership(instruction->runtimeOp, 0) == XL6_IR_OPERAND_OWNERSHIP_INVALID)
                goto done;
            for (p = 0; p < instruction->operandCount; ++p) {
                XL6_IrOperandOwnership effect = xl6IrInstructionOperandOwnership(module, instruction, p);
                XL6_IrValueId value = module->operands[instruction->firstOperand + p];
                const XL6_IrInstruction* definition = xl6IrValueDefinition(module, value, NULL);
                const XL6_IrType* type = definition != NULL ? xl6IrTypeGet(&module->types, definition->type) : NULL;
                if (effect == XL6_IR_OPERAND_OWNERSHIP_INVALID) goto done;
                if (type == NULL || type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
                    (effect != XL6_IR_OPERAND_OWNERSHIP_CONSUME &&
                     effect != XL6_IR_OPERAND_OWNERSHIP_CONSUME_FIELD &&
                     effect != XL6_IR_OPERAND_OWNERSHIP_DETACH)) continue;
                if (effect == XL6_IR_OPERAND_OWNERSHIP_CONSUME_FIELD &&
                    (type->cAbi != XL6_IR_C_ABI_PRODUCT || instruction->elementIndex >= type->paramCount)) {
                    reason = "field consumption has no exact product element"; goto done;
                }
                root = xl6IrConsumptionRoot(module, value);
                reason = "consuming operand has no independent acquisition";
                if (root < function->firstInstruction || root >= limit) goto done;
                definition = &module->instructions[root];
                if (definition->kind == XL6_IR_INSTR_NULL) continue;
                if (definition->kind != XL6_IR_INSTR_LOAD &&
                    xl6IrValueOwnershipGet(module, definition->result) != XL6_IR_OWNERSHIP_MOVE) goto done;
                operandRoots[instruction->firstOperand + p] = root + 1;
                roots[root] = 1;
            }
        }
        if (function->blockCount == 0) continue;
        if (function->blockCount > SIZE_MAX / (4 * sizeof(*queue))) {
            reason = "analysis size overflow"; goto done;
        }
        seen = (unsigned char*)xrtMalloc(function->blockCount);
        queue = (size_t*)xrtMalloc(4 * function->blockCount * sizeof(*queue));
        if (seen == NULL || queue == NULL) {
            xrtFree(seen); xrtFree(queue); reason = "analysis allocation failed"; goto done;
        }
        for (root = function->firstInstruction; root < limit; ++root) {
            const XL6_IrType* type;
            size_t field, fieldCount;
            if (!roots[root]) continue;
            type = xl6IrTypeGet(&module->types, module->instructions[root].type);
            fieldCount = type->cAbi == XL6_IR_C_ABI_PRODUCT ? type->paramCount : 1;
            for (field = 0; field < fieldCount; ++field) {
                const XL6_IrType* fieldType = type->cAbi == XL6_IR_C_ABI_PRODUCT
                    ? xl6IrTypeGet(&module->types, type->params[field]) : type;
                size_t head = 0, tail = 0;
                if (fieldType == NULL || fieldType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED) continue;
                failedField = type->cAbi == XL6_IR_C_ABI_PRODUCT ? field : SIZE_MAX;
                memset(seen, 0, function->blockCount);
                i = function->entryBlock - 1 - function->firstBlock;
                seen[i] = 1;
                queue[tail++] = 4 * i;
                while (head < tail) {
                    size_t item = queue[head++], b = item / 4;
                    unsigned int state = (unsigned int)(item % 4);
                    const XL6_IrBlock* block = &module->blocks[function->firstBlock + b];
                    const XL6_IrInstruction* last;
                    XL6_IrBlockId edges[2];
                    int trapped = 0;
                    for (i = block->firstInstruction; i < block->firstInstruction + block->instructionCount; ++i) {
                        const XL6_IrInstruction* instruction = &module->instructions[i];
                        if (xl6IrInstructionAlwaysTraps(module, instruction)) { trapped = 1; break; }
                        for (p = 0; p < instruction->operandCount; ++p) {
                            XL6_IrOperandOwnership effect;
                            if (operandRoots[instruction->firstOperand + p] != root + 1) continue;
                            effect = xl6IrInstructionOperandOwnership(module, instruction, p);
                            if (effect == XL6_IR_OPERAND_OWNERSHIP_CONSUME_FIELD) {
                                if (instruction->elementIndex != field) continue;
                            } else if (effect != XL6_IR_OPERAND_OWNERSHIP_CONSUME &&
                                       effect != XL6_IR_OPERAND_OWNERSHIP_DETACH) continue;
                            failedInstruction = i;
                            if (instruction->kind == XL6_IR_INSTR_MOVE) {
                                if (state != 1) { reason = "storage observation detached more than once or before LOAD"; goto flow_failed; }
                                state = 2;
                            } else {
                                if (state != 2 && (state != 3 ||
                                    instruction->kind == XL6_IR_INSTR_PRODUCT_TAKE)) {
                                    reason = "owner consumed more than once or without acquisition on a CFG path";
                                    goto flow_failed;
                                }
                                state = 0;
                            }
                        }
                        if (i == root) {
                            if (state == 2) {
                                failedInstruction = i;
                                reason = "owner overwritten by repeated acquisition without a sink";
                                goto flow_failed;
                            }
                            state = instruction->kind == XL6_IR_INSTR_LOAD ? 1 : 2;
                        }
                    }
                    if (trapped) continue;
                    last = &module->instructions[block->firstInstruction + block->instructionCount - 1];
                    edges[0] = last->trueTarget; edges[1] = last->falseTarget;
                    if (state == 2 && edges[0] == 0 && edges[1] == 0) {
                        failedInstruction = root;
                        reason = "owner reaches function exit without transfer or destruction";
                        goto flow_failed;
                    }
                    for (p = 0; p < 2; ++p) {
                        size_t target;
                        unsigned int edgeState = state;
                        if (edges[p] == 0) continue;
                        if (edges[p] <= function->firstBlock || edges[p] > function->firstBlock + function->blockCount) {
                            reason = "invalid ownership CFG edge"; goto flow_failed;
                        }
                        target = edges[p] - 1 - function->firstBlock;
                        if (p == 1 && state == 2 && last->kind == XL6_IR_INSTR_ERROR_TEST &&
                            last->operandCount == 1 &&
                            xl6IrConsumptionRoot(module, module->operands[last->firstOperand]) == root &&
                            xl6IrOwnerAbsentOnFailure(module, &module->instructions[root]))
                            edgeState = 3;
                        if (!(seen[target] & (1u << edgeState))) {
                            seen[target] |= (unsigned char)(1u << edgeState);
                            queue[tail++] = 4 * target + edgeState;
                        }
                    }
                }
            }
        }
        xrtFree(seen); xrtFree(queue);
        continue;
flow_failed:
        xrtFree(seen); xrtFree(queue); goto done;
    }
    result = 1;
done:
    xrtFree(operandRoots); xrtFree(roots);
    if (!result) {
        char message[280];
        if (failedField != SIZE_MAX)
            snprintf(message, sizeof(message), "XLIR owner consumption: %s (instruction %lu, product field %lu)",
                reason, (unsigned long)failedInstruction, (unsigned long)failedField);
        else
            snprintf(message, sizeof(message), "XLIR owner consumption: %s (instruction %lu)",
                reason, (unsigned long)failedInstruction);
        return xl6IrDiag(diags, failedInstruction < module->instructionCount
            ? xl6IrNodeGet(module, module->instructions[failedInstruction].origin) : NULL, message);
    }
    return 1;
}
