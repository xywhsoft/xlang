const char* xl6StdlibUtf8ValidatorSource =
    "class Utf8Validator\n"
    "    private native point handle\n"
    "    extern \"xl6UnicodeValidatorCreateV1\" create()\n"
    "    extern \"xl6UnicodeValidatorDestroyV1\" destroy()\n"
    "    extern \"xl6UnicodeValidatorFeedV1\" func feed(bytes data, bool final = false, int size = -1) as int\n"
    "    extern \"xl6UnicodeValidatorFeedSpanV1\" func feed(cbytes data, bool final = false, int size = -1) as int\n"
    "    extern \"xl6UnicodeValidatorResetV1\" func reset()\n"
    "    extern \"xl6UnicodeValidatorErrorV1\" get errorOffset() as int\n"
    "    extern \"xl6UnicodeValidatorTotalV1\" get total() as int\n"
    "    extern \"xl6UnicodeValidatorFinishedV1\" get finished() as bool\n"
    "end\n";
static const char xl6StdUnicodeScalarBody[] =
    "extern \"xl6UnicodeScalarV1\" func __xl_std_unicode_scalar(int code) as bool\n";
static const char xl6StdUnicodeCharStringBody[] =
    "extern \"xl6UnicodeCharStringV1\" func __xl_std_unicode_char_string(char value) as string\n";
static const char xl6StdUnicodeByteOffsetBody[] =
    "extern \"xl6UnicodeByteOffsetV1\" func __xl_std_unicode_byte_offset(string text, int index, int size = -1) as int\n";
static const char xl6StdUnicodeScalarIndexBody[] =
    "extern \"xl6UnicodeScalarIndexV1\" func __xl_std_unicode_scalar_index(string text, int offset, int size = -1) as int\n";
static const char xl6StdUnicodeValidateTextBody[] =
    "extern \"xl6UnicodeInvalidTextV1\" func __xl_unicode_invalid_text(string text, int size) as int\n"
    "func __xl_std_unicode_validate(string text, int size = -1) as (bool, int)\n"
    "    int offset = __xl_unicode_invalid_text(text, size)\n"
    "    return offset == -1, offset\nend\n";
static const char xl6StdUnicodeValidateBytesBody[] =
    "extern \"xl6UnicodeInvalidBytesV1\" func __xl_unicode_invalid_bytes(bytes data, int size) as int\n"
    "extern \"xl6UnicodeInvalidBytesSpanV1\" func __xl_unicode_invalid_bytes(cbytes data, int size) as int\n"
    "func __xl_std_unicode_validate(bytes data, int size = -1) as (bool, int)\n"
    "    int offset = __xl_unicode_invalid_bytes(data, size)\n"
    "    return offset == -1, offset\nend\n";
static const char xl6StdUnicodeValidatorBody[] =
    "func __xl_std_unicode_validator() as Utf8Validator\n"
    "    return Utf8Validator()\nend\n";
static const char xl6StdHtmlEscapeTextBody[] =
    "extern \"xl6HtmlEscapeTextV1\" func __xl_std_html_escape_text(string text, int size = -1) as string\n";
static const char xl6StdHtmlEscapeAttributeBody[] =
    "extern \"xl6HtmlEscapeAttributeV1\" func __xl_std_html_escape_attribute(string text, int size = -1) as string\n";
