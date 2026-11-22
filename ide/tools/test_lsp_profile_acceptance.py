"""Reject mixed native receipts and protect installed runtime programs."""
from pathlib import Path
import copy
import tempfile
import unittest
from build_lsp_lifecycle_profile import verify_build
from lsp_profile_inputs import digest,verify_native_runs,runtime_programs


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='lsp-acceptance-')
        self.root=Path(self.tmp.name);assert self.root.resolve().parent==Path(tempfile.gettempdir()).resolve()
        self.build={'inputs':{},'outputs':{}}
        for name in ('first.exe','second.exe','xlang-ls.exe','xlang-analysis-core.a'):
            p=self.root/name;p.write_bytes(('bytes:'+name).encode());self.build['outputs'][name]=digest(p)
        self.receipt={'status':'passed','runs':[dict(name=n,program=str(self.root/(n+'.exe')),programSha256=self.build['outputs'][n+'.exe'],exit=0)for n in ('first','second')]}
    def tearDown(self):
        assert self.root.resolve().parent==Path(tempfile.gettempdir()).resolve();self.tmp.cleanup()
    def verify(self):verify_native_runs(self.build,self.root,self.receipt)
    def override(self):
        source=self.root/'corrected.c';source.write_text('/*corrected fixture*/')
        obj=self.root/'corrected.o';obj.write_bytes(b'object')
        exe=self.root/'corrected.exe';exe.write_bytes(b'corrected executable')
        self.build['inputs'][str(source)]=digest(source)
        self.build['testOverrides']={'first':dict(program=str(exe),compile=['cc','-c',str(source),'-o',str(obj)],link=['cc',str(obj),str(self.root/'xlang-analysis-core.a'),'-o',str(exe)])}
        self.receipt['runs'][0].update(program=str(exe),programSha256=digest(exe))
        return source,obj,exe
    def test_complete_identity_bound_receipt(self):self.verify()
    def test_missing_row(self):
        self.receipt['runs'].pop()
        with self.assertRaisesRegex(ValueError,'incomplete'):self.verify()
    def test_duplicate_row(self):
        self.receipt['runs'].append(copy.deepcopy(self.receipt['runs'][0]))
        with self.assertRaisesRegex(ValueError,'duplicate'):self.verify()
    def test_unknown_program(self):
        self.receipt['runs'][0]['name']='unknown'
        with self.assertRaisesRegex(ValueError,'unknown'):self.verify()
    def test_same_bytes_at_another_path(self):
        other=self.root/'other.exe';other.write_bytes((self.root/'first.exe').read_bytes());self.receipt['runs'][0]['program']=str(other)
        with self.assertRaisesRegex(ValueError,'identity'):self.verify()
    def test_failed_execution(self):
        self.receipt['runs'][0]['exit']=1
        with self.assertRaisesRegex(ValueError,'failure'):self.verify()
    def test_boolean_is_not_an_exit_code(self):
        self.receipt['runs'][0]['exit']=False
        with self.assertRaisesRegex(ValueError,'failure'):self.verify()
    def test_changed_program(self):
        (self.root/'first.exe').write_bytes(b'new image')
        with self.assertRaisesRegex(ValueError,'identity'):self.verify()
    def test_failed_receipt(self):
        self.receipt['status']='failed'
        with self.assertRaisesRegex(ValueError,'did not pass'):self.verify()
    def test_explicit_corrected_override(self):self.override();self.verify()
    def test_override_receipt_digest_mismatch(self):
        self.override();self.receipt['runs'][0]['programSha256']='0'*64
        with self.assertRaisesRegex(ValueError,'identity'):self.verify()
    def test_override_changed_source(self):
        source,_,_=self.override();source.write_text('changed')
        with self.assertRaisesRegex(ValueError,'source/output'):self.verify()
    def test_override_wrong_core(self):
        self.override();other=self.root/'another-xlang-analysis-core.a';other.write_bytes(b'another core');self.build['testOverrides']['first']['link'][2]=str(other)
        with self.assertRaisesRegex(ValueError,'another object/core'):self.verify()
    def test_override_wrong_output(self):
        self.override();self.build['testOverrides']['first']['link'][-1]=str(self.root/'different.exe')
        with self.assertRaisesRegex(ValueError,'source/output'):self.verify()
    def test_empty_fixture_set(self):
        self.build['outputs']={'xlang-ls.exe':self.build['outputs']['xlang-ls.exe']};self.receipt['runs']=[]
        with self.assertRaisesRegex(ValueError,'no fixture'):self.verify()
    def test_build_only_requires_separate_receipt(self):
        self.build.update(status='passed',runs=[])
        with self.assertRaisesRegex(ValueError,'native runs'):verify_build(self.build,self.root)
    def test_build_and_separate_receipt_are_both_checked(self):
        import json
        self.build.update(status='passed',runs=[]);path=self.root/'acceptance.json';path.write_text(json.dumps(self.receipt))
        verify_build(self.build,self.root,path)
        (self.root/'xlang-ls.exe').write_bytes(b'changed service')
        with self.assertRaisesRegex(ValueError,'normal build output changed'):verify_build(self.build,self.root,path)


class RuntimeAliasTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='lsp-alias-')
        self.root=Path(self.tmp.name);assert self.root.resolve().parent==Path(tempfile.gettempdir()).resolve()
        self.runtime=self.root/'runtime';self.runtime.mkdir();self.source=self.root/'source.exe';self.source.write_bytes(b'MZ source')
        self.programs={name:dict(path=str(self.source),sha256=digest(self.source))for name in('normal','diagnostic')}
        self.installed=self.runtime/'xlang-ls.exe';self.installed.write_bytes(b'installed image')
    def tearDown(self):
        assert self.root.resolve().parent==Path(tempfile.gettempdir()).resolve();self.tmp.cleanup()
    def assert_installed(self):self.assertEqual(self.installed.read_bytes(),b'installed image')
    def test_exclusive_aliases_and_cleanup(self):
        with runtime_programs(self.programs,self.runtime)as selected:
            paths=[Path(p['path'])for p in selected.values()];self.assertEqual(len(set(paths)),2)
            for path in paths:self.assertEqual(path.parent,self.runtime.resolve());self.assertEqual(path.read_bytes(),self.source.read_bytes())
        self.assertTrue(all(not p.exists()for p in paths));self.assert_installed()
    def test_exception_cleans_aliases(self):
        with self.assertRaisesRegex(RuntimeError,'body'):
            with runtime_programs(self.programs,self.runtime):raise RuntimeError('body')
        self.assertEqual(list(self.runtime.iterdir()),[self.installed]);self.assert_installed()
    def test_later_source_failure_cleans_earlier_alias(self):
        self.programs['diagnostic']['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'program changed'):
            with runtime_programs(self.programs,self.runtime):self.fail('must not yield')
        self.assertEqual(list(self.runtime.iterdir()),[self.installed]);self.assert_installed()
    def test_changed_alias_is_preserved_other_alias_is_removed(self):
        with self.assertRaisesRegex(ValueError,'preserve'):
            with runtime_programs(self.programs,self.runtime)as selected:
                changed=Path(selected['diagnostic']['path']);other=Path(selected['normal']['path']);changed.write_bytes(b'changed by someone else')
        self.assertEqual(changed.read_bytes(),b'changed by someone else');self.assertFalse(other.exists());self.assert_installed()
    def test_missing_alias_still_cleans_other_alias(self):
        with self.assertRaisesRegex(ValueError,'disappeared'):
            with runtime_programs(self.programs,self.runtime)as selected:
                Path(selected['diagnostic']['path']).unlink();other=Path(selected['normal']['path'])
        self.assertFalse(other.exists());self.assert_installed()
    def test_without_runtime_preserves_original_programs(self):
        with runtime_programs(self.programs)as selected:self.assertEqual(selected['normal']['path'],str(self.source.resolve()))
        self.assertTrue(self.source.exists());self.assert_installed()
    def test_read_only_source_does_not_leave_a_read_only_windows_alias(self):
        import stat
        self.source.chmod(stat.S_IREAD)
        try:
            with runtime_programs(self.programs,self.runtime):pass
            self.assertEqual(list(self.runtime.iterdir()),[self.installed])
        finally:self.source.chmod(stat.S_IREAD|stat.S_IWRITE)


if __name__=='__main__':unittest.main()
