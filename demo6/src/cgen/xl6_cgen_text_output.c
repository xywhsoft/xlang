/* Byte-for-byte legacy text-output semantics, independent of either backend:
 * borrow a nullable C string and flush immediately; never consume the owner. */
void xl6CgenEmitTextOutputRuntime(XL6_Output* out, unsigned operations)
{
    if (out == NULL || operations == 0) return;
    xl6OutputPuts(
        "#include <stdio.h>\n"
        "static void xl6_output_write_text(const char* text)\n"
        "{\n"
        "    if (text != NULL && text[0] != '\\0') (void)fputs(text, stdout);\n"
        "    (void)fflush(stdout);\n"
        "}\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_WRITE)
        xl6OutputPuts("void xl6_xrt_write(const char* text) { xl6_output_write_text(text); }\n", out);
    if (operations & XL6_CGEN_TEXT_OUTPUT_WRITELN)
        xl6OutputPuts(
            "void xl6_xrt_writeln(const char* text)\n"
            "{ xl6_output_write_text(text); (void)fputc('\\n', stdout); (void)fflush(stdout); }\n", out);
}
