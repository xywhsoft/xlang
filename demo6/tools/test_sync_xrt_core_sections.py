"""Checks for the conservative section-delta snapshot generator."""
import unittest
from sync_xrt_core_sections import replace


def block(lines):
    return ("/* ========================================================================== */\n"
            "/* public: include/xrt/value.h */\n"
            "/* ========================================================================== */\n" + "".join(lines) +
            "/* ========================================================================== */\n"
            "/* public: include/xrt/next.h */\n"
            "/* ========================================================================== */\nend\n")


class SnapshotSectionTests(unittest.TestCase):
    def setUp(self):
        self.lines = [f"old line {i}\n" for i in range(24)]
        self.old = block(self.lines)
        self.new = block(self.lines[:10] + ["new resident read API\n"] + self.lines[10:])

    def apply(self, current):
        return replace(current, self.old, self.new, "public", "include/xrt/value.h")

    def test_exact_delta(self):
        self.assertEqual(self.apply(self.old), self.new)

    def test_preserves_unrelated_snapshot_text(self):
        target = self.old.replace("old line 0", "existing snapshot comment")
        expected = self.new.replace("old line 0", "existing snapshot comment")
        self.assertEqual(self.apply(target), expected)

    def test_repeat_does_not_duplicate(self):
        self.assertEqual(self.apply(self.new), self.new)

    def test_changed_context_refuses(self):
        with self.assertRaises(ValueError):
            self.apply(self.old.replace("old line 9", "conflicting local change"))

    def test_missing_section_refuses(self):
        with self.assertRaises(ValueError):
            self.apply(self.old.replace("include/xrt/value.h", "include/xrt/unknown.h"))

    def test_duplicate_section_refuses(self):
        with self.assertRaises(ValueError):
            self.apply(self.old + self.old)


if __name__ == "__main__":
    unittest.main()
