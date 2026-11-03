#!/usr/bin/env python3
"""Exercise actual terminal rendering and no-VT fallback under a Linux PTY."""
import fcntl
import functools
import http.server
import os
from pathlib import Path
import pty
import select
import struct
import subprocess
import sys
import termios
import threading
import time
sys.path.insert(0,str(Path(__file__).parent))
import check_update as fixture
def run(columns=80,term='xterm'):
    c=fixture.UpdateTests();c.setUp()
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(fixture.Quiet,directory=str(c.store)))
    threading.Thread(target=server.serve_forever,daemon=True).start()
    master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack('HHHH',24,columns,0,0))
    args=[str(fixture.EXE),'--root',str(c.install),'--public-key',c.public,'--no-launch','--url',f'http://127.0.0.1:{server.server_port}','--allow-http']
    p=subprocess.Popen(args,stdin=slave,stdout=slave,stderr=slave,env=dict(os.environ,TERM=term,LANG='C.UTF-8'));os.close(slave);data=bytearray();deadline=time.monotonic()+30
    try:
        while time.monotonic()<deadline:
            if select.select([master],[],[],.2)[0]:
                try:block=os.read(master,65536)
                except OSError:break
                if not block:break
                data.extend(block)
            elif p.poll() is not None:break
        try:p.wait(timeout=2)
        except subprocess.TimeoutExpired:
            p.kill();p.wait();(fixture.ROOT/'ide/build/Release/update/tui-failure.txt').write_bytes(data)
            raise AssertionError('TUI did not finish: '+data[-2400:].decode(errors='replace'))
        assert p.returncode==0,data.decode(errors='replace')
        text=data.decode('utf-8');assert 'xlang 更新器' in text and '更新说明' in text
        if term!='dumb':
            for label in ('更新内容','当前文件','下载总计','更新完成'):assert label in text,label
        else:assert '\x1b[' not in text
        report=fixture.ROOT/'ide/build/Release/update'/f'tui-{columns}-{term}.txt';report.write_bytes(data)
        print(f'PASS terminal {columns} columns, TERM={term}, captured={len(data)} bytes')
    finally:
        os.close(master);server.shutdown();server.server_close();c.tearDown()
if __name__=='__main__':
    run();run(50);run(80,'dumb')
