"""Directory and file manifest staging must match the shipping VFS resolver."""
from pathlib import Path
import json
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from stage_runtime_headers import stage


class HeaderStaging(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / 'root'
        self.out = Path(self.temporary.name) / 'out'
        (self.root / 'tools').mkdir(parents=True)
        (self.root / 'src/nested').mkdir(parents=True)
        (self.root / 'src/a.h').write_bytes(b'a\r\n')
        (self.root / 'src/nested/b.h').write_bytes(b'b\n')
        (self.root / 'src/ignore.c').write_bytes(b'not a header')
        (self.root / 'extra.h').write_bytes(b'extra')
        shutil.copyfile(Path(__file__).resolve().parents[1] / 'tools/gen_tcc_vfs_resources.py',
                        self.root / 'tools/gen_tcc_vfs_resources.py')
        self.manifest = self.root / 'tools/tcc_vfs_manifest_linux_xlc.txt'
        self.manifest.write_text('dir|include/__xl_runtime|%XL6_ROOT%/src|*.h\n'
            'file|include/__xl_runtime/extra.h|%XL6_ROOT%/extra.h\n', encoding='utf-8')

    def test_directory_and_file_preserve_bytes(self):
        records = stage(self.root, self.out)
        self.assertEqual(len(records), 3)
        self.assertEqual((self.out / 'include/__xl_runtime/a.h').read_bytes(), b'a\r\n')
        self.assertEqual((self.out / 'include/__xl_runtime/nested/b.h').read_bytes(), b'b\n')
        self.assertEqual(json.loads((self.out / 'runtime-headers.json').read_text()), records)
        self.assertFalse((self.out / 'include/__xl_runtime/ignore.c').exists())
        with self.assertRaises(AssertionError):
            stage(self.root, self.out)

    def test_missing_source_fails(self):
        (self.root / 'extra.h').unlink()
        with self.assertRaises(RuntimeError):
            stage(self.root, self.out)

    def test_first_target_matches_shipping_resolver(self):
        with self.manifest.open('a') as stream:
            stream.write('file|include/__xl_runtime/a.h|%XL6_ROOT%/extra.h\n')
        stage(self.root, self.out)
        self.assertEqual((self.out / 'include/__xl_runtime/a.h').read_bytes(), b'a\r\n')


if __name__ == '__main__':
    unittest.main()
