"""Observe current compiler semantics and package-variant freshness over stdio."""
from pathlib import Path
import argparse,json,hashlib,os,sys
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,checked

CHANNEL='channel<int> globalChannel\nclass Owner\n channel<int> pipe\nend\nfunc main(...) as int\n channel<int> local\n local.send(7)\n Owner object = Owner()\n return 0\nend\n'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--observe-legacy',action='store_true')
    a=p.parse_args();server=a.server.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,Path(__file__).resolve(),*[Path(__file__).with_name(n) for n in ('check_lsp_automatic_diagnostics.py','check_lsp_module_semantics.py','check_language_snapshots.py')]]}
    report=dict(status='running',scope=__doc__,protected=protected,cases=[],processes=[])
    def save():(out/'result.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    def write(path,data):
        target=path.with_name(path.name+'.new');target.write_bytes(data.encode('utf-8'));os.replace(target,path)
    def package(name,linkage,variants=None,native_link=None):
        return json.dumps(dict(format='xlang-package-1',name=name,version='1.0.0',abiVersion='1',kind='source',artifact=dict(kind='source',xlSources=['api.xl'],nativeLink=native_link or {}),native=dict(linkage=linkage,variants=variants or {}),dependencies=[],links=[],loadlibs=[]))
    client=None
    def start(name,directory,entry,code):
        nonlocal client
        client=AutomaticClient(server,directory/'absent/xl.exe',directory);row=dict(name=name,pid=client.process.pid);report['processes'].append(row)
        initialize(client,entry,[entry],directory);client.send('textDocument/didOpen',dict(textDocument=dict(uri=entry.as_uri(),languageId='xlang',version=1,text=code)));return row
    def stop(row):
        nonlocal client
        primary=sys.exc_info()[1];errors=[]
        try:stderr=client.stop()
        except BaseException as error:
            stderr='';errors.append(repr(error))
            if client.process.poll() is None:client.process.kill();client.process.wait(timeout=5)
        path=out/(row['name']+'-transcript.json');path.write_bytes((json.dumps(client.transcript,ensure_ascii=False,indent=2)+'\n').encode())
        row.update(exit=client.process.returncode,stderr=stderr,cleanupErrors=errors,transcriptSha256=sha(path));save();client=None
        if primary is None:assert row['exit']==0 and not stderr and not errors,row
    def case(name,result,expected):
        actual=result['ok'];report['cases'].append(dict(name=name,expected=expected,accepted=actual,diagnostics=result.get('diagnostics',[])));save();assert actual is expected,(name,actual,result.get('diagnostics'))
    try:
        folder=out/'channel';folder.mkdir();entry=folder/'main.xl';write(entry,'// use open text\n');row=start('channel',folder,entry,CHANNEL)
        try:
            result=checked(client,entry,1);case('channel-default-field-global-local',result,not a.observe_legacy)
            if not a.observe_legacy:
                text='channel<int> pipe\npipe.send("wrong")\n';client.send('textDocument/didChange',dict(textDocument=dict(uri=entry.as_uri(),version=2),contentChanges=[dict(text=text)]));case('channel-invalid-payload',checked(client,entry,2),False)
                client.send('textDocument/didChange',dict(textDocument=dict(uri=entry.as_uri(),version=3),contentChanges=[dict(text=CHANNEL)]));case('channel-recovery',checked(client,entry,3),True)
        finally:stop(row)
        folder=out/'variants';folder.mkdir();pkg=folder/'current_core_package';pkg.mkdir();entry=folder/'main.xl';write(entry,'// use open text\n');write(pkg/'api.xl','export func value() as int\n return 7\nend\n');base=pkg/'xlang.package.json';variant=pkg/'dynamic.json'
        write(base,package('current_core_package','static',dict(dynamic='dynamic.json')));write(variant,package('current_core_package','dynamic',native_link=dict(gcSections=True)))
        code='import current_core_package\nfunc main(...) as int\n return current_core_package.value()\nend\n';row=start('package-variants',folder,entry,code)
        try:
            case('selected-native-variant',checked(client,entry,1),True)
            write(variant,package('wrong','dynamic'));case('changed-selected-variant-same-buffer-version',checked(client,entry,1),a.observe_legacy)
            if not a.observe_legacy:
                write(variant,package('current_core_package','dynamic',native_link=dict(gcSections=True)));case('restored-selected-variant-same-buffer-version',checked(client,entry,1),True)
                write(variant,package('current_core_package','dynamic',native_link=dict(printGcSections=True)));case('invalid-native-link-requirement',checked(client,entry,1),False)
                write(variant,package('current_core_package','dynamic'));case('native-link-recovery',checked(client,entry,1),True)
                write(base,package('current_core_package','dynamic',dict(static='missing-static.json')));case('already-selected-base-does-not-follow-other-variant',checked(client,entry,1),True)
        finally:stop(row)
        assert all(sha(p)==h for p,h in protected.items());report['status']='baseline-defects-reproduced' if a.observe_legacy else 'passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),processes=len(report['processes']))))


if __name__=='__main__':main()
