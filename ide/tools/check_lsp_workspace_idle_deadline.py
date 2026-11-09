"""Gate a real native check to verify quiet time starts after foreground work.

Builds two private stdio probes from an intact native receipt. The first adds
only a named-event gate to the real worker; the second also restores the old
server deadline predicate. Neither probe is a distributable language server.
"""
from pathlib import Path
import argparse
import ctypes as C
import json
import os
import shutil
import subprocess
import time
import uuid
from build_lsp_lifecycle_profile import sha, verify_build
from build_lsp_core import dependencies
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path.cwd(); build = args.build.resolve(strict=True)
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=False)
    b = json.loads((build/'build-result.json').read_text(encoding='utf-8-sig')); verify_build(b, build)
    commands, cases = [], []
    report = dict(status='failed', scope=__doc__, buildReceipt=str(build/'build-result.json'),
                  buildReceiptSha256=sha(build/'build-result.json'), commands=commands, cases=cases)
    def run(name, command):
        result = subprocess.run(command, capture_output=True, cwd=output, timeout=240)
        (output/(name+'.log')).write_bytes(result.stdout+result.stderr)
        commands.append(dict(name=name, command=command, exit=result.returncode))
        (output/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        assert not result.returncode, (name, result.stderr.decode(errors='replace')[-2500:])
        return result
    def compile_unit(name, template, source):
        command = template.copy(); command[command.index('-c')+1] = str(source)
        command[command.index('-o')+1] = str(output/(name+'.o'))
        dep = command.copy(); at = dep.index('-o'); del dep[at:at+2]
        at = dep.index('-c'); dep[at:at+2] = ['-MM', '-MT', 'probeObject', str(source)]
        result = run(name+'-deps', dep)
        closure = {str(p.resolve()): sha(p) for p in dependencies(result.stdout.decode())}
        assert str(source.resolve()) in closure
        run(name+'-compile', command)
        return command[command.index('-o')+1], closure
    worker = root/'ide/src/analysis/worker.c'
    worker_command = next(c.copy() for c in reversed(b['commands']) if '-c' in c and Path(c[c.index('-c')+1]) == worker)
    worker_source = output/'worker_probe.c'
    body = worker.read_text(encoding='utf-8'); needle = 'if(input->kind>=QUERY_CHECK && input->kind<QUERY_COUNT) {'
    assert body.count(needle) == 1
    prefix = '''#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
static void xideIdleDeadlineProbe(void) {
 static LONG used; char name[256],entered[288];
 DWORD n=GetEnvironmentVariableA("XIDE_LSP_IDLE_GATE",name,sizeof(name));
 if(!n || n>=sizeof(name) || InterlockedCompareExchange(&used,1,0))return;
 snprintf(entered,sizeof(entered),"%s-entered",name);
 HANDLE gate=OpenEventA(SYNCHRONIZE,FALSE,name),signal=OpenEventA(EVENT_MODIFY_STATE,FALSE,entered);
 if(gate && signal){SetEvent(signal);WaitForSingleObject(gate,15000);}
 if(gate)CloseHandle(gate);
 if(signal)CloseHandle(signal);
}
'''
    worker_source.write_text(prefix+body.replace(needle, needle+'\n        if(input->kind==QUERY_CHECK)xideIdleDeadlineProbe();', 1), encoding='utf-8')
    worker_object, worker_deps = compile_unit('worker_probe', worker_command, worker_source)
    link = next(c.copy() for c in reversed(b['commands']) if '-c' not in c and '-o' in c and Path(c[c.index('-o')+1]).name == 'xlang-ls.exe')
    link.insert(1, worker_object)  # Satisfy worker symbols before the core archive.
    server = root/'ide/src/server/xlang_ls.c'
    server_object = next(v for v in link if v.endswith('xlang_ls.o'))
    server_command = next(c.copy() for c in reversed(b['commands']) if '-c' in c and c[c.index('-o')+1] == server_object)
    mutation = output/'old_deadline'; mutation.mkdir()
    server_source = mutation/'xlang_ls.c'; shutil.copy2(server, server_source)
    include = root/'ide/src/server/workspace_symbol_refresh.inc'
    private_server = mutation/'server'; private_server.mkdir()
    shutil.copy2(root/'ide/src/server/workspace_symbols.inc', private_server/'workspace_symbols.inc')
    text = include.read_text(encoding='utf-8')
    first = text.index(' if(!s->workspaceRefresh.pending)return;')
    last = text.index(' xls_context* contexts[64]', first)
    text = text[:first]+' if(!s->workspaceRefresh.pending || GetTickCount64()<s->workspaceRefresh.due || workspaceRefreshBusy(s))return;\n'+text[last:]
    (private_server/include.name).write_text(text, encoding='utf-8')
    server_command.insert(1, '-I'+str(server.parent))
    mutant_object, mutant_deps = compile_unit('old_deadline_server', server_command, server_source)
    assert str((private_server/include.name).resolve()) in mutant_deps and str(include.resolve()) not in mutant_deps
    programs = {}
    for mode in ('current', 'old-deadline'):
        program = output/(mode+'.exe'); command = link.copy()
        if mode == 'old-deadline': command = [mutant_object if v == server_object else v for v in command]
        command[command.index('-o')+1] = str(program); run(mode+'-link', command); programs[mode] = program
    report['dependencies'] = dict(worker=worker_deps, oldDeadlineServer=mutant_deps)
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.CreateEventW.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_wchar_p]; kernel.CreateEventW.restype = C.c_void_p
    kernel.WaitForSingleObject.argtypes = [C.c_void_p, C.c_uint32]; kernel.WaitForSingleObject.restype = C.c_uint32
    kernel.SetEvent.argtypes = [C.c_void_p]; kernel.SetEvent.restype = C.c_int
    kernel.CloseHandle.argtypes = [C.c_void_p]; kernel.CloseHandle.restype = C.c_int
    try:
        for mode, program in programs.items():
            directory = output/mode; directory.mkdir(); entry = directory/'main.xl'
            body = 'func main(...) as int\n return 1\nend\n'; entry.write_text(body, encoding='utf-8')
            token = 'Local\\xlang-idle-'+uuid.uuid4().hex
            gate = kernel.CreateEventW(None, 1, 0, token); entered = kernel.CreateEventW(None, 1, 0, token+'-entered'); assert gate and entered
            alias = root/'release/win_x64'/('.lsp-idle-probe-'+mode+'.exe'); assert not alias.exists(); shutil.copy2(program, alias); digest = sha(alias)
            old_env = os.environ.get('XIDE_LSP_IDLE_GATE'); os.environ['XIDE_LSP_IDLE_GATE'] = token
            client = None
            try:
                client = AutomaticClient(alias, output/'absent/xl.exe', directory)
                original = client.send
                def send(method, *a, **kw):
                    if method != 'initialized': return original(method, *a, **kw)
                client.send = send; initialize(client, entry, [entry], directory); client.send = original
                client.open(entry, body, 1); client.request += 1; request = client.request
                client.send('xlang/check', dict(textDocument=dict(uri=entry.as_uri()), version=1), request)
                assert kernel.WaitForSingleObject(entered, 30000) == 0
                client.send('initialized', {}); time.sleep(.75)
                before = client.call('xlang/workspaceSymbolStats')['background']; assert not before['started']
                assert kernel.SetEvent(gate); reply, _ = client.reply(request); assert reply['result']['ok'], reply
                time.sleep(.15); early = client.call('xlang/workspaceSymbolStats')['background']
                assert (early['started'] == 0) if mode == 'current' else (early['started'] > 0), early
                until = time.monotonic()+15
                while True:
                    after = client.call('xlang/workspaceSymbolStats')['background']; assert not after['failed'], after
                    if after['completed'] >= 1 and not after['running']: break
                    assert time.monotonic() < until, after; time.sleep(.02)
                assert len(client.call('workspace/symbol', dict(query='main'))) == 1
                cases.append(dict(mode=mode, passed=True, before=before, early=early, after=after, programSha256=digest))
            finally:
                kernel.SetEvent(gate)
                if client:
                    stderr = client.stop(); (directory/'transcript.json').write_text(json.dumps(client.transcript, indent=2)+'\n', encoding='utf-8')
                    (directory/'stderr.log').write_text(stderr, encoding='utf-8'); assert client.process.returncode == 0 and not stderr
                if old_env is None: os.environ.pop('XIDE_LSP_IDLE_GATE', None)
                else: os.environ['XIDE_LSP_IDLE_GATE'] = old_env
                kernel.CloseHandle(gate); kernel.CloseHandle(entered); assert sha(alias) == digest; alias.unlink()
        verify_build(b, build); report['status'] = 'passed'
    finally:
        (output/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('Idle deadline passed: native check held beyond delay,current quiet window preserved,actual old C predicate rejected,eventual automatic refresh and current symbols.')


if __name__ == '__main__': main()
