"""Checks for the conservative section-delta snapshot generator."""
import unittest
from sync_xrt_core_sections import replace, guarded_delta, section


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

    def test_exact_section_with_ambiguous_local_diff_context(self):
        # Repeated native guard/brace lines are harmless when the WHOLE
        # selected section matches; a local conflict must still be rejected.
        lines = (["guard\n"] * 20 + ["first body\n"] +
                 ["guard\n"] * 20 + ["second body\n"] + ["guard\n"] * 20)
        old = block(lines)
        new = block(lines[:10] + ["owned slot\n"] + lines[10:])
        self.assertEqual(replace(old, old, new, "public", "include/xrt/value.h"), new)
        self.assertEqual(replace(new, old, new, "public", "include/xrt/value.h"), new)
        with self.assertRaises(ValueError):
            replace(old.replace("guard\n", "conflict\n", 1), old, new,
                    "public", "include/xrt/value.h")

    def test_changed_context_refuses(self):
        with self.assertRaises(ValueError):
            self.apply(self.old.replace("old line 9", "conflicting local change"))

    def test_missing_section_refuses(self):
        with self.assertRaises(ValueError):
            self.apply(self.old.replace("include/xrt/value.h", "include/xrt/unknown.h"))

    def test_duplicate_section_refuses(self):
        with self.assertRaises(ValueError):
            self.apply(self.old + self.old)

    def test_nearby_changes_share_one_exact_guard(self):
        lines = list(self.lines)
        lines[5] = "first edit\n"
        lines[8] = "second edit\n"
        new = block(lines)
        self.assertEqual(replace(self.old, self.old, new, "public", "include/xrt/value.h"), new)
        self.assertEqual(replace(new, self.old, new, "public", "include/xrt/value.h"), new)
        with self.assertRaises(ValueError):
            replace(self.old.replace("old line 7", "local conflict"), self.old, new, "public", "include/xrt/value.h")

    def test_separated_groups_preserve_unselected_content(self):
        lines = list(self.lines)
        lines[2] = "first edit\n"
        lines[20] = "second edit\n"
        current = self.old.replace("old line 11", "existing snapshot content")
        expected = block(lines).replace("old line 11", "existing snapshot content")
        self.assertEqual(replace(current, self.old, block(lines), "public", "include/xrt/value.h"), expected)

    def test_internal_boundary_not_part_of_public_section(self):
        old=self.old.replace("public: include/xrt/next.h", "internal: src/internal/next.h")
        new=self.new.replace("public: include/xrt/next.h", "internal: src/internal/next.h")
        self.assertEqual(replace(old,old,new,"public","include/xrt/value.h"),new)

    def test_terminal_runtime_source_requires_exact_footer_and_opt_in(self):
        marker=("/* ========================================================================== */\n"
            "/* source: extlibs/xruntime/src/runtime/runtime_dynamic_field.c */\n"
            "/* ========================================================================== */\n")
        name="extlibs/xruntime/src/runtime/runtime_dynamic_field.c"
        footer="\n#endif\n#endif\n\n#endif\n"
        old=marker+"old body\n"+footer; new=marker+"new body\n"+footer
        with self.assertRaises(ValueError): replace(old,old,new,"source",name)
        self.assertEqual(replace(old,old,new,"source",name,True),new)
        self.assertEqual(replace(new,old,new,"source",name,True),new)
        with self.assertRaises(ValueError): replace(old,old,new+"product drift\n","source",name,True)
        with self.assertRaises(ValueError): replace(old.replace("old body","conflict"),old,new,"source",name,True)

    def test_insert_remove_and_idempotence(self):
        marker = ("/* ========================================================================== */\n"
                  "/* public: include/xrt/crc32.h */\n"
                  "/* ========================================================================== */\nCRC API\n")
        new = self.old
        at = new.index("/* ========================================================================== */\n/* public: include/xrt/next.h")
        new = new[:at] + marker + new[at:]
        self.assertEqual(replace(self.old,self.old,new,"public","include/xrt/crc32.h"),new)
        self.assertEqual(replace(new,self.old,new,"public","include/xrt/crc32.h"),new)
        self.assertEqual(replace(new,new,self.old,"public","include/xrt/crc32.h"),self.old)
        self.assertEqual(replace(self.old,new,self.old,"public","include/xrt/crc32.h"),self.old)
        with self.assertRaises(ValueError):
            replace(new.replace("CRC API","conflict"),self.old,new,"public","include/xrt/crc32.h")
        with self.assertRaises(ValueError):
            replace(new.replace("CRC API","conflict"),new,self.old,"public","include/xrt/crc32.h")

    def test_feature_selection_alias(self):
        text = self.old.replace("public: include/xrt/value.h","feature selection: include/xrt/features.h")
        start,end = section(text,"public","include/xrt/features.h")
        self.assertEqual(text[start:end],"".join(self.lines))

    def test_declaration_population_guards_and_unselected_content(self):
        old = "unselected\n" + "\n".join(f"line {i}" for i in range(20)) + "\n"
        new = old.replace("line 10\n","CRC declaration\n")
        current = old.replace("unselected","local ownership comment")
        expected = new.replace("unselected","local ownership comment")
        self.assertEqual(guarded_delta(current,old,new,"test"),expected)
        self.assertEqual(guarded_delta(expected,old,new,"test"),expected)
        self.assertEqual(guarded_delta(old+old,old+old,new+new,"test"),new+new)
        with self.assertRaises(ValueError): guarded_delta(old+old,old,new,"test")
        with self.assertRaises(ValueError): guarded_delta(old.replace("line 9","conflict"),old,new,"test")


if __name__ == "__main__":
    unittest.main()
