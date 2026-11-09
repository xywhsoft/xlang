"""Object recipes must identify actual linked/archive bytes,not proposals."""
from pathlib import Path
import tempfile
import unittest
from lsp_profile_inputs import select_units,digest


class ProfileInputTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='lsp-profile-inputs-')
        self.root=Path(self.tmp.name);assert self.root.resolve().parent==Path(tempfile.gettempdir()).resolve()
        self.source=self.root/'中文 source';self.source.mkdir()
        self.core=self.root/'core';self.core.mkdir();self.host=self.root/'hash-prefix';self.host.mkdir()
        for name,folder in [('check',self.core),('language_snapshot',self.host)]:
            (self.source/(name+'.c')).write_text('/*fixture*/',encoding='utf8')
            (folder/(('f117-' if folder==self.host else '')+name+'.o')).write_bytes(b'COFF\x00\n\r\n'+name.encode())
        self.check=self.core/'check.o';self.snapshot=self.host/'f117-language_snapshot.o'
        self.check_cmd=['cc','-c',str(self.source/'check.c'),'-o',str(self.check)]
        self.snapshot_cmd=['cc','-c',str(self.source/'language_snapshot.c'),'-o',str(self.snapshot)]
        self.link=['cc',str(self.snapshot),'core.a','-o','xlang-ls.exe']
        self.members={'check.o':digest(self.check)}
        self.object_hashes={str(self.snapshot):digest(self.snapshot)}
    def tearDown(self):
        assert self.root.resolve().parent==Path(tempfile.gettempdir()).resolve();self.tmp.cleanup()
    def select(self,commands,names=('check','language_snapshot')):
        return select_units({'commands':commands,'objectOutputs':self.object_hashes},self.link,self.members,names)
    def test_actual_hash_prefixed_host_and_core(self):
        self.assertEqual(self.select([self.check_cmd,self.snapshot_cmd]),{'check':self.check_cmd,'language_snapshot':self.snapshot_cmd})
    def test_missing_proposed_core_output_does_not_shadow_actual(self):
        bad=self.check_cmd[:-1]+[str(self.root/'missing'/'check.o')]
        self.assertEqual(self.select([self.check_cmd,bad],('check',))['check'],self.check_cmd)
    def test_wrong_archive_bytes_do_not_shadow_actual(self):
        folder=self.root/'wrong';folder.mkdir();obj=folder/'check.o';obj.write_bytes(b'wrong')
        bad=self.check_cmd[:-1]+[str(obj)]
        self.assertEqual(self.select([self.check_cmd,bad],('check',))['check'],self.check_cmd)
    def test_unlinked_same_name_host_does_not_shadow_actual(self):
        folder=self.root/'unlinked';folder.mkdir();obj=folder/self.snapshot.name;obj.write_bytes(self.snapshot.read_bytes())
        bad=self.snapshot_cmd[:-1]+[str(obj)]
        self.assertEqual(self.select([self.snapshot_cmd,bad],('language_snapshot',))['language_snapshot'],self.snapshot_cmd)
    def test_canonical_link_alias(self):
        alias=self.host/'nested';alias.mkdir();self.link[1]=str(alias/'..'/self.snapshot.name)
        self.assertEqual(self.select([self.snapshot_cmd],('language_snapshot',))['language_snapshot'],self.snapshot_cmd)
    def test_corrupted_core_output_is_rejected(self):
        self.check.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'archived.*check'):self.select([self.check_cmd],('check',))
    def test_proposal_only_host_is_rejected(self):
        bad=self.snapshot_cmd[:-1]+[str(self.root/self.snapshot.name)]
        with self.assertRaisesRegex(ValueError,'linked.*language_snapshot'):self.select([bad],('language_snapshot',))
    def test_missing_name_rejected(self):
        with self.assertRaisesRegex(ValueError,'archived.*absent'):self.select([self.check_cmd],('absent',))
    def test_corrupted_linked_host_is_rejected(self):
        self.snapshot.write_bytes(b'changed host')
        with self.assertRaisesRegex(ValueError,'Linked object bytes changed'):self.select([self.snapshot_cmd],('language_snapshot',))
    def test_missing_linked_identity_is_rejected(self):
        self.object_hashes.clear()
        with self.assertRaisesRegex(ValueError,'Missing linked object identity'):self.select([self.snapshot_cmd],('language_snapshot',))
    def test_conflicting_linked_identities_are_rejected(self):
        with self.assertRaisesRegex(ValueError,'Conflicting linked object identities'):
            select_units({'commands':[self.snapshot_cmd],'objectOutputs':self.object_hashes,
                          'dependencyAudit':[{'actualObject':str(self.snapshot),'actualObjectSha256':'0'*64}]},
                         self.link,self.members,('language_snapshot',))

if __name__=='__main__':unittest.main()
