/* Text output borrows an exact language string view; flush is explicit. */
void xl6CgenEmitTextOutputRuntime(XL6_Output* out, unsigned operations)
{
    if (out == NULL || operations == 0) return;
    if (operations & XL6_CGEN_TEXT_OUTPUT_WRITE)
        xl6OutputPuts("bool xl6_xrt_write(xlir_string text)\n{ return xrtConsoleWrite(XCONSOLE_STDOUT, text); }\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_WRITELN)
        xl6OutputPuts("bool xl6_xrt_writeln(xlir_string text)\n{ return xrtConsoleWriteLine(XCONSOLE_STDOUT, text); }\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_ERROR)
        xl6OutputPuts("bool xl6_xrt_console_error(xlir_string text)\n{ return xrtConsoleWrite(XCONSOLE_STDERR, text); }\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_ERRORLN)
        xl6OutputPuts("bool xl6_xrt_console_error_line(xlir_string text)\n{ return xrtConsoleWriteLine(XCONSOLE_STDERR, text); }\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_FLUSH)
        xl6OutputPuts(
            "bool xl6_xrt_console_flush(int stream)\n"
            "{ if (stream != 1 && stream != 2) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "  return xrtConsoleFlush((xconsolestream)stream); }\n", out);
}
