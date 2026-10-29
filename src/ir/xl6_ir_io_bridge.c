/* IO file facades have closed XLIR signatures, never inferred by Cgen. */
typedef enum XL6_IrIoShape {
    XL6_IR_IO_STRING = 1,
    XL6_IR_IO_BYTES,
    XL6_IR_IO_INT,
    XL6_IR_IO_BOOL,
    XL6_IR_IO_POINTER,
    XL6_IR_IO_HANDLE,
    XL6_IR_IO_VOID,
    XL6_IR_IO_FACTORY,
    XL6_IR_IO_NULL_STRING
} XL6_IrIoShape;

typedef struct XL6_IrIoBridgeContract {
    const char* name;
    XL6_IrRuntimeBridgeKind kind;
    XL6_IrIoShape result;
    size_t parameterCount;
    XL6_IrIoShape parameters[5];
    const char* ownerDestroy;
} XL6_IrIoBridgeContract;

static const XL6_IrIoBridgeContract xl6IrIoBridgeContracts[] = {
    {"xl6_xrt_io_read_file", XL6_IR_RUNTIME_BRIDGE_IO_READ_FILE,
        XL6_IR_IO_BYTES, 2, {XL6_IR_IO_STRING, XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_io_write_file", XL6_IR_RUNTIME_BRIDGE_IO_WRITE_FILE,
        XL6_IR_IO_BOOL, 4,
        {XL6_IR_IO_STRING, XL6_IR_IO_BYTES, XL6_IR_IO_INT, XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_io_write_text_file", XL6_IR_RUNTIME_BRIDGE_IO_WRITE_TEXT_FILE,
        XL6_IR_IO_BOOL, 4,
        {XL6_IR_IO_STRING, XL6_IR_IO_STRING, XL6_IR_IO_INT, XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_io_reader_open", XL6_IR_RUNTIME_BRIDGE_IO_READER_OPEN, XL6_IR_IO_FACTORY, 1, {XL6_IR_IO_STRING}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_bytes", XL6_IR_RUNTIME_BRIDGE_IO_READER_BYTES, XL6_IR_IO_FACTORY, 2, {XL6_IR_IO_BYTES, XL6_IR_IO_INT}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_buffer", XL6_IR_RUNTIME_BRIDGE_IO_READER_BUFFER, XL6_IR_IO_FACTORY, 2, {XL6_IR_IO_POINTER, XL6_IR_IO_INT}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_stdin", XL6_IR_RUNTIME_BRIDGE_IO_READER_STDIN, XL6_IR_IO_FACTORY, 0, {0}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_destroy", XL6_IR_RUNTIME_BRIDGE_IO_READER_DESTROY, XL6_IR_IO_VOID, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_close", XL6_IR_RUNTIME_BRIDGE_IO_READER_CLOSE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_handle", XL6_IR_RUNTIME_BRIDGE_IO_READER_HANDLE, XL6_IR_IO_POINTER, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_read", XL6_IR_RUNTIME_BRIDGE_IO_READER_READ, XL6_IR_IO_BYTES, 2, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_exact", XL6_IR_RUNTIME_BRIDGE_IO_READER_EXACT, XL6_IR_IO_BYTES, 2, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_all", XL6_IR_RUNTIME_BRIDGE_IO_READER_ALL, XL6_IR_IO_BYTES, 2, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_into", XL6_IR_RUNTIME_BRIDGE_IO_READER_INTO, XL6_IR_IO_INT, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_POINTER, XL6_IR_IO_INT}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_seek", XL6_IR_RUNTIME_BRIDGE_IO_READER_SEEK, XL6_IR_IO_INT, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT, XL6_IR_IO_INT}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_tell", XL6_IR_RUNTIME_BRIDGE_IO_READER_TELL, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_size", XL6_IR_RUNTIME_BRIDGE_IO_READER_SIZE, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_eof", XL6_IR_RUNTIME_BRIDGE_IO_READER_EOF, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_can_seek", XL6_IR_RUNTIME_BRIDGE_IO_READER_CAN_SEEK, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_can_size", XL6_IR_RUNTIME_BRIDGE_IO_READER_CAN_SIZE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_reader_closed", XL6_IR_RUNTIME_BRIDGE_IO_READER_CLOSED, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_reader_destroy"},
    {"xl6_xrt_io_writer_open", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_OPEN, XL6_IR_IO_FACTORY, 2, {XL6_IR_IO_STRING, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_buffer", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_BUFFER, XL6_IR_IO_FACTORY, 0, {0}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_standard", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_STANDARD, XL6_IR_IO_FACTORY, 1, {XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_destroy", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_DESTROY, XL6_IR_IO_VOID, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_close", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CLOSE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_handle", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_HANDLE, XL6_IR_IO_POINTER, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_bytes", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_BYTES, XL6_IR_IO_INT, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_BYTES, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_text", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_TEXT, XL6_IR_IO_INT, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_STRING, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_write_buffer", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_WRITE_BUFFER, XL6_IR_IO_INT, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_POINTER, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_all_bytes", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_ALL_BYTES, XL6_IR_IO_BOOL, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_BYTES, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_all_text", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_ALL_TEXT, XL6_IR_IO_BOOL, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_STRING, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_all_buffer", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_ALL_BUFFER, XL6_IR_IO_BOOL, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_POINTER, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_flush", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_FLUSH, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_seek", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_SEEK, XL6_IR_IO_INT, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT, XL6_IR_IO_INT}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_tell", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_TELL, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_size", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_SIZE, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_can_seek", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CAN_SEEK, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_can_size", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CAN_SIZE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_closed", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_CLOSED, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_writer_to_bytes", XL6_IR_RUNTIME_BRIDGE_IO_WRITER_TO_BYTES, XL6_IR_IO_BYTES, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_writer_destroy"},
    {"xl6_xrt_io_lines_create", XL6_IR_RUNTIME_BRIDGE_IO_LINES_CREATE, XL6_IR_IO_FACTORY, 2, {XL6_IR_IO_POINTER, XL6_IR_IO_INT}, "xl6_xrt_io_lines_destroy"},
    {"xl6_xrt_io_lines_destroy", XL6_IR_RUNTIME_BRIDGE_IO_LINES_DESTROY, XL6_IR_IO_VOID, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_lines_destroy"},
    {"xl6_xrt_io_lines_close", XL6_IR_RUNTIME_BRIDGE_IO_LINES_CLOSE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_lines_destroy"},
    {"xl6_xrt_io_lines_advance", XL6_IR_RUNTIME_BRIDGE_IO_LINES_ADVANCE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_lines_destroy"},
    {"xl6_xrt_io_lines_bytes", XL6_IR_RUNTIME_BRIDGE_IO_LINES_BYTES, XL6_IR_IO_BYTES, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_lines_destroy"},
    {"xl6_xrt_io_lines_ending", XL6_IR_RUNTIME_BRIDGE_IO_LINES_ENDING, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_io_lines_destroy"},
    {"xl6_xrt_io_copy", XL6_IR_RUNTIME_BRIDGE_IO_COPY, XL6_IR_IO_INT, 4, {XL6_IR_IO_POINTER, XL6_IR_IO_POINTER, XL6_IR_IO_INT, XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_io_write_buffer_file", XL6_IR_RUNTIME_BRIDGE_IO_WRITE_BUFFER_FILE, XL6_IR_IO_BOOL, 4, {XL6_IR_IO_STRING, XL6_IR_IO_POINTER, XL6_IR_IO_INT, XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_console_read_line", XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_LINE, XL6_IR_IO_NULL_STRING, 1, {XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_console_read_char", XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_CHAR, XL6_IR_IO_INT, 0, {0}, NULL},
    {"xl6_xrt_console_is_terminal", XL6_IR_RUNTIME_BRIDGE_CONSOLE_IS_TERMINAL, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_console_size", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SIZE, XL6_IR_IO_INT, 1, {XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_console_color", XL6_IR_RUNTIME_BRIDGE_CONSOLE_COLOR, XL6_IR_IO_INT, 1, {XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_console_styled", XL6_IR_RUNTIME_BRIDGE_CONSOLE_STYLED, XL6_IR_IO_BOOL, 5, {XL6_IR_IO_STRING, XL6_IR_IO_INT, XL6_IR_IO_INT, XL6_IR_IO_INT, XL6_IR_IO_INT}, NULL},
    {"xl6_xrt_console_session_open", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_OPEN, XL6_IR_IO_FACTORY, 1, {XL6_IR_IO_INT}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_destroy", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_DESTROY, XL6_IR_IO_VOID, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_close", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CLOSE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_closed", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CLOSED, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_paste", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_PASTE, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_write", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_WRITE, XL6_IR_IO_BOOL, 2, {XL6_IR_IO_HANDLE, XL6_IR_IO_STRING}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_flush", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_FLUSH, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_move", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_MOVE, XL6_IR_IO_BOOL, 3, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT, XL6_IR_IO_INT}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_clear", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CLEAR, XL6_IR_IO_BOOL, 2, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_cursor", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_CURSOR, XL6_IR_IO_BOOL, 2, {XL6_IR_IO_HANDLE, XL6_IR_IO_BOOL}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_style", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_STYLE, XL6_IR_IO_BOOL, 4, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT, XL6_IR_IO_INT, XL6_IR_IO_INT}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_session_read", XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_READ, XL6_IR_IO_POINTER, 2, {XL6_IR_IO_HANDLE, XL6_IR_IO_INT}, "xl6_xrt_console_session_destroy"},
    {"xl6_xrt_console_event_wrap", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_WRAP, XL6_IR_IO_FACTORY, 1, {XL6_IR_IO_POINTER}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_destroy", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_DESTROY, XL6_IR_IO_VOID, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_kind", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_KIND, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_text", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_TEXT, XL6_IR_IO_STRING, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_key", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_KEY, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_modifiers", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_MODIFIERS, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_repeat", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_REPEAT, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_x", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_X, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_y", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_Y, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_wheel", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_WHEEL, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_columns", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_COLUMNS, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_rows", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_ROWS, XL6_IR_IO_INT, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
    {"xl6_xrt_console_event_down", XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_DOWN, XL6_IR_IO_BOOL, 1, {XL6_IR_IO_HANDLE}, "xl6_xrt_console_event_destroy"},
};

static const XL6_IrIoBridgeContract* xl6IrIoBridgeContract(const char* name)
{
    size_t i;
    if (name == NULL) return NULL;
    for (i = 0; i < sizeof(xl6IrIoBridgeContracts) /
                    sizeof(xl6IrIoBridgeContracts[0]); ++i)
        if (strcmp(name, xl6IrIoBridgeContracts[i].name) == 0)
            return &xl6IrIoBridgeContracts[i];
    return NULL;
}

XL6_IrRuntimeBridgeKind xl6IrRuntimeIoBridgeKind(const char* name)
{
    const XL6_IrIoBridgeContract* contract = xl6IrIoBridgeContract(name);
    return contract != NULL ? contract->kind : XL6_IR_RUNTIME_BRIDGE_NONE;
}

static int xl6IrIoTypeMatches(const XL6_IrModule* module,
    XL6_IrTypeId typeId, XL6_IrIoShape shape)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types, typeId);
    if (shape == XL6_IR_IO_VOID) return typeId == XL6_IR_TYPE_INVALID;
    if (shape == XL6_IR_IO_NULL_STRING) return type != NULL && type->kind == XL6_IR_TYPE_NULLABLE &&
        type->cAbi == XL6_IR_C_ABI_MANAGED_STRING && type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    if (shape == XL6_IR_IO_HANDLE || shape == XL6_IR_IO_FACTORY)
        return type != NULL && type->kind == XL6_IR_TYPE_CLASS &&
            type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
            type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    if (type == NULL || type->kind != XL6_IR_TYPE_BUILTIN ||
        type->collectionAbi != XL6_IR_COLLECTION_NONE || type->paramCount != 0)
        return 0;
    switch (shape) {
    case XL6_IR_IO_STRING: return type->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    case XL6_IR_IO_BYTES: return type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
    case XL6_IR_IO_INT: return type->cAbi == XL6_IR_C_ABI_I64;
    case XL6_IR_IO_BOOL: return type->cAbi == XL6_IR_C_ABI_BOOL;
    case XL6_IR_IO_POINTER: return type->cAbi == XL6_IR_C_ABI_RAW_POINTER;
    default: return 0;
    }
}

int xl6IrRuntimeIoBridgeTargetMatches(const XL6_IrModule* module,
    const XL6_IrCallTarget* target)
{
    const XL6_IrIoBridgeContract* contract;
    size_t i;
    if (module == NULL || target == NULL ||
        (contract = xl6IrIoBridgeContract(target->cName)) == NULL ||
        target->runtimeBridge != contract->kind ||
        target->kind != (contract->result == XL6_IR_IO_FACTORY
            ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_EXTERN_C) ||
        target->errorAbi != (contract->result == XL6_IR_IO_VOID
            ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE) ||
        (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
         target->errorPhysicalAbi != (contract->result == XL6_IR_IO_VOID
            ? XL6_IR_CALL_ERROR_PHYSICAL_NONE : XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR)) ||
        (contract->result == XL6_IR_IO_FACTORY
            ? (target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE ||
               target->resultPhysicalFieldDeclaration == XL6_IR_NODE_INVALID ||
               target->resultPhysicalDestroySymbol == 0)
            : (target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
               target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID ||
               target->resultPhysicalDestroySymbol != 0)) ||
        target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
        target->parameterCount != contract->parameterCount ||
        !xl6IrIoTypeMatches(module, target->returnType, contract->result) ||
        (contract->parameterCount != 0 && (target->parameterTypes == NULL || target->parameterAbis == NULL ||
        target->parameterPhysicalAbis == NULL ||
        target->parameterConstraintAbis == NULL ||
        target->parameterPhysicalFieldDeclarations == NULL))) return 0;
    for (i = 0; i < contract->parameterCount; ++i) {
        XL6_IrIoShape shape = contract->parameters[i];
        if (!xl6IrIoTypeMatches(module, target->parameterTypes[i], shape) ||
            target->parameterAbis[i] !=
                ((shape == XL6_IR_IO_STRING || shape == XL6_IR_IO_BYTES || shape == XL6_IR_IO_HANDLE)
                    ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE) ||
            target->parameterPhysicalAbis[i] != (shape == XL6_IR_IO_HANDLE
                ? XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW : XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT) ||
            target->parameterConstraintAbis[i] != XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
            (shape == XL6_IR_IO_HANDLE
                ? target->parameterPhysicalFieldDeclarations[i] == XL6_IR_NODE_INVALID
                : target->parameterPhysicalFieldDeclarations[i] != XL6_IR_NODE_INVALID))
            return 0;
        if (shape == XL6_IR_IO_HANDLE && module->stage == XL6_IR_STAGE_LOWERED) {
            const XL6_IrClassLayout* layout = xl6IrBuildClassLayoutByType(module, target->parameterTypes[i]);
            const XL6_IrCallTarget* destroy;
            const XL6_IrClassField* field;
            if (layout == NULL || layout->fieldCount != 1) return 0;
            field = &module->classFields[layout->firstField];
            destroy = xl6IrCallTargetGet(module, field->nativeDestroySymbol);
            if (field->declaration != target->parameterPhysicalFieldDeclarations[i] ||
                destroy == NULL || destroy->cName == NULL || contract->ownerDestroy == NULL ||
                strcmp(destroy->cName, contract->ownerDestroy) != 0) return 0;
        }
    }
    if (contract->result == XL6_IR_IO_FACTORY && module->stage == XL6_IR_STAGE_LOWERED) {
        const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(module, target->resultPhysicalDestroySymbol);
        if (destroy == NULL || destroy->cName == NULL || contract->ownerDestroy == NULL ||
            strcmp(destroy->cName, contract->ownerDestroy) != 0) return 0;
    }
    return 1;
}
