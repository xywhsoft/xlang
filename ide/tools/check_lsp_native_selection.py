"""Real configured checks/completions and native-variant identity invalidation."""
from pathlib import Path
import argparse,hashlib,json,os,sys,time
from check_language_snapshots import Client
from check_language_definitions import position
from check_lsp_analysis_configuration import LISTS,PROFILE
from check_lsp_automatic_diagnostics import AutomaticClient

CODE='import mode_package\nfunc main(...) as int\n return mode_package.value()\nend\n'


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--observe-legacy',action='store_true');a=p.parse_args()
    server=a.server.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    protected={str(path):sha(path) for path in (server,Path(__file__).resolve(),*[Path(__file__).with_name(name) for name in ('check_language_snapshots.py','check_lsp_analysis_configuration.py','check_lsp_automatic_diagnostics.py','check_language_definitions.py')])}
    entry=out/'main.xl';entry.write_bytes(b'// open overlay is authoritative\n');pkg=out/'mode_package';pkg.mkdir();(pkg/'api.xl').write_bytes(b'export func value() as int\n return 7\nend\n');base=pkg/'xlang.package.json';variant=pkg/'dynamic.json'
    report=dict(status='running',scope=__doc__,protected=protected,cases=[],processes=[]);client=None;rid=20
    def save():(out/'result.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    def write(path,text):
        temporary=path.with_name(path.name+'.new');temporary.write_bytes(text.encode());os.replace(temporary,path)
    def manifest(name,linkage=None,variants=None):
        result=dict(format='xlang-package-1',name=name,version='1.0.0',abiVersion='1',kind='source',artifact=dict(kind='source',xlSources=['api.xl']),dependencies=[],links=[],loadlibs=[])
        if linkage is not None:result['native']=dict(linkage=linkage,variants=variants or {})
        return json.dumps(result)
    write(base,manifest('mode_package','static',dict(dynamic='dynamic.json')));write(variant,manifest('mode_package','dynamic'))
    def packet(epoch,mode=None):
        lists={key:[] for key in LISTS};lists['members']=[entry.as_posix()]
        context=dict(schema=1 if mode is None else 2,kind='project',project=(out/'project.xlp').as_posix(),configuration='debug',entry=entry.as_posix(),directory=out.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath='',lists=lists)
        if mode is not None:context['nativeSelection']=mode
        return dict(epoch=epoch,catalog=dict(schema=1,projects=[context],selectedProject=context['project'],packagePath=''))
    def call(method,params,error=None):
        nonlocal rid
        until=time.monotonic()+20
        while True:
            rid+=1;client.send(method,params,rid);reply,_=client.reply(rid)
            if error is None and reply.get('error',{}).get('code')==-32001:
                assert time.monotonic()<until,reply;time.sleep(.002);continue
            break
        if error is not None:assert reply.get('error',{}).get('code')==error,reply;return reply['error']
        assert 'error' not in reply,reply;return reply['result']
    def done(name,**fields):report['cases'].append(dict(name=name,**fields));save()
    def check(epoch,accepted,dynamic=None):
        value=call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=7));assert value['ok'] is accepted and value['version']==7 and value['analysisContext']['epoch']==epoch,value
        if dynamic is not None and accepted:
            paths={Path(item['path']).name for item in value['documents']};assert ('dynamic.json' in paths)==dynamic,paths
        return value
    def cursor(epoch,available=True):
        offset=CODE.index('mode_package.value')+len('mode_package.val')
        value=call('textDocument/completion',dict(textDocument=dict(uri=entry.as_uri()),version=7,position=position(CODE,offset)))
        assert value['available'] is available and value['analysisContext']['epoch']==epoch,value
        labels={item['label'] for item in value['items']};assert ('value' in labels)==available,value
        return value
    def close(name):
        nonlocal client
        if not client:return
        primary=sys.exc_info()[1];errors=[]
        try:stderr=client.stop()
        except BaseException as error:
            stderr='';errors.append(repr(error))
            if client.process.poll() is None:client.process.kill();client.process.wait(timeout=5)
        transcript=out/(name+'.json');transcript.write_bytes((json.dumps(client.transcript,ensure_ascii=False,indent=2)+'\n').encode());row.update(exit=client.process.returncode,stderr=stderr,cleanupErrors=errors,transcriptSha256=sha(transcript));client=None;save()
        if primary is None and (row['exit']!=0 or stderr or errors):
            report.update(status='failed',error='Client shutdown or stderr validation failed');save();raise AssertionError(row)
    try:
        client=Client(server,out/'absent/xl.exe',out);row=dict(pid=client.process.pid);report['processes'].append(row)
        client.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False)),1);reply,_=client.reply(1);caps=reply['result']['capabilities']['experimental'];client.send('initialized',{})
        if a.observe_legacy:
            assert not caps.get('nativePackageSelection',False),caps;call('xlang/configureAnalysis',packet(1,'static'),-32602);done('legacy-context-cannot-express-static-selection');report['status']='baseline-gap-reproduced'
        else:
            assert caps['nativePackageSelection'] is True,caps;client.open(entry,CODE,7);epoch=0;pid=client.process.pid
            for mode,dynamic in [('static',False),('dynamic',True),('auto-aot',False),('auto-jit',True),(None,True)]:
                epoch+=1;configured=call('xlang/configureAnalysis',packet(epoch,mode));assert configured['epoch']==epoch
                value=check(epoch,True,dynamic);cursor(epoch);done('selection-'+str(mode),epoch=epoch,builds=value['analysis']['builds'])
            write(variant,manifest('wrong_package','dynamic'))
            for mode,accepted in [('static',True),('dynamic',False),('auto-aot',True),('auto-jit',False)]:
                epoch+=1;call('xlang/configureAnalysis',packet(epoch,mode));value=check(epoch,accepted,False if accepted else None);cursor(epoch,accepted);done('damaged-variant-'+mode,diagnostics=value['count'])
            # Validation must occur even without an epoch or document change.
            write(variant,manifest('mode_package','dynamic'));check(epoch,True,True);cursor(epoch);done('same-context-version-disk-recovery')
            write(variant,manifest('wrong_package','dynamic'));check(epoch,False);cursor(epoch,False);done('same-context-version-disk-invalidation')
            write(variant,manifest('mode_package','dynamic'));check(epoch,True,True);cursor(epoch);done('second-disk-recovery')
            for invalid in ('STATIC','auto',' static',None,2):
                bad=packet(epoch+1,'static');bad['catalog']['projects'][0]['nativeSelection']=invalid;call('xlang/configureAnalysis',bad,-32602);check(epoch,True,True)
            bad=packet(epoch+1,'static');del bad['catalog']['projects'][0]['nativeSelection'];call('xlang/configureAnalysis',bad,-32602);check(epoch,True,True)
            bad=packet(epoch+1,'static');bad['catalog']['projects'][0]['schema']=1;call('xlang/configureAnalysis',bad,-32602);check(epoch,True,True)
            done('seven-invalid-contexts-preserve-current-configuration')
            write(base,manifest('mode_package','dynamic',dict(static='missing-static.json')))
            for mode,accepted in [('dynamic',True),('static',False),('auto-jit',True),('auto-aot',False)]:
                epoch+=1;call('xlang/configureAnalysis',packet(epoch,mode));check(epoch,accepted);done('declared-missing-alternative-'+mode)
            missing=pkg/'missing-static.json';write(missing,manifest('mode_package','static'));check(epoch,True,False);cursor(epoch);done('missing-alternative-created-same-epoch-and-version')
            missing.unlink();check(epoch,False);cursor(epoch,False);done('selected-alternative-deleted-same-epoch-and-version')
            write(base,manifest('mode_package'))
            for mode in ('static','dynamic','auto-jit','auto-aot'):
                epoch+=1;call('xlang/configureAnalysis',packet(epoch,mode));check(epoch,True,False);cursor(epoch);done('legacy-unmarked-package-'+mode)
            assert client.process.pid==pid and sha(entry)==hashlib.sha256(b'// open overlay is authoritative\n').hexdigest()
            close('manual-transcript')
            write(base,manifest('mode_package','dynamic',dict(static='missing-static.json')))
            client=AutomaticClient(server,out/'absent/xl.exe',out);row=dict(pid=client.process.pid);report['processes'].append(row)
            client.send('initialize',dict(capabilities=dict(textDocument=dict(publishDiagnostics=dict(versionSupport=True))),initializationOptions=dict(automaticDiagnostics=True,checkOnSave=False,diagnosticDelay=10,analysisConfiguration=packet(1,'static'))),1);reply,_=client.reply(1);assert 'error' not in reply,reply;client.send('initialized',{});client.open(entry,CODE,7)
            def automatic(errors,after=-1):
                return client.wait_for(lambda stats:stats['submittedChecks']>after and not stats['checking'] and entry.as_uri() in client.latest and bool(client.latest[entry.as_uri()]['diagnostics'])==errors)
            stats=automatic(True);assert any('missing-static.json' in d['message'] for d in client.latest[entry.as_uri()]['diagnostics']);done('automatic-missing-selected-manifest-diagnostic')
            write(missing,manifest('mode_package','static'));stats=automatic(False,stats['submittedChecks']);done('automatic-negative-input-creation-recovery')
            missing.unlink();stats=automatic(True,stats['submittedChecks']);done('automatic-selected-input-deletion-diagnostic')
            before=stats['submittedChecks'];call('xlang/configureAnalysis',packet(2,'dynamic'));stats=automatic(False,before);done('automatic-selection-change-without-edit-clears-diagnostic')
            before=stats['submittedChecks'];call('xlang/configureAnalysis',packet(3,'static'));automatic(True,before);done('automatic-selection-change-without-edit-restores-diagnostic')
            assert all(event.get('version')==7 for event in client.events if event['uri']==entry.as_uri());report['status']='passed'
        for path,digest in protected.items():assert sha(path)==digest,path
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:
        close('transcript');save()
    print(json.dumps(dict(status=report['status'],cases=len(report['cases']),processes=len(report['processes']))))


if __name__=='__main__':main()
