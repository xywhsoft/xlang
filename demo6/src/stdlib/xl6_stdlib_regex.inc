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
    "    func all(string text, int start = 0, int limit = -1, int size = -1) as RegexIterator\n"
    "        return RegexIterator(self, text, start, limit, size)\nend\n"
    "    extern \"xl6RegexReplaceV1\" func replace(string text, string replacement, int limit = -1, int size = -1, int replacementSize = -1) as string\n"
    "    func replaceWith(string text, func(RegexMatch) as string replacement, int limit = -1, int size = -1) as string\n"
    "        auto session = RegexReplacer(self, text, limit, size)\n"
    "        return session.run(func() as string\n"
    "            return replacement(RegexMatch(session))\n        end)\nend\n"
    "    func split(string text, int limit = -1, int flags = 0, int size = -1) as RegexSplitIterator\n"
    "        return RegexSplitIterator(self, text, limit, flags, size)\nend\n";
const char* xl6StdlibRegexBufferSource =
    "    extern \"xl6RegexReplaceBufferV1\" func replaceTo(Buffer target, string text, string replacement, int limit = -1, int size = -1, int replacementSize = -1) as int\n"
    "    func replaceWithTo(Buffer target, string text, func(RegexMatch) as string replacement, int limit = -1, int size = -1) as int\n"
    "        auto session = RegexReplacer(self, text, limit, size)\n"
    "        return session.runTo(target, func() as string\n"
    "            return replacement(RegexMatch(session))\n        end)\nend\n";
const char* xl6StdlibRegexEndSource =
    "end\n"
    "class RegexMatch\n"
    "    private native point handle\n"
    "    extern \"xl6RegexMatchCreateV1\" create(Regex source, string text, int mode = 0, int start = 0, int size = -1)\n"
    "    extern \"xl6RegexIteratorSnapshotV1\" create(RegexIterator source)\n"
    "    extern \"xl6RegexReplacerSnapshotV1\" create(RegexReplacer source)\n"
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
    "end\n"
    "class RegexIterator\n"
    "    private native point handle\n"
    "    extern \"xl6RegexIteratorCreateV1\" create(Regex source, string text, int start = 0, int limit = -1, int size = -1)\n"
    "    extern \"xl6RegexIteratorDestroyV1\" destroy()\n"
    "    extern \"xl6RegexIteratorHasNextV1\" func hasNext() as bool\n"
    "    private extern \"xl6RegexIteratorConsumeV1\" func consume()\n"
    "    extern \"xl6RegexIteratorCloseV1\" func close()\n"
    "    extern \"xl6RegexIteratorClosedV1\" get closed() as bool\n"
    "    func next() as RegexMatch?\n"
    "        if not self.hasNext() then return null end\n"
    "        auto item = RegexMatch(self)\n"
    "        self.consume()\n        return item\nend\n"
    "    func __iter__() as RegexIterator\n        return self\nend\n"
    "    func __next__() as (bool, RegexMatch?)\n"
    "        auto item = self.next()\n        return item != null, item\nend\n"
    "end\n"
    "class RegexSplitIterator\n"
    "    private native point handle\n"
    "    extern \"xl6RegexSplitCreateV1\" create(Regex source, string text, int limit = -1, int flags = 0, int size = -1)\n"
    "    extern \"xl6RegexSplitDestroyV1\" destroy()\n"
    "    extern \"xl6RegexSplitHasNextV1\" func hasNext() as bool\n"
    "    private extern \"xl6RegexSplitConsumeV1\" func consume()\n"
    "    extern \"xl6RegexSplitCloseV1\" func close()\n"
    "    extern \"xl6RegexSplitClosedV1\" get closed() as bool\n"
    "    func next() as RegexSplitPart?\n"
    "        if not self.hasNext() then return null end\n"
    "        auto item = RegexSplitPart(self)\n"
    "        self.consume()\n        return item\nend\n"
    "    func __iter__() as RegexSplitIterator\n        return self\nend\n"
    "    func __next__() as (bool, RegexSplitPart?)\n"
    "        auto item = self.next()\n        return item != null, item\nend\n"
    "end\n"
    "class RegexSplitPart\n"
    "    private native point handle\n"
    "    extern \"xl6RegexSplitSnapshotV1\" create(RegexSplitIterator source)\n"
    "    extern \"xl6RegexSplitPartDestroyV1\" destroy()\n"
    "    extern \"xl6RegexSplitPartTextV1\" get text() as string?\n"
    "    extern \"xl6RegexSplitPartMatchedV1\" get matched() as bool\n"
    "    extern \"xl6RegexSplitPartCaptureV1\" get captureIndex() as int\n"
    "    extern \"xl6RegexSplitPartStartV1\" get startByte() as int\n"
    "    extern \"xl6RegexSplitPartEndV1\" get endByte() as int\n"
    "end\n";
