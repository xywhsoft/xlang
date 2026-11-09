"""Exercise real directory moves, ambiguity and legacy platform precedence."""
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from runtime_layout import runtime_example


class ExamplePaths(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.runtime = Path(temporary.name)

    def source(self, name):
        p = self.runtime / 'examples' / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('source')
        return p

    def test_categorized_extension(self):
        p = self.source('extlibs/data_format/vcard.xl')
        self.assertEqual(runtime_example(self.runtime, 'extlibs/vcard.xl'), p)
        self.assertEqual(runtime_example(self.runtime, 'extlibs/data_format/vcard.xl'), p)

    def test_moved_root_example(self):
        p = self.source('language/oop/copy_clone.xl')
        self.assertEqual(runtime_example(self.runtime, 'copy_clone.xl'), p)

    def test_legacy_platform_is_not_redirected(self):
        flat = self.source('extlibs/vcard.xl')
        self.source('extlibs/data_format/vcard.xl')
        self.assertEqual(runtime_example(self.runtime, 'extlibs/vcard.xl'), flat)

    def test_ambiguity_is_an_error(self):
        self.source('extlibs/one/a.xl')
        self.source('extlibs/two/a.xl')
        with self.assertRaisesRegex(ValueError, 'Ambiguous'):
            runtime_example(self.runtime, 'extlibs/a.xl')

    def test_category_does_not_match_an_unrelated_example(self):
        self.source('language/a.xl')
        with self.assertRaises(FileNotFoundError):
            runtime_example(self.runtime, 'extlibs/a.xl')

    def test_explicit_paths_and_escapes_are_checked(self):
        self.source('extlibs/data_format/vcard.xl')
        with self.assertRaises(FileNotFoundError):
            runtime_example(self.runtime, 'extlibs/typo/vcard.xl')
        for path in ('../outside.xl', '/outside.xl', '..\\outside.xl', ''):
            with self.assertRaises(ValueError):
                runtime_example(self.runtime, path)


if __name__ == '__main__':
    unittest.main()
