/* Only native declarations and ordinary managed object composition. Regex
 * algorithms and capture snapshots are resident XRT adapters, never Cgen. */
const char* xl6StdlibRegexSource =
    "class Regex\n"
    "    private native point handle\n"
    "    extern \"xl6RegexCreateV1\" create(string pattern, int flags = 0, int maxPatternBytes = 1048576, int maxCaptures = 4096, int size = -1)\n"
    "    extern \"xl6RegexDestroyV1\" destroy()\n"
    "    extern \"xl6RegexPatternV1\" get pattern() as string\n"
    "    extern \"xl6RegexFlagsV1\" get flags() as int\n"
    "    extern \"xl6RegexCaptureCountV1\" get captureCount() as int\n"
    "    extern \"xl6RegexCaptureNameV1\" func captureName(int index) as string\n"
    "    extern \"xl6RegexCaptureIndexV1\" func captureIndex(string name, int size = -1) as int\n"
    "    extern \"xl6RegexTestV1\" func test(string text, bool full = false, int size = -1) as bool\n"
    "    func find(string text, int start = 0, int size = -1) as RegexMatch?\n"
    "        auto result = RegexMatch(self, text, 0, start, size)\n"
    "        if not result.matched then return null end\n"
    "        return result\n"
    "    end\n"
    "    func at(string text, int start = 0, int size = -1) as RegexMatch?\n"
    "        auto result = RegexMatch(self, text, 1, start, size)\n"
    "        if not result.matched then return null end\n"
    "        return result\n"
    "    end\n"
    "    func full(string text, int size = -1) as RegexMatch?\n"
    "        auto result = RegexMatch(self, text, 2, 0, size)\n"
    "        if not result.matched then return null end\n"
    "        return result\n"
    "    end\n"
    "end\n"
    "class RegexMatch\n"
    "    private native point handle\n"
    "    extern \"xl6RegexMatchCreateV1\" create(Regex source, string text, int mode = 0, int start = 0, int size = -1)\n"
    "    extern \"xl6RegexMatchDestroyV1\" destroy()\n"
    "    extern \"xl6RegexMatchMatchedV1\" get matched() as bool\n"
    "    extern \"xl6RegexMatchCountV1\" get captureCount() as int\n"
    "    extern \"xl6RegexMatchInputV1\" get input() as string\n"
    "    extern \"xl6RegexMatchTextV1\" func text(int index = 0) as string?\n"
    "    extern \"xl6RegexMatchGroupMatchedV1\" func groupMatched(int index = 0) as bool\n"
    "    extern \"xl6RegexMatchStartV1\" func startByte(int index = 0) as int\n"
    "    extern \"xl6RegexMatchEndV1\" func endByte(int index = 0) as int\n"
    "    extern \"xl6RegexMatchNameV1\" func captureName(int index) as string\n"
    "    extern \"xl6RegexMatchIndexV1\" func captureIndex(string name, int size = -1) as int\n"
    "end\n";
const char* xl6StdlibRegexFlagsSource =
    "enum RegexFlags as int\n"
    "    none = 0\n    ignoreCase = 1\n    multiline = 2\n    dotAll = 4\n    ungreedy = 8\nend\n";
static const char xl6StdRegexCompileBody[] =
    "func __xl_std_regex_compile(string pattern, int flags = 0, int maxPatternBytes = 1048576, int maxCaptures = 4096, int size = -1) as Regex\n"
    "    return Regex(pattern, flags, maxPatternBytes, maxCaptures, size)\nend\n";
static const char xl6StdRegexEscapeBody[] =
    "extern \"xl6RegexEscapeV1\" func __xl_std_regex_escape(string text, int size = -1) as string\n";