const char* xl6StdlibRegexReplacerSource =
    "class RegexReplacer\n"
    "    private native point handle\n"
    "    extern \"xl6RegexReplacerCreateV1\" create(Regex source, string text, int limit = -1, int size = -1)\n"
    "    extern \"xl6RegexReplacerDestroyV1\" destroy()\n"
    "    extern \"xl6RegexReplacerRunV1\" func run(func() as string replacement) as string\n";
const char* xl6StdlibRegexReplacerBufferSource =
    "    extern \"xl6RegexReplacerRunBufferV1\" func runTo(Buffer target, func() as string replacement) as int\n";
const char* xl6StdlibRegexSetSource =
    "class RegexSet\n"
    "    private native point handle\n"
    "    extern \"xl6RegexSetCreateV1\" create(array<string> patterns, int flags = 0, int maxPatternBytes = 1048576, int maxCaptures = 4096, int maxPatterns = 4096, int itemSize = -1)\n"
    "    extern \"xl6RegexSetDestroyV1\" destroy()\n"
    "    extern \"xl6RegexSetCountV1\" get count() as int\n"
    "    extern \"xl6RegexSetPatternV1\" func pattern(int index) as string\n"
    "    extern \"xl6RegexSetHitsV1\" func hits(string text, int start = 0, int size = -1) as array<int>\n"
    "    extern \"xl6RegexSetFirstV1\" func first(string text, int start = 0, int size = -1) as int\n"
    "    extern \"xl6RegexSetTestV1\" func test(string text, int start = 0, int size = -1) as bool\n"
    "end\n";
const char* xl6StdlibRegexFlagsSource =
    "enum RegexFlags as int\n"
    "    none = 0\n    ignoreCase = 1\n    multiline = 2\n    dotAll = 4\n    ungreedy = 8\nend\n";
const char* xl6StdlibRegexSplitFlagsSource =
    "enum RegexSplitFlags as int\n    none = 0\n    captures = 1\n    skipEmpty = 2\nend\n";
static const char xl6StdRegexCompileBody[] =
    "func __xl_std_regex_compile(string pattern, int flags = 0, int maxPatternBytes = 1048576, int maxCaptures = 4096, int size = -1) as Regex\n"
    "    return Regex(pattern, flags, maxPatternBytes, maxCaptures, size)\nend\n";
static const char xl6StdRegexEscapeBody[] =
    "extern \"xl6RegexEscapeV1\" func __xl_std_regex_escape(string text, int size = -1) as string\n";
static const char xl6StdRegexSetBody[] =
    "func __xl_std_regex_set(array<string> patterns, int flags = 0, int maxPatternBytes = 1048576, int maxCaptures = 4096, int maxPatterns = 4096, int itemSize = -1) as RegexSet\n"
    "    return RegexSet(patterns, flags, maxPatternBytes, maxCaptures, maxPatterns, itemSize)\nend\n";
static const char xl6StdRegexErrorOffsetBody[] =
    "extern \"xl6RegexErrorOffsetV1\" @no_raise func __xl_std_regex_error_offset(error value) as int\n";
static const char xl6StdRegexErrorIndexBody[] =
    "extern \"xl6RegexErrorIndexV1\" @no_raise func __xl_std_regex_error_index(error value) as int\n";
