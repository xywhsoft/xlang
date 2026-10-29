/* Platform input, parsing and mode restoration live in XRT, not in Cgen. */
static void xl6IrCgenEmitConsoleRuntime(XL6_Output* out, uint64_t operations)
{
    xl6OutputPuts(
        "static bool xlir_console_stream(int64_t value, xconsolestream* stream)\n"
        "{ if (value < 1 || value > 2) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  *stream = (xconsolestream)value; return true; }\n"
        "static bool xlir_console_style(int64_t fg, int64_t bg, int64_t attrs)\n"
        "{ if (fg < -1 || bg < -1 || fg > 0x1ffffff || bg > 0x1ffffff || attrs < 0 || attrs > 31 ||\n"
        "      (fg > 255 && fg < 0x1000000) || (bg > 255 && bg < 0x1000000))\n"
        "    { xrtSetErrorKind(XERR_RANGE); return false; } return true; }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(READ_LINE)) xl6OutputPuts(
        "xlir_string xl6_xrt_console_read_line(int64_t limit)\n"
        "{ xbuffer* buffer; xlir_string value; xbytesview view;\n"
        "  if (limit < 0 || (uint64_t)limit > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
        "  buffer = xrtConsoleReadLine((size_t)limit); if (buffer == NULL) return (xlir_string){0};\n"
        "  view = xrtBufferView(buffer); value = xlir_string_from_utf8((const char*)view.Data, view.Size);\n"
        "  xrtBufferDestroy(buffer); return value; }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(READ_CHAR)) xl6OutputPuts(
        "int64_t xl6_xrt_console_read_char(void)\n"
        "{ uint32_t codepoint = 0; return xrtConsoleReadChar(&codepoint) == 1 ? (int64_t)codepoint : -1; }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(IS_TERMINAL)) xl6OutputPuts(
        "bool xl6_xrt_console_is_terminal(int64_t value)\n"
        "{ xconsolestream stream; return xlir_console_stream(value, &stream) && xrtConsoleIsTerminal(stream); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SIZE)) xl6OutputPuts(
        "int64_t xl6_xrt_console_size(int64_t value)\n"
        "{ xconsolestream stream; uint32_t columns=0, rows=0;\n"
        "  if (!xlir_console_stream(value, &stream) || !xrtConsoleSize(stream, &columns, &rows)) return -1;\n"
        "  if (columns > INT32_MAX) { xrtSetErrorKind(XERR_RANGE); return -1; }\n"
        "  return (int64_t)(((uint64_t)columns << 32) | rows); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(COLOR)) xl6OutputPuts(
        "int64_t xl6_xrt_console_color(int64_t value)\n"
        "{ xconsolestream stream; return xlir_console_stream(value, &stream) ? xrtConsoleColorMode(stream) : -1; }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(STYLED)) xl6OutputPuts(
        "bool xl6_xrt_console_styled(xlir_string text, int64_t value, int64_t fg, int64_t bg, int64_t attrs)\n"
        "{ xconsolestream stream;\n"
        "  return xlir_console_stream(value, &stream) && xlir_console_style(fg, bg, attrs) &&\n"
        "    xrtConsoleWriteStyled(stream, (xstrview){text.Data, text.Size}, (int32_t)fg, (int32_t)bg, (uint32_t)attrs); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_OPEN)) xl6OutputPuts(
        "void* xl6_xrt_console_session_open(int64_t flags)\n"
        "{ if (flags < 0 || flags > 15) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  return xrtConsoleSessionOpen((uint32_t)flags); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_DESTROY)) xl6OutputPuts(
        "void xl6_xrt_console_session_destroy(void* session)\n"
        "{ xerror* error = xrtTakeError(); xrtConsoleSessionDestroy((xconsolesession*)session);\n"
        "  xrtClearError(); if (error != NULL) xrtSetErrorTake(error); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_CLOSE)) xl6OutputPuts(
        "bool xl6_xrt_console_session_close(void* session)\n"
        "{ return xrtConsoleSessionClose((xconsolesession*)session); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_CLOSED)) xl6OutputPuts(
        "bool xl6_xrt_console_session_closed(void* session)\n"
        "{ return xrtConsoleSessionClosed((xconsolesession*)session); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_PASTE)) xl6OutputPuts(
        "bool xl6_xrt_console_session_paste(void* session)\n"
        "{ return xrtConsoleSessionPasteSupported((xconsolesession*)session); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_FLUSH)) xl6OutputPuts(
        "bool xl6_xrt_console_session_flush(void* session)\n"
        "{ return xrtConsoleSessionFlush((xconsolesession*)session); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_WRITE)) xl6OutputPuts(
        "bool xl6_xrt_console_session_write(void* session, xlir_string text)\n"
        "{ return xrtConsoleSessionWrite((xconsolesession*)session, (xstrview){text.Data, text.Size}); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_MOVE)) xl6OutputPuts(
        "bool xl6_xrt_console_session_move(void* session, int64_t x, int64_t y)\n"
        "{ if (x < 0 || y < 0 || x >= UINT32_MAX || y >= UINT32_MAX) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  return xrtConsoleSessionMove((xconsolesession*)session, (uint32_t)x, (uint32_t)y); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_CLEAR)) xl6OutputPuts(
        "bool xl6_xrt_console_session_clear(void* session, int64_t mode)\n"
        "{ if (mode < 0 || mode > 2) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
        "  return xrtConsoleSessionClear((xconsolesession*)session, (int)mode); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_CURSOR)) xl6OutputPuts(
        "bool xl6_xrt_console_session_cursor(void* session, bool visible)\n"
        "{ return xrtConsoleSessionCursor((xconsolesession*)session, visible); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_STYLE)) xl6OutputPuts(
        "bool xl6_xrt_console_session_style(void* session, int64_t fg, int64_t bg, int64_t attrs)\n"
        "{ return xlir_console_style(fg, bg, attrs) && xrtConsoleSessionStyle((xconsolesession*)session, (int32_t)fg, (int32_t)bg, (uint32_t)attrs); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(SESSION_READ)) xl6OutputPuts(
        "void* xl6_xrt_console_session_read(void* session, int64_t timeout)\n"
        "{ if (timeout < -1 || timeout > INT_MAX) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
        "  return xrtConsoleSessionRead((xconsolesession*)session, (int)timeout); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_WRAP)) xl6OutputPuts(
        "void* xl6_xrt_console_event_wrap(void* event) { return event; }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_DESTROY)) xl6OutputPuts(
        "void xl6_xrt_console_event_destroy(void* event)\n"
        "{ xrtConsoleEventDestroy((xconsoleevent*)event); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_KIND)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_kind(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Kind); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_KEY)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_key(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Key); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_MODIFIERS)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_modifiers(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Modifiers); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_REPEAT)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_repeat(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Repeat); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_X)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_x(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->X); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_Y)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_y(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Y); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_WHEEL)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_wheel(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Wheel); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_COLUMNS)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_columns(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Columns); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_ROWS)) xl6OutputPuts(
        "int64_t xl6_xrt_console_event_rows(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((int64_t)((xconsoleevent*)handle)->Rows); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_DOWN)) xl6OutputPuts(
        "bool xl6_xrt_console_event_down(void* handle)\n"
        "{ if (handle == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }\n"
        "  return ((bool)((xconsoleevent*)handle)->Down); }\n"
        "\n"
        , out);
    if (operations & XL6_CONSOLE_BIT(EVENT_TEXT)) xl6OutputPuts(
        "xlir_string xl6_xrt_console_event_text(void* handle)\n"
        "{ xconsoleevent* event = (xconsoleevent*)handle; xbytesview view;\n"
        "  if (event == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return (xlir_string){0}; }\n"
        "  if (event->Text == NULL) return xlir_string_from_utf8(\"\", 0);\n"
        "  view = xrtBufferView(event->Text); return xlir_string_from_utf8((const char*)view.Data, view.Size); }\n"
        "\n"
        , out);
}
