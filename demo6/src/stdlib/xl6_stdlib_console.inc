const char* xl6StdlibConsoleOptionsSource =
    "class ConsoleOptions\n"
    "    bool raw = true\n"
    "    bool mouse = false\n"
    "    bool paste = true\n"
    "    bool alternateScreen = false\n"
    "end\n"
    "class ConsoleStyle\n"
    "    int foreground = -1\n"
    "    int background = -1\n"
    "    int attributes = 0\n"
    "end\n"
    "\n";

const char* xl6StdlibConsoleSessionSource =
    "class ConsoleSession\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_console_session_open\" create(int flags)\n"
    "    extern \"xl6_xrt_console_session_destroy\" destroy()\n"
    "    extern \"xl6_xrt_console_session_close\" func closeNative() as bool\n"
    "    extern \"xl6_xrt_console_session_closed\" func closed() as bool\n"
    "    extern \"xl6_xrt_console_session_paste\" func pasteSupported() as bool\n"
    "    extern \"xl6_xrt_console_session_write\" func writeNative(string text) as bool\n"
    "    extern \"xl6_xrt_console_session_flush\" func flushNative() as bool\n"
    "    extern \"xl6_xrt_console_session_move\" func moveNative(int x, int y) as bool\n"
    "    extern \"xl6_xrt_console_session_clear\" func clearNative(int mode) as bool\n"
    "    extern \"xl6_xrt_console_session_cursor\" func cursorNative(bool visible) as bool\n"
    "    extern \"xl6_xrt_console_session_style\" func styleNative(int fg, int bg, int attrs) as bool\n"
    "    extern \"xl6_xrt_console_session_read\" func readEventNative(int timeoutMs) as point\n"
    "    func close()\n"
    "        if not self.closeNative() then throw IOError \"terminal close failed\" end\n"
    "    end\n"
    "    func flush()\n"
    "        if not self.flushNative() then throw IOError \"terminal flush failed\" end\n"
    "    end\n"
    "    func write(string text)\n"
    "        if not self.writeNative(text) then throw IOError \"terminal write failed\" end\n"
    "    end\n"
    "    func moveTo(int x, int y)\n"
    "        if not self.moveNative(x, y) then throw IOError \"terminal moveTo failed\" end\n"
    "    end\n"
    "    func clear(int mode = 0)\n"
    "        if not self.clearNative(mode) then throw IOError \"terminal clear failed\" end\n"
    "    end\n"
    "    func showCursor(bool visible)\n"
    "        if not self.cursorNative(visible) then throw IOError \"terminal showCursor failed\" end\n"
    "    end\n"
    "    func setStyle(ConsoleStyle style)\n"
    "        if not self.styleNative(style.foreground, style.background, style.attributes) then throw IOError \"terminal setStyle failed\" end\n"
    "    end\n"
    "    func resetStyle()\n"
    "        if not self.styleNative(-1, -1, 0) then throw IOError \"terminal style reset failed\" end\n"
    "    end\n"
    "    func readEvent(int timeoutMs = -1) as ConsoleEvent?\n"
    "        point event = self.readEventNative(timeoutMs)\n"
    "        if event == null then return null end\n"
    "        return ConsoleEvent(event)\n"
    "    end\n"
    "end\n"
    "class ConsoleEvent\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_console_event_wrap\" create(point event)\n"
    "    extern \"xl6_xrt_console_event_destroy\" destroy()\n"
    "    extern \"xl6_xrt_console_event_kind\" get kind() as int\n"
    "    extern \"xl6_xrt_console_event_text\" get text() as string\n"
    "    extern \"xl6_xrt_console_event_key\" get key() as int\n"
    "    extern \"xl6_xrt_console_event_modifiers\" get modifiers() as int\n"
    "    extern \"xl6_xrt_console_event_repeat\" get repeat() as int\n"
    "    extern \"xl6_xrt_console_event_x\" get x() as int\n"
    "    extern \"xl6_xrt_console_event_y\" get y() as int\n"
    "    extern \"xl6_xrt_console_event_wheel\" get wheel() as int\n"
    "    extern \"xl6_xrt_console_event_columns\" get columns() as int\n"
    "    extern \"xl6_xrt_console_event_rows\" get rows() as int\n"
    "    extern \"xl6_xrt_console_event_down\" get down() as bool\n"
    "end\n"
    "\n";
static const char readLineBody[] =
    "func __xl_std_console_readLine(int maxBytes = 1048576) as string?\n"
    "    return __xl_std_runtime_console_read_line(maxBytes)\n"
    "end\n"
    "\n";

static const char readCharBody[] =
    "func __xl_std_console_readChar() as char?\n"
    "    int value = __xl_std_runtime_console_read_char()\n"
    "    if value < 0 then return null end\n"
    "    return char(value)\n"
    "end\n"
    "\n";

static const char isTerminalBody[] =
    "func __xl_std_console_isTerminal(int stream = 1) as bool\n"
    "    return __xl_std_runtime_console_is_terminal(stream)\n"
    "end\n"
    "\n";

static const char sizeBody[] =
    "func __xl_std_console_size(int stream = 1) as (int, int)\n"
    "    int value = __xl_std_runtime_console_size(stream)\n"
    "    return value >> 32, value & 4294967295\n"
    "end\n"
    "\n";

static const char colorModeBody[] =
    "func __xl_std_console_colorMode(int stream = 1) as int\n"
    "    return __xl_std_runtime_console_color(stream)\n"
    "end\n"
    "\n";

static const char writeStyledBody[] =
    "func __xl_std_console_writeStyled(string text, ConsoleStyle style)\n"
    "    if not __xl_std_runtime_console_styled(text, 1, style.foreground, style.background, style.attributes) then throw IOError \"styled output failed\" end\n"
    "end\n"
    "\n";

static const char errorStyledBody[] =
    "func __xl_std_console_errorStyled(string text, ConsoleStyle style)\n"
    "    if not __xl_std_runtime_console_styled(text, 2, style.foreground, style.background, style.attributes) then throw IOError \"styled output failed\" end\n"
    "end\n"
    "\n";

static const char rgbBody[] =
    "func __xl_std_console_rgb(int red, int green, int blue) as int\n"
    "    if red < 0 or red > 255 or green < 0 or green > 255 or blue < 0 or blue > 255 then throw RangeError \"RGB component out of range\" end\n"
    "    return 16777216 | (red << 16) | (green << 8) | blue\n"
    "end\n"
    "\n";

static const char openSessionBody[] =
    "func __xl_std_console_openSession(ConsoleOptions? options = null) as ConsoleSession\n"
    "    if options == null then return ConsoleSession(5) end\n"
    "    int flags = 0\n"
    "    if options.raw then flags |= 1 end\n"
    "    if options.mouse then flags |= 2 end\n"
    "    if options.paste then flags |= 4 end\n"
    "    if options.alternateScreen then flags |= 8 end\n"
    "    return ConsoleSession(flags)\n"
    "end\n"
    "\n";
