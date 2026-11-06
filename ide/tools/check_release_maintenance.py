import sys
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile
sys.path.insert(0, str(Path(__file__).parent / 'update'))
import release_maintenance as release

class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=release.ROOT / 'ide/build/Release')
        self.base = Path(self.temp.name)
        self.source = self.base / 'client'
        self.source.mkdir()
        for name in ('data/release.json', 'static/sdk/NOTICE.txt', 'extlibs/test/package.json',
                     'option/defaults/settings.json', 'README.md', 'projects/private.xl',
                     'mdo-home/config/secret.json', 'option/settings.json', 'build/output.exe', 'tmp/scratch'):
            p = self.source / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('{}' if name.endswith('.json') else 'fixture')
        release.manager.write(self.source / 'data/release.json',
                              {'format': 'xlang-release-1', 'platform': 'win_x64', 'version': 1, 'displayVersion': '0.6.0'})

    def tearDown(self):
        self.temp.cleanup()

    def test_archive_and_bundle(self):
        preview = release.clean(self.source, 'win_x64', self.base / 'archive')
        self.assertFalse(preview['applied'])
        self.assertTrue((self.source / 'projects/private.xl').exists())
        result = release.clean(self.source, 'win_x64', self.base / 'archive', True)
        self.assertTrue(result['applied'])
        self.assertFalse(release.plan(self.source))
        self.assertEqual((self.base / 'archive/mdo-home/config/secret.json').read_text(), '{}')
        release.package(self.source, 'win_x64', self.base / 'client.zip')
        with zipfile.ZipFile(self.base / 'client.zip') as z:
            self.assertIn('static/sdk/NOTICE.txt', z.namelist())
            self.assertIn('extlibs/test/package.json', z.namelist())
            self.assertFalse(any(n.startswith(('projects/', 'mdo-home/', 'tmp/', 'build/')) for n in z.namelist()))

    def test_archive_must_be_outside(self):
        with self.assertRaises(ValueError):
            release.clean(self.source, 'win_x64', self.source / 'archive', True)
        self.assertTrue((self.source / 'projects/private.xl').exists())

    def test_failed_archive_rolls_back(self):
        original = Path.rename
        def rename(p, target):
            if p == self.source / 'projects':
                raise OSError('injected move failure')
            return original(p, target)
        with mock.patch.object(Path, 'rename', rename), self.assertRaises(OSError):
            release.clean(self.source, 'win_x64', self.base / 'archive', True)
        for n in ('build/output.exe', 'mdo-home/config/secret.json', 'projects/private.xl'):
            self.assertTrue((self.source / n).exists())

    def test_dirty_bundle_is_rejected(self):
        with self.assertRaises(ValueError):
            release.package(self.source, 'win_x64', self.base / 'client.zip')
        self.assertFalse((self.base / 'client.zip').exists())

if __name__ == '__main__':
    unittest.main(verbosity=2)
