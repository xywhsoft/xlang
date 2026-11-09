"""Safety checks for material recovery and the pre-push history guard."""
from pathlib import Path
import hashlib
import json
import subprocess
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import dev_materials as materials
import git_repository_budget as budget
import install_git_budget_hook as installer


class RepositoryMaintenance(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.repo = self.root / 'repo'
        subprocess.run(['git', 'init', '-q', str(self.repo)], check=True)
        self.raw = b'preserved\0binary\xffmaterial\n'
        self.oid = subprocess.check_output(['git', 'hash-object', '-w', '--stdin'], cwd=self.repo, input=self.raw).decode().strip()
        self.spec = dict(size=len(self.raw), sha256=hashlib.sha256(self.raw).hexdigest())

    def tearDown(self):
        self.temporary.cleanup()

    def commit(self, path, raw):
        file = self.repo / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(raw)
        subprocess.run(['git', 'add', '--', path], cwd=self.repo, check=True)
        subprocess.run(['git', '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                        'commit', '-qm', 'fixture'], cwd=self.repo, check=True)

    def test_exact_binary_recovery_and_repeat(self):
        target = self.root / 'restore'
        target.mkdir()
        self.assertEqual(materials.restore_blob(target, self.repo / '.git', 'proof/file.bin', self.oid, self.spec), 'restored')
        self.assertEqual((target / 'proof/file.bin').read_bytes(), self.raw)
        self.assertEqual(materials.restore_blob(target, self.repo / '.git', 'proof/file.bin', self.oid, self.spec), 'already present')

    def test_existing_user_file_is_preserved(self):
        (self.root / 'keep.bin').write_bytes(b'user content')
        with self.assertRaises(ValueError):
            materials.restore_blob(self.root, self.repo / '.git', 'keep.bin', self.oid, self.spec)
        self.assertEqual((self.root / 'keep.bin').read_bytes(), b'user content')

    def test_corrupt_object_contract_delivers_no_partial_file(self):
        with self.assertRaises(ValueError):
            materials.restore_blob(self.root, self.repo / '.git', 'bad.bin', self.oid, dict(self.spec, sha256='0' * 64))
        self.assertFalse((self.root / 'bad.bin').exists())
        self.assertFalse(list(self.root.glob('.material-*')))

    def test_traversal_and_absolute_paths_rejected(self):
        for name in ['../outside', 'a/../b', '/absolute', 'C:/outside', 'a\\b', 'a//b']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                materials.destination(self.root, name)

    def test_bundle_bytes_verified_before_import(self):
        file = self.root / 'bad.bundle'
        file.write_bytes(b'wrong')
        index = {'bundle': dict(bytes=5, sha256='0' * 64)}
        with self.assertRaises(ValueError):
            materials.obtain(index, self.root, file)

    def test_removed_archive_in_old_commit_still_blocks_push(self):
        path = 'dev/release-materials/sources/fixture/data.zip'
        self.commit(path, b'archive')
        subprocess.run(['git', 'rm', '-q', path], cwd=self.repo, check=True)
        subprocess.run(['git', '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                        'commit', '-qm', 'remove'], cwd=self.repo, check=True)
        errors, _ = budget.inspect(self.repo, ['HEAD'], {'approvedLargeBlobs': {}, 'fileLimitMiB': 5})
        self.assertTrue(any(path in e for e in errors))

    def test_large_new_blob_is_rejected(self):
        self.commit('source.c', b'x' * 1024)
        errors, _ = budget.inspect(self.repo, ['HEAD'], {'approvedLargeBlobs': {}, 'fileLimitMiB': 0.0001})
        self.assertTrue(any('large file' in e for e in errors))

    def test_pack_measurement_and_early_limit(self):
        self.commit('source.c', b'int main(void){return 0;}\n')
        self.assertGreater(budget.pack_size(self.repo, ['HEAD'], 1024 * 1024), 0)
        self.assertGreater(budget.pack_size(self.repo, ['HEAD'], 1), 1)

    def test_installer_chains_existing_hook_with_original_stdin(self):
        self.commit('source.c', b'int main(void){return 0;}\n')
        target = self.root / 'remote.git'
        subprocess.run(['git', 'init', '--bare', '-q', str(target)], check=True)
        source = Path(__file__).resolve().parents[1] / 'tools/hooks/pre-push'
        template = self.repo / 'tools/hooks/pre-push'
        template.parent.mkdir(parents=True)
        shutil.copy2(source, template)
        (self.repo / 'tools/git_repository_budget.py').write_text(
            'import sys\nfrom pathlib import Path\n'
            'Path("budget-stdin").write_bytes(sys.stdin.buffer.read())\n', encoding='utf8')
        hook = self.repo / '.git/hooks/pre-push'
        original = b'#!/bin/sh\ncat > previous-stdin\n'
        hook.write_bytes(original)
        hook.chmod(0o755)
        previous = installer.ROOT
        try:
            installer.ROOT = self.repo
            installer.main()
        finally:
            installer.ROOT = previous
        self.assertEqual(hook.with_name('pre-push.xlang-original').read_bytes(), original)
        subprocess.run(['git', 'push', '-q', str(target), 'HEAD:refs/heads/main'], cwd=self.repo, check=True)
        self.assertEqual((self.repo / 'budget-stdin').read_bytes(), (self.repo / 'previous-stdin').read_bytes())
        self.assertIn(b'refs/heads/main', (self.repo / 'previous-stdin').read_bytes())


if __name__ == '__main__':
    unittest.main()
