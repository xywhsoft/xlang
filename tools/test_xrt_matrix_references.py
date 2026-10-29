import unittest

from pathlib import Path
from tempfile import TemporaryDirectory
from gen_xrt_xlang_matrix import CALL, INLINE_FUNCTION, _api_headers, _source_references


class SourceReferences(unittest.TestCase):
    def test_callback_return_types_are_not_calls(self):
        source = "struct xrtownershipref (*Ownership)(const XL6_Closure*);"
        self.assertEqual(CALL.findall(source), [])

    def test_calls_and_unknown_apis_remain_visible(self):
        source = "xrtValueRetain(value); xrtValueRelease(*slot); xrtUnknownFutureApi(x);"
        self.assertEqual(CALL.findall(source), [
            "xrtValueRetain", "xrtValueRelease", "xrtUnknownFutureApi"])

    def test_only_declared_header_inline_definitions_are_classified(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / "runtime.h"
            header.write_text("/* public: include/memory.h */\n"
                "static inline bool xrtHeaderHelper(const void* p) { return p != 0; }\n"
                "static bool xrtPrivateHelper(void) { return true; }\n"
                "XRT_API void xrtExternal(void);\n"
                "#if defined(XRT_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION_ONCE)\n"
                "static inline bool xrtImplementationOnly(void) { return true; }\n", encoding="utf-8")
            inlines = _api_headers(header, INLINE_FUNCTION)
            self.assertEqual(inlines, {"xrtHeaderHelper": "include/memory.h"})
            self.assertEqual(_api_headers(header), {"xrtExternal": "include/memory.h"})
            (root / "src").mkdir()
            (root / "release/include").mkdir(parents=True)
            (root / "release/include/xl_runtime.h").write_text("", encoding="utf-8")
            (root / "src/call.c").write_text(
                "xrtHeaderHelper(p); xrtExternal(); xrtUnknownFutureApi(x); "
                "xrtPrivateHelper(); xrtImplementationOnly();", encoding="utf-8")
            present, missing = _source_references(root, {"xrtExternal"} | set(inlines))
            self.assertEqual(set(present), {"xrtHeaderHelper", "xrtExternal"})
            self.assertEqual(set(missing), {"xrtUnknownFutureApi", "xrtPrivateHelper", "xrtImplementationOnly"})


if __name__ == "__main__":
    unittest.main()
