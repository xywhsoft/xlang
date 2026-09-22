/* Text output borrows a nullable C string and flushes immediately; it never
 * consumes the owner. */
void xl6CgenEmitTextOutputRuntime(XL6_Output* out, unsigned operations)
{
    if (out == NULL || operations == 0) return;
    xl6OutputPuts(
        "static void xl6_output_write_text(const char* text)\n"
        "{\n"
        "    if (text != NULL && text[0] != '\\0')\n"
        "        (void)xrtConsoleWrite(XCONSOLE_STDOUT, xrtStrView(text));\n"
        "    (void)xrtConsoleFlush(XCONSOLE_STDOUT);\n"
        "}\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_WRITE)
        xl6OutputPuts("void xl6_xrt_write(const char* text) { xl6_output_write_text(text); }\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_WRITELN)
        xl6OutputPuts(
            "void xl6_xrt_writeln(const char* text)\n"
            "{\n"
            "    (void)xrtConsoleWriteLine(XCONSOLE_STDOUT, xrtStrView(text));\n"
            "    (void)xrtConsoleFlush(XCONSOLE_STDOUT);\n"
            "}\n", out);
}
