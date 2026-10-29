const char* xl6StdlibStringIteratorSource =
    "class StringIterator\n"
    "    private native point handle\n"
    "    extern \"xl6TextIteratorCreateV1\" create(string text, string separator, int mode, int size, int separatorSize)\n"
    "    extern \"xl6TextIteratorDestroyV1\" destroy()\n"
    "    extern \"xl6TextIteratorNextV1\" func next() as string?\n"
    "    extern \"xl6TextIteratorCloseV1\" func close()\n"
    "    extern \"xl6TextIteratorClosedV1\" func closed() as bool\n"
    "    func __iter__() as StringIterator\n"
    "        return self\n"
    "    end\n"
    "    func __next__() as (bool, string)\n"
    "        auto item = self.next()\n"
    "        if item == null then return false, \"\" end\n"
    "        return true, item\n"
    "    end\n"
    "end\n\n";
static const char xl6StdStringSplitIterBody[] =
    "func __xl_std_string_splitIter(string text, string separator, int size = -1, int separatorSize = -1) as StringIterator\n"
    "    return StringIterator(text, separator, 0, size, separatorSize)\n"
    "end\n\n";
static const char xl6StdStringLinesIterBody[] =
    "func __xl_std_string_linesIter(string text, int size = -1) as StringIterator\n"
    "    return StringIterator(text, \"\", 1, size, -1)\n"
    "end\n\n";
static const char xl6StdStringFieldsIterBody[] =
    "func __xl_std_string_fieldsIter(string text, int size = -1) as StringIterator\n"
    "    return StringIterator(text, \"\", 2, size, -1)\n"
    "end\n\n";
