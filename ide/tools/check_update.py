#!/usr/bin/env python3
"""End-to-end signed-update/fault fixtures. All destructive inputs are private fixtures."""
import functools
import http.server
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'ide/tools/update'))
import update_manager as manager
import prepare_update as maker
PLATFORM=os.environ.get('UPDATE_TEST_PLATFORM','win_x64' if os.name=='nt' else 'linux_x64')
EXE=ROOT/'release'/PLATFORM/('xlang-update.exe' if os.name=='nt' else 'xlang-update')
CRYPTO=ROOT/'ide/build/Release/update'/PLATFORM/('update-crypto.exe' if os.name=='nt' else 'update-crypto')
class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self,*args): pass
class UpdateTests(unittest.TestCase):
    def setUp(self):
        base=Path(os.environ.get('UPDATE_TEST_DIR',ROOT/'ide/build/Release/update-tests'));base.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix='case-',dir=base);self.base=Path(self.temp.name)
        self.source=self.base/'source';self.source.mkdir();self.store=self.base/'repository';self.install=self.base/'安装 目录';self.install.mkdir()
        self.key=self.base/'seed';self.key.write_bytes(bytes(range(32)))
        self.public=subprocess.check_output([str(CRYPTO),'public',str(self.key)],text=True).strip()
        (self.source/'data').mkdir();(self.source/'data/commands.json').write_bytes(b'A'*24000)
        (self.source/'xl.exe').write_bytes(b'engine-v1' if os.name=='nt' else b'engine')
        (self.source/'extlibs').mkdir();(self.source/'extlibs/private').write_text('never ship')
        self.prepare(1,'init');self.server=None
    def tearDown(self):
        if self.server:self.server.shutdown();self.server.server_close();self.thread.join()
        self.temp.cleanup()
    def prepare(self,version,command='prepare'):
        args=[command,'--platform',PLATFORM,'--source',str(self.source),'--store',str(self.store),'--version',str(version),'--key',str(self.key),'--crypto',str(CRYPTO),'--notes','更新说明\n修复和改进']
        self.assertEqual(manager.main(args),0)
    def update(self,*extra,expected=0,env=None,online=False,stage=True):
        args=[str(EXE),'--root',str(self.install),'--public-key',self.public,'--no-launch','--plain']
        if stage and os.name=='nt':args.append('--staged')
        if online:
            if not self.server:
                self.server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Quiet,directory=str(self.store)))
                self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
            args+=['--url',f'http://127.0.0.1:{self.server.server_port}','--allow-http']
        else:args+=['--offline',str(self.store)]
        result=subprocess.run([*args,*extra],capture_output=True,timeout=60,env=env)
        self.assertEqual(result.returncode,expected,result.stdout.decode(errors='replace')+'\n'+result.stderr.decode(errors='replace'))
        return result
    def resign(self,fn):
        value=manager.payload(self.store/'update.json');fn(value);data=manager.signed(value,self.key,str(CRYPTO))
        (self.store/'update.json').write_bytes(data);(self.store/'update.lzma').write_bytes(maker.lzma_alone(data))
    def test_bootstrap_repair_and_user_state(self):
        for name in ('extlibs/private','option/settings.json','projects/user.xl','mdo-home/settings'):
            p=self.install/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'USER')
        self.update();self.assertEqual((self.install/'data/commands.json').read_bytes(),b'A'*24000)
        self.update('--check');(self.install/'xl.exe').write_bytes(b'broken');self.update('--check',expected=2);self.update()
        for name in ('extlibs/private','option/settings.json','projects/user.xl','mdo-home/settings'):self.assertEqual((self.install/name).read_bytes(),b'USER')
    def test_http_streaming(self):self.update(online=True)
    def test_self_staging(self):self.update(stage=False)
    @unittest.skipUnless(os.name=='nt','Windows running executable self-replacement')
    def test_running_updater_replacement(self):
        original=EXE.read_bytes();installed=self.install/'xlang-update.exe';installed.write_bytes(original+b'OLD')
        (self.source/'xlang-update.exe').write_bytes(original+b'NEW');self.prepare(2)
        r=subprocess.run([str(installed),'--root',str(self.install),'--public-key',self.public,'--offline',str(self.store),'--plain','--no-launch'],capture_output=True,timeout=60)
        self.assertEqual(r.returncode,0,r.stderr.decode(errors='replace'))
        # Default Windows bootstrap releases its original process. Inherited
        # output handles keep communicate() attached until the staged worker exits.
        self.assertEqual(installed.read_bytes(),original+b'NEW')
        self.assertFalse((self.install/'.xlang-update/txn').exists())
        import time
        deadline=time.monotonic()+5
        while True:
            try:installed.unlink();break
            except PermissionError:
                if time.monotonic()>deadline:raise
                time.sleep(.05)
    @unittest.skipUnless(os.name=='nt','Windows extended-length path')
    def test_long_install_path(self):
        p=self.install/('目录'*45)/('nested'*18)
        # Extended prefix also permits running this fixture when the host's
        # global long-path policy is disabled.
        p=Path('\\\\?\\'+str(p.resolve()));p.mkdir(parents=True);self.install=p
        try:self.update();self.assertEqual((p/'data/commands.json').read_bytes(),b'A'*24000)
        finally:
            absolute=str(p.resolve()).removeprefix('\\\\?\\')
            workspace=str(self.base.resolve()).removeprefix('\\\\?\\')
            self.assertEqual(os.path.commonpath([absolute,workspace]),workspace)
            shutil.rmtree(str(p))
    def test_tampered_signature(self):
        env=json.loads((self.store/'update.json').read_text(encoding='utf-8'));env['payload']=env['payload'].replace('修复','篡改')
        (self.store/'update.lzma').write_bytes(maker.lzma_alone(json.dumps(env,ensure_ascii=False).encode()));self.update(expected=1)
        self.assertFalse((self.install/'xl.exe').exists())
    def test_wrong_key(self):self.update('--public-key','00'*32,expected=1)
    def test_key_rotation(self):
        nextkey=self.base/'next-key';nextkey.write_bytes(bytes(reversed(range(32))))
        public=subprocess.check_output([str(CRYPTO),'public',str(nextkey)],text=True).strip()
        data=manager.signed(manager.payload(self.store/'update.json'),self.key,str(CRYPTO),[nextkey])
        (self.store/'update.lzma').write_bytes(maker.lzma_alone(data));self.update('--public-key',public)
    def test_platform_mismatch(self):self.resign(lambda m:m.update(platform='wrong'));self.update(expected=1)
    def test_protected_file(self):self.resign(lambda m:m['files'][0].update(path='extlibs/private'));self.update(expected=1)
    def test_path_traversal(self):self.resign(lambda m:m['files'][0].update(path='../escape'));self.update(expected=1)
    def test_duplicate_path(self):self.resign(lambda m:m['files'].append(m['files'][0]));self.update(expected=1)
    def test_corrupt_object_before_commit(self):
        entry=manager.payload(self.store/'update.json')['files'][0];(self.store/'update'/(entry['sha256']+'.lzma')).write_bytes(b'corrupt')
        self.update(expected=1);self.assertFalse((self.install/'xl.exe').exists())
    def test_incremental_delete_and_discard(self):
        self.update();(self.source/'data/commands.json').unlink();(self.source/'xl.exe').write_bytes(b'engine-v2');self.prepare(2)
        self.update();self.assertFalse((self.install/'data/commands.json').exists());self.assertEqual((self.install/'xl.exe').read_bytes(),b'engine-v2')
        self.assertEqual(manager.main(['discard','--store',str(self.store),'--platform',PLATFORM]),0)
        self.assertEqual(manager.current(self.store),1)
    def test_crash_recovery(self):
        self.update();(self.source/'data/commands.json').write_bytes(b'B'*23000);(self.source/'xl.exe').write_bytes(b'new');self.prepare(2)
        env=dict(os.environ,XLANG_UPDATE_TEST_CRASH_AFTER='1');self.update(expected=86,env=env)
        self.update('--check',expected=1);self.update('--recover-only');self.assertEqual((self.install/'data/commands.json').read_bytes(),b'A'*24000)
        self.update();self.assertEqual((self.install/'xl.exe').read_bytes(),b'new')
    def test_recovery_preserves_external_modification(self):
        self.update();(self.source/'data/commands.json').write_bytes(b'B'*23000);self.prepare(2)
        self.update(expected=86,env=dict(os.environ,XLANG_UPDATE_TEST_CRASH_AFTER='1'))
        journal=self.install/'.xlang-update/txn/journal.json'
        state=json.loads(journal.read_text(encoding='utf-8'));op=state['ops'][0]
        target=self.install/op['path'];target.write_bytes(b'EXTERNAL CHANGE')
        self.update('--recover-only',expected=1)
        self.assertEqual(target.read_bytes(),b'EXTERNAL CHANGE');self.assertTrue(journal.exists())
        # Once the conflict is explicitly resolved, the preserved backup can recover.
        target.unlink();self.update('--recover-only')
        self.assertEqual((self.install/'data/commands.json').read_bytes(),b'A'*24000)
    def test_multipart(self):
        previous=maker.OBJECT_PART_MAX_BYTES;maker.OBJECT_PART_MAX_BYTES=1024
        try:(self.source/'xl.exe').write_bytes(os.urandom(6500));self.prepare(2)
        finally:maker.OBJECT_PART_MAX_BYTES=previous
        m=manager.payload(self.store/'update.json');self.assertTrue(any('archiveParts' in f for f in m['files']));self.update()
    def test_version_manifest_race(self):
        (self.store/'version.txt').write_text('2\n');self.update(expected=1);self.assertFalse((self.install/'xl.exe').exists())
    @unittest.skipIf(os.name=='nt','Unix executable permission repair')
    def test_executable_permission_repair(self):
        self.update();p=self.install/'xl.exe';p.chmod(0o644)
        if p.stat().st_mode&0o111:self.skipTest('filesystem does not enforce Unix modes')
        self.update('--check',expected=2);self.update();self.assertTrue(p.stat().st_mode&0o111)
    def test_protected_deletion(self):
        self.resign(lambda m:m['delete'].append('option/settings.json'));self.update(expected=1)
    def test_updater_cannot_be_deleted(self):
        self.resign(lambda m:m['delete'].append('xlang-update.exe'));self.update(expected=1)
    @unittest.skipUnless(os.name=='nt','Windows sharing violation fixture')
    def test_locked_file_rolls_back(self):
        import ctypes
        self.update();(self.source/'data/commands.json').write_bytes(b'B'*20000);(self.source/'xl.exe').write_bytes(b'new');self.prepare(2)
        dll=ctypes.WinDLL('kernel32',use_last_error=True);dll.CreateFileW.restype=ctypes.c_void_p;dll.CloseHandle.argtypes=[ctypes.c_void_p]
        handle=dll.CreateFileW(str(self.install/'xl.exe'),0x80000000,0,None,3,0,None)
        self.assertNotEqual(handle,ctypes.c_void_p(-1).value)
        try:self.update(expected=1)
        finally:dll.CloseHandle(handle)
        self.assertEqual((self.install/'data/commands.json').read_bytes(),b'A'*24000);self.assertEqual((self.install/'xl.exe').read_bytes(),b'engine-v1')
    @unittest.skipUnless(os.name=='nt','Windows directory junction fixture')
    def test_windows_junction_escape(self):
        outside=self.base/'outside';outside.mkdir()
        result=subprocess.run(['cmd','/c','mklink','/J',str(self.install/'data'),str(outside)],capture_output=True)
        if result.returncode:self.skipTest('junction not supported by test filesystem')
        self.update(expected=1);self.assertEqual(list(outside.iterdir()),[])
    @unittest.skipIf(os.name=='nt','symlinks require extra Windows privileges; junction case is separately checked')
    def test_symlink_escape(self):
        outside=self.base/'outside';outside.mkdir();(self.install/'data').symlink_to(outside,target_is_directory=True)
        self.update(expected=1);self.assertEqual(list(outside.iterdir()),[])
if __name__=='__main__':unittest.main(verbosity=2)
