"""Regression checks for exact and retry-safe selective snapshot maintenance."""
import unittest

from sync_xrt_core_sections import guarded_delta


def public(name, body):
    return "/* ==== */\n/* public: " + name + " */\n/* ==== */\n" + body


class SelectiveDeclarationRetry(unittest.TestCase):
    def setUp(self):
        # Equal trailing guards deliberately reproduce a valid insertion whose
        # OLD four-line context still appears in the NEW section's own tail.
        tail = "\n#endif\n#endif\n\n\n"
        self.old = public("a.h", "#ifndef A\n#define A\n" + tail) + public("z.h", "end\n")
        self.inserted = public("b.h", "#ifndef B\n#define B\n" + tail)
        self.new = public("a.h", "#ifndef A\n#define A\n" + tail) + self.inserted + public("z.h", "end\n")

    def test_initial_and_repeated_application(self):
        actual = guarded_delta(self.old, self.old, self.new, "test")
        self.assertEqual(actual, self.new)
        for _ in range(3):
            actual = guarded_delta(actual, self.old, self.new, "test")
            self.assertEqual(actual, self.new)
            self.assertEqual(actual.count("public: b.h"), 1)

    def test_unselected_changes_survive_retry(self):
        prefix, suffix = "unselected preamble\n", "unselected tail\n"
        actual = prefix + self.new + suffix
        for _ in range(3):
            actual = guarded_delta(actual, self.old, self.new, "test")
            self.assertEqual(actual, prefix + self.new + suffix)

    def test_changed_guard_context_is_refused(self):
        actual = self.new.replace("end\n", "external change\n")
        actual += "external tail\n"
        original = actual
        # A changed context must refuse, never compensate by adding a section.
        with self.assertRaises(ValueError):
            guarded_delta(actual, self.old, self.new, "test")
        self.assertIn("external change\n", original)

    def test_modified_or_duplicate_inserted_section_is_refused(self):
        for actual in (self.new.replace("#define B", "#define B_CHANGED"),
                       self.new.replace(self.inserted, self.inserted * 2)):
            with self.assertRaises(ValueError):
                guarded_delta(actual, self.old, self.new, "test")


if __name__ == "__main__":
    unittest.main()
