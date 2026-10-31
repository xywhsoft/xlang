"""Force real demand navigation to wait; verify owner dispatch and input fences.

--build uses an intact passed normal build and adds only a private link wrapper.
--server/--reference accept an already linked diagnostic component pair. The
named-event barrier is released only after unrelated protocol traffic and any
cancellation response arrive. No sleeps or timing wins stand in for that proof.
This is functional coverage,not performance admission or a shipped test mode.
"""
from pathlib import Path
import argparse,ctypes,ctypes.wintypes as w,hashlib,json,os,queue,subprocess,time,uuid
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,checked
from check_language_definitions import position


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def barrier_source():
    return r'''#include "services/semantic_definition.h"
#include <windows.h>
#include <stdio.h>
xide_semantic_definitions* __real_xideSemanticNavigationResolve(const xide_navigation_snapshot*,xls_projection_store*,xls_projection**,xide_navigation_work*,xide_model_error*);
void __real_xideSemanticNavigationBind(xide_navigation_snapshot*,xls_projection_store*);
void __wrap_xideSemanticNavigationBind(xide_navigation_snapshot* snapshot,xls_projection_store* store)
{
 __real_xideSemanticNavigationBind(snapshot,store);
 wchar_t name[128];DWORD n=GetEnvironmentVariableW(L"XLS_TEST_NAV_CAPTURE",name,128);
 if(n){if(n>=128)ExitProcess(93);HANDLE e=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);if(!e || !SetEvent(e))ExitProcess(94);CloseHandle(e);}
}
xide_semantic_definitions* __wrap_xideSemanticNavigationResolve(const xide_navigation_snapshot* snapshot,xls_projection_store* store,xls_projection** lease,xide_navigation_work* work,xide_model_error* error)
{
 wchar_t entered[128],release[128];
 DWORD a=GetEnvironmentVariableW(L"XLS_TEST_NAV_ENTER",entered,128),b=GetEnvironmentVariableW(L"XLS_TEST_NAV_RELEASE",release,128);
 if(a || b){
  if(!a || a>=128 || !b || b>=128)ExitProcess(90);
  HANDLE e=OpenEventW(EVENT_MODIFY_STATE,FALSE,entered),r=OpenEventW(SYNCHRONIZE,FALSE,release);
  if(!e || !r || !SetEvent(e))ExitProcess(91);
  if(WaitForSingleObject(r,10000)!=WAIT_OBJECT_0)ExitProcess(92);
  CloseHandle(e);CloseHandle(r);
 }
 xide_semantic_definitions* result=__real_xideSemanticNavigationResolve(snapshot,store,lease,work,error);
 wchar_t complete[128];DWORD n=GetEnvironmentVariableW(L"XLS_TEST_NAV_COMPLETE",complete,128);
 if(n){if(n>=128)ExitProcess(95);HANDLE e=OpenEventW(EVENT_MODIFY_STATE,FALSE,complete);if(!e || !SetEvent(e))ExitProcess(96);CloseHandle(e);}
 return result;
}
'''


def build_barrier(build,out,record):
    from build_lsp_lifecycle_profile import verify_build
    build=build.resolve(strict=True);b=json.loads((build/'build-result.json').read_text('utf8'));verify_build(b,build)
    source=out/'navigation_barrier.c';source.write_text(barrier_source(),encoding='utf8',newline='\n');obj=out/'navigation_barrier.o';server=out/'barrier-ls.exe';temporary=out/'compiler-temp';temporary.mkdir()
    compile_template=next(c for c in b['commands'] if '-c' in c and Path(c[c.index('-c')+1]).stem=='semantic_definition')
    compile_command=compile_template[:compile_template.index('-c')]+['-c',str(source),'-o',str(obj)]
    link=next(c for c in b['commands'] if '-c' not in c and str(build/'xlang-ls.exe') in c);link=[str(server) if x==str(build/'xlang-ls.exe') else x for x in link];link.insert(link.index(str(build/'xlang-analysis-core.a')),str(obj));link+=['-Wl,--wrap=xideSemanticNavigationResolve','-Wl,--wrap=xideSemanticNavigationBind']
    env=dict(os.environ,TMP=str(temporary),TEMP=str(temporary));env['PATH']=str(Path(b['compiler']).parent)+os.pathsep+env.get('PATH','');record['commands']=[]
    for name,command in [('compile',compile_command),('link',link)]:
        cp=subprocess.run(command,cwd=out,env=env,capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);record['commands'].append(dict(command=command,exit=cp.returncode,log=str(log)));assert cp.returncode==0,(name,log.read_text('utf8',errors='replace')[-2400:])
    verify_build(b,build);record['normalBuild']=str(build);record['protected'].update({str(build/'build-result.json'):sha(build/'build-result.json'),str(source):sha(source),str(obj):sha(obj)})
    return server,build/'xlang-ls.exe'


