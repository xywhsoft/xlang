#!/usr/bin/env python3
"""Real OpenSSH transport + temporary SSH server, shell filesystem and HTTP activation."""
import functools
import hashlib
import http.server
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import threading
import unittest
import paramiko
sys.path.insert(0,str(Path(__file__).parent))
import check_update as fixture
manager=fixture.manager
class SSHServer(paramiko.ServerInterface):
    def __init__(self,key,shell):self.key=key;self.shell=shell
    def get_allowed_auths(self,user):return 'publickey'
    def check_auth_publickey(self,user,key):return paramiko.AUTH_SUCCESSFUL if key==self.key else paramiko.AUTH_FAILED
    def check_channel_request(self,kind,chanid):return paramiko.OPEN_SUCCEEDED if kind=='session' else paramiko.OPEN_FAILED_ADMINISTRATIVELY_PROHIBITED
    def check_channel_exec_request(self,channel,command):
        def execute():
            p=subprocess.Popen([self.shell,'--noprofile','--norc','-c',command.decode()],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            def stdin():
                try:
                    while data:=channel.recv(65536):p.stdin.write(data);p.stdin.flush()
                except (OSError,EOFError):pass
                finally:
                    try:p.stdin.close()
                    except OSError:pass
            def output(pipe,stderr=False):
                try:
                    while data:=pipe.read(32768):
                        if stderr:channel.send_stderr(data)
                        else:channel.sendall(data)
                except (OSError,EOFError):pass
            threading.Thread(target=stdin,daemon=True).start()
            threads=[threading.Thread(target=output,args=(p.stdout,)),threading.Thread(target=output,args=(p.stderr,True))]
            for t in threads:t.start()
            result=p.wait()
            for t in threads:t.join()
            p.stdout.close();p.stderr.close()
            channel.send_exit_status(result);channel.close()
        threading.Thread(target=execute,daemon=True).start();return True
class Public(fixture.Quiet):
    break_metadata=False
    break_version=False
    def do_GET(self):
        if self.break_version and self.path.startswith('/version.txt'):
            self.send_response(200);self.end_headers();self.wfile.write(b'0\n');return
        if self.break_metadata and self.path.startswith('/update.json'):
            self.send_response(200);self.end_headers();self.wfile.write(b'broken');return
        super().do_GET()
class PublishTests(unittest.TestCase):
    def test_publish_resume_immutable_and_rollback(self):
        case=fixture.UpdateTests();case.setUp();server=None;sock=None;transports=[];stop=threading.Event()
        try:
            shell=str(Path('C:/Program Files/Git/usr/bin/bash.exe')) if os.name=='nt' else '/bin/bash'
            if not Path(shell).is_file():self.skipTest('bash required on publishing test machine')
            key=paramiko.RSAKey.generate(2048);keypath=case.base/'ssh-key';key.write_private_key_file(str(keypath))
            if os.name!='nt':keypath.chmod(0o600)
            hostkey=paramiko.RSAKey.generate(2048);sock=socket.socket();sock.bind(('127.0.0.1',0));sock.listen();sock.settimeout(.2);port=sock.getsockname()[1]
            def accept():
                while not stop.is_set():
                    try:client,_=sock.accept()
                    except socket.timeout:continue
                    except OSError:return
                    transport=paramiko.Transport(client);transports.append(transport);transport.add_server_key(hostkey)
                    try:transport.start_server(server=SSHServer(key,shell))
                    except paramiko.SSHException:transport.close()
            thread=threading.Thread(target=accept,daemon=True);thread.start()
            known=case.base/'known_hosts';known.write_text(f'[127.0.0.1]:{port} {hostkey.get_name()} {hostkey.get_base64()}\n')
            config=case.base/'ssh-config';config.write_text(f'Host fixture-update\n HostName 127.0.0.1\n Port {port}\n User test\n IdentityFile "{keypath.as_posix()}"\n UserKnownHostsFile "{known.as_posix()}"\n StrictHostKeyChecking yes\n IdentitiesOnly yes\n',encoding='utf-8')
            remote=case.base/'remote'/('update_'+fixture.PLATFORM);remote.mkdir(parents=True)
            server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Public,directory=str(remote)))
            httpthread=threading.Thread(target=server.serve_forever,daemon=True);httpthread.start()
            remote_path=remote.as_posix()
            if os.name=='nt':remote_path='/'+remote_path[0].lower()+remote_path[2:]
            args=['--store',str(case.store),'--platform',fixture.PLATFORM]
            self.assertEqual(manager.main(['configure',*args,'--host','fixture-update','--remote-root',remote_path,'--public-url',f'http://127.0.0.1:{server.server_port}','--ssh-config',str(config)]),0)
            # Real OpenSSH with enough long object paths to exceed legacy
            # command limits if each absolute path is repeated in one chmod.
            transport=manager.SSH(manager.read(case.store/'.state/deploy.json'))
            permission_dir=remote/'permission-fixture';permission_dir.mkdir()
            expected={}
            for i in range(110):
                data=str(i).encode();name=hashlib.sha256(data).hexdigest()+'.lzma'
                (permission_dir/name).write_bytes(data);expected[name]=hashlib.sha256(data).hexdigest()
            transport.set_read_permissions(remote_path+'/permission-fixture',sorted(expected))
            self.assertEqual(transport.hashes(remote_path+'/permission-fixture',sorted(expected)),expected)
            with self.assertRaises(ValueError):transport.run('printf '+('x'*7000))
            self.assertEqual(manager.main(['push',*args,'--dry-run']),0)
            self.assertFalse((remote/'version.txt').exists())
            self.assertEqual(manager.main(['push',*args]),0)
            self.assertEqual((remote/'version.txt').read_text().strip(),'1')
            # Missing remote objects must be healed even on a same-version rerun.
            removed=next((remote/'update').glob('*.lzma'));removed.unlink()
            self.assertEqual(manager.main(['push',*args]),0);self.assertTrue(removed.exists())
            first={name:(remote/name).read_bytes() for name in manager.META}
            (case.source/'xl.exe').write_bytes(b'engine-v2');case.prepare(2)
            Public.break_metadata=True
            self.assertEqual(manager.main(['push',*args]),1)
            for name,data in first.items():self.assertEqual((remote/name).read_bytes(),data)
            Public.break_metadata=False
            self.assertEqual(manager.main(['push',*args]),0)
            self.assertEqual((remote/'version.txt').read_text().strip(),'2')
            self.assertTrue(manager.read(case.store/'.state/history/2/record.json')['published'])
            self.assertFalse((case.store/'.state/push.json').exists())
            (case.source/'xl.exe').write_bytes(b'engine-v3');case.prepare(3)
            Public.break_version=True
            self.assertEqual(manager.main(['push',*args]),1)
            self.assertEqual((remote/'version.txt').read_text().strip(),'3')
            self.assertTrue((case.store/'.state/push.json').exists())
            Public.break_version=False
            self.assertEqual(manager.main(['push',*args]),0)
        finally:
            Public.break_metadata=False;Public.break_version=False;stop.set()
            if sock:sock.close()
            for transport in transports:transport.close()
            if server:server.shutdown();server.server_close()
            case.tearDown()
if __name__=='__main__':unittest.main(verbosity=2)