def main():
    p=argparse.ArgumentParser(description=__doc__);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--build',type=Path);g.add_argument('--server',type=Path);p.add_argument('--reference',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    if os.name!='nt':raise RuntimeError('The private host barrier currently uses Windows named events.')
    protected={str(Path(__file__).resolve()):sha(__file__)};record=dict(status='running',accepted=False,scope=__doc__,protected=protected,cases=[]);clients=[]
    k=ctypes.WinDLL('kernel32',use_last_error=True);k.CreateEventW.argtypes=[w.LPVOID,w.BOOL,w.BOOL,w.LPCWSTR];k.CreateEventW.restype=w.HANDLE;k.SetEvent.argtypes=[w.HANDLE];k.SetEvent.restype=w.BOOL;k.ResetEvent.argtypes=[w.HANDLE];k.ResetEvent.restype=w.BOOL;k.WaitForSingleObject.argtypes=[w.HANDLE,w.DWORD];k.WaitForSingleObject.restype=w.DWORD;k.CloseHandle.argtypes=[w.HANDLE];k.CloseHandle.restype=w.BOOL
    handles=[]
    def save():(out/'result.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    try:
        if a.build:server,reference=build_barrier(a.build,out,record)
        else:
            assert a.reference,'--server requires its uninstrumented --reference';server=a.server.resolve(strict=True);reference=a.reference.resolve(strict=True)
        protected.update({str(x):sha(x) for x in [server,reference,Path(__file__).with_name('check_lsp_automatic_diagnostics.py'),Path(__file__).with_name('check_lsp_module_semantics.py'),Path(__file__).with_name('check_language_snapshots.py'),Path(__file__).with_name('check_language_definitions.py')]})
        for opened in [False,True]:
            for mode in ['ordinary','cancel-one','source-change','batch-replaced']:
                folder=out/(('opened-' if opened else 'disk-')+mode);folder.mkdir();entry=folder/'main.xl';dep=folder/'dep.xl';main_text='import dep\nfunc main(...) as int\n return dep.compute(3)\nend\n';entry.write_text(main_text,encoding='utf8');protected[str(entry)]=sha(entry)
                bodies=['\ufeffexport func compute(int value) as int\r\n string label="😀"; int selected=value+1\r\n return selected\r\nend\r\n','\ufeff// 中文😀\r\nexport func compute(int value) as int\r\n string label="😀"; int updated=value+2\r\n return updated\r\nend\r\n']
                def params(body,version,check_id):return dict(textDocument=dict(uri=dep.as_uri()),version=version,checkId=check_id,position=position(body,body.rindex('return ')+len('return ')))
                ref=AutomaticClient(reference,folder/'absent/xl.exe',folder);clients.append(ref);answers=[]
                try:
                    initialize(ref,entry,[entry,dep],folder)
                    if opened:ref.open(entry,main_text,1)
                    ref.open(dep,bodies[0],1)
                    for generation,body in enumerate(bodies):
                        if generation:ref.change(dep,body,2)
                        value=checked(ref,dep,generation+1);assert value['ok'];q=params(body,generation+1,value['checkId']);hover=ref.call('textDocument/hover',q);symbols=ref.call('textDocument/documentSymbol',q);assert hover and symbols;answers.append(dict(check={key:v for key,v in value.items() if key not in ['analysis','durationMs','checkId']},hover=hover,symbols=symbols))
                finally:assert not ref.stop() and ref.process.returncode==0
                token=uuid.uuid4().hex;enter_name='Local\\xls-nav-enter-'+token;release_name='Local\\xls-nav-release-'+token;capture_name='Local\\xls-nav-capture-'+token;complete_name='Local\\xls-nav-complete-'+token;e=k.CreateEventW(None,True,False,enter_name);release=k.CreateEventW(None,True,False,release_name);captured=k.CreateEventW(None,True,False,capture_name);complete=k.CreateEventW(None,True,False,complete_name);assert e and release and captured and complete;handles += [e,release,captured,complete]
                old={key:os.environ.get(key) for key in ['XLS_TEST_NAV_ENTER','XLS_TEST_NAV_RELEASE','XLS_TEST_NAV_CAPTURE','XLS_TEST_NAV_COMPLETE']}
                try:
                    os.environ.update(XLS_TEST_NAV_ENTER=enter_name,XLS_TEST_NAV_RELEASE=release_name,XLS_TEST_NAV_CAPTURE=capture_name,XLS_TEST_NAV_COMPLETE=complete_name);c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
                finally:
                    for key,value in old.items():
                        if value is None:os.environ.pop(key,None)
                        else:os.environ[key]=value
                inbox={};handled=set()
                def take(ident,forbidden=(),timeout=3):
                    until=time.monotonic()+timeout
                    while ident not in inbox:
                        try:m=c.messages.get(timeout=max(.001,until-time.monotonic()))
                        except queue.Empty:raise AssertionError('Owner dispatch did not answer while navigation was held.')
                        if isinstance(m,BaseException):raise m
                        c.transcript.append(dict(receive=m));key=m.get('id')
                        if key is not None:
                            assert key not in forbidden,('Navigation escaped its unreleased barrier',m);assert key not in handled and key not in inbox,('duplicate reply',m);inbox[key]=m
                        assert time.monotonic()<until
                    handled.add(ident);return inbox.pop(ident)
                try:
                    initialize(c,entry,[entry,dep],folder)
                    if opened:c.open(entry,main_text,1)
                    c.open(dep,bodies[0],1);value=checked(c,dep,1);assert {key:v for key,v in value.items() if key not in ['analysis','durationMs','checkId']}==answers[0]['check'];assert value['analysis']['navigation']['deferred']
                    assert k.WaitForSingleObject(captured,0)==0 and k.ResetEvent(captured)
                    assert value['analysis']['navigation']['points']['ready']
                    q=params(bodies[0],1,value['checkId']);c.send('textDocument/documentSymbol',q,700);assert k.WaitForSingleObject(e,5000)==0,'Full navigation did not enter its actual resolver.'
                    c.send('xlang/inputState',{},800);ping=take(800,forbidden=[700]);assert 'error' not in ping
                    c.send('textDocument/hover',q,702);assert take(702,forbidden=[700]).get('result')==answers[0]['hover'],'Native point query waited for the unrelated full projection.'
                    if mode=='cancel-one':
                        c.send('textDocument/documentSymbol',q,701);c.send('$/cancelRequest',dict(id=700));cancelled=take(700,forbidden=[701]);assert cancelled['error']['code']==-32800;assert k.SetEvent(release);v=take(701);assert v.get('result')==answers[0]['symbols'],v
                    elif mode=='source-change':
                        c.change(dep,bodies[1],2);cancelled=take(700);assert cancelled['error']['code']==-32801;c.send('xlang/check',dict(textDocument=dict(uri=dep.as_uri()),version=2),900);assert k.WaitForSingleObject(captured,5000)==0,'New capture did not overtake held old navigation.';assert k.SetEvent(release);assert k.WaitForSingleObject(complete,5000)==0,'Old resolver did not finish before publication accounting.';reply=take(900,timeout=30);assert 'error' not in reply,reply;assert reply['result']['analysis']['navigation']['points']['builds']==1;current=checked(c,dep,2);assert {key:v for key,v in current.items() if key not in ['analysis','durationMs','checkId']}==answers[1]['check'];assert current['analysis']['navigation']['published']==value['analysis']['navigation']['published']+1,'Old cancelled work repopulated the new bank beyond the one new native point view.';assert c.call('textDocument/hover',params(bodies[1],2,current['checkId']))==answers[1]['hover']
                    elif mode=='batch-replaced':
                        c.send('xlang/check',dict(textDocument=dict(uri=dep.as_uri()),version=1),900);cancelled=take(700);assert cancelled['error']['code']==-32801;assert k.WaitForSingleObject(captured,5000)==0,'Replacement capture did not overtake held old navigation.';assert k.SetEvent(release);assert k.WaitForSingleObject(complete,5000)==0,'Superseded resolver did not finish before publication accounting.';reply=take(900,timeout=30);assert 'error' not in reply,reply;assert reply['result']['analysis']['navigation']['points']['cacheHits']==1;current=checked(c,dep,1);assert current['analysis']['navigation']['published']==value['analysis']['navigation']['published'],'Superseded batch repopulated the new bank.';assert c.call('textDocument/hover',params(bodies[0],1,current['checkId']))==answers[0]['hover']
                    else:
                        assert k.SetEvent(release);v=take(700);assert v.get('result')==answers[0]['symbols'],v
                    # A completed demand and a later owner-loop fence must not
                    # emit a second response for an already cancelled request.
                    c.send('xlang/inputState',{},801);assert 'error' not in take(801)
                    record['cases'].append(dict(name=folder.name,passed=True,ownerPingBeforeRelease=True));save();print(folder.name,flush=True)
                finally:
                    k.SetEvent(release);stderr=c.stop();(folder/'candidate-stderr.log').write_text(stderr,encoding='utf8');(folder/'candidate-transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8');(folder/'reference-transcript.json').write_text(json.dumps(ref.transcript,ensure_ascii=False,indent=2),encoding='utf8')
                assert not stderr and c.process.returncode==0 and not dep.exists()
                k.CloseHandle(e);k.CloseHandle(release);k.CloseHandle(captured);k.CloseHandle(complete);handles.remove(e);handles.remove(release);handles.remove(captured);handles.remove(complete)
        assert len(record['cases'])==8 and all(sha(path)==digest for path,digest in protected.items());record['status']='passed'
    except BaseException as exc:record.update(status='failed',error=repr(exc));raise
    finally:
        for handle in handles:k.SetEvent(handle)
        for c in clients:c.stop()
        for handle in handles:k.CloseHandle(handle)
        save()
    print(json.dumps(dict(status=record['status'],cases=len(record['cases']),proof=str(out/'result.json'))))


if __name__=='__main__':main()
