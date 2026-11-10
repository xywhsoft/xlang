"""Pause actual token projection workers; prove nonblocking cancellation and guards."""
from pathlib import Path
import argparse,ctypes,ctypes.wintypes as wt,hashlib,json,os,subprocess,time,uuid
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_semantic_tokens import CAP,checked

BARRIER=r'''#include "services/semantic_token_positions.h"
#include <windows.h>
xide_token_positions* __real_xideTokenPositionsCreate(const xls_semantic_check*,size_t,int,size_t,int (*)(void*),void*,enum xls_semantic_tokens_error*);
xide_token_positions* __wrap_xideTokenPositionsCreate(const xls_semantic_check* check,size_t source,int prefix,size_t budget,int (*cancelled)(void*),void* user,enum xls_semantic_tokens_error* error)
{
 wchar_t entered[128],release[128];DWORD a=GetEnvironmentVariableW(L"XLS_TEST_TOKEN_ENTER",entered,128),b=GetEnvironmentVariableW(L"XLS_TEST_TOKEN_RELEASE",release,128);
 if(a || b){
  if(!a || a>=128 || !b || b>=128)ExitProcess(90);
  HANDLE e=OpenSemaphoreW(SEMAPHORE_MODIFY_STATE,FALSE,entered),r=OpenEventW(SYNCHRONIZE,FALSE,release);
  if(!e || !r || !ReleaseSemaphore(e,1,NULL))ExitProcess(91);
  if(WaitForSingleObject(r,15000)!=WAIT_OBJECT_0)ExitProcess(92);
  CloseHandle(e);CloseHandle(r);
 }
 return __real_xideTokenPositionsCreate(check,source,prefix,budget,cancelled,user,error);
}
'''


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--build',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);a=parser.parse_args()
    root=Path(__file__).resolve().parents[2];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);build=a.build.resolve(strict=True);b=json.loads((build/'result.json').read_text());assert b['status']=='passed'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(Path(__file__).resolve()):sha(__file__),str(build/'result.json'):sha(build/'result.json')};report=dict(status='failed',cases=[],commands=[],protected=protected)
    for row in b['dependencyAudit']:
        assert sha(row['actualObject'])==row['actualObjectSha256']
        for p,h in row['dependencies'].items():assert sha(p)==h,p
    source=out/'worker_barrier.c';source.write_text(BARRIER,encoding='utf8');obj=out/'worker_barrier.o';row=next(x for x in b['dependencyAudit']if Path(x['command'][x['command'].index('-c')+1]).stem=='semantic_token_positions');command=row['command'].copy();command[command.index('-c')+1]=str(source);command[command.index('-o')+1]=str(obj)
    link=next(c.copy()for c in b['commands']if '-o'in c and Path(c[c.index('-o')+1]).name=='xlang-ls.exe');exe=out/'barrier-ls.exe';link[link.index('-o')+1]=str(exe);link.insert(1,str(obj));link.append('-Wl,--wrap=xideTokenPositionsCreate')
    for name,c in [('compile',command),('link',link)]:
        p=subprocess.run(c,capture_output=True,timeout=180);(out/(name+'.log')).write_bytes(p.stdout+p.stderr);report['commands'].append(dict(command=c,exit=p.returncode));assert not p.returncode,(name,p.stderr.decode(errors='replace')[-1200:])
    alias=root/'release/win_x64/.lsp-semantic-token-worker-barrier.exe';assert not alias.exists();alias.write_bytes(exe.read_bytes());h=sha(alias);report['serverSha256']=h
    k=ctypes.WinDLL('kernel32',use_last_error=True);k.CreateSemaphoreW.argtypes=[wt.LPVOID,wt.LONG,wt.LONG,wt.LPCWSTR];k.CreateSemaphoreW.restype=wt.HANDLE;k.CreateEventW.argtypes=[wt.LPVOID,wt.BOOL,wt.BOOL,wt.LPCWSTR];k.CreateEventW.restype=wt.HANDLE;k.WaitForSingleObject.argtypes=[wt.HANDLE,wt.DWORD];k.WaitForSingleObject.restype=wt.DWORD;k.SetEvent.argtypes=[wt.HANDLE];k.ResetEvent.argtypes=[wt.HANDLE];k.CloseHandle.argtypes=[wt.HANDLE]
    tag=uuid.uuid4().hex;enteredName='Local\\xls-token-enter-'+tag;releaseName='Local\\xls-token-release-'+tag;entered=k.CreateSemaphoreW(None,0,16,enteredName);release=k.CreateEventW(None,True,False,releaseName);assert entered and release
    env={name:os.environ.get(name)for name in ('XLS_TEST_TOKEN_ENTER','XLS_TEST_TOKEN_RELEASE')};os.environ['XLS_TEST_TOKEN_ENTER']=enteredName;os.environ['XLS_TEST_TOKEN_RELEASE']=releaseName;c=None
    def wait_entered():assert k.WaitForSingleObject(entered,10000)==0,'Actual worker did not enter projection barrier'
    def send_full(path):
        c.request+=1;c.send('textDocument/semanticTokens/full',dict(textDocument=dict(uri=path.as_uri())),c.request);return c.request
    def collect(ids):
        pending=set(ids);replies={};until=time.monotonic()+10
        while pending:
            assert time.monotonic()<until,'Owner did not answer while helper was paused'
            item=c.next()
            if 'id'in item:
                assert item['id']in pending,item;replies[item['id']]=item;pending.remove(item['id'])
        return replies
    try:
        c=AutomaticClient(alias,out/'absent/xl.exe',out);c.send('initialize',dict(capabilities=dict(textDocument=dict(semanticTokens=CAP)),initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,backgroundWorkspaceSymbols=False)),1);reply,_=c.reply(1);assert 'semanticTokensProvider'in reply['result']['capabilities'];c.send('initialized',{})
        path=out/'main.xl';text='func main(...) as int\n int value=7\n return value\nend\n';c.open(path,text,1);assert checked(c,path,1)['ok']
        ids=[send_full(path)for _ in range(5)]
        for _ in range(4):wait_entered()
        began=time.perf_counter()
        messages=[dict(jsonrpc='2.0',method='$/cancelRequest',params=dict(id=i))for i in ids[:4]];c.request+=1;control=c.request;messages.append(dict(jsonrpc='2.0',method='xlang/analysisStats',params={},id=control));c.batch(messages);replies=collect([*ids[:4],control]);assert all(replies[i].get('error',{}).get('code')==-32800 for i in ids[:4]);assert 'result'in replies[control]
        report['cases'].append(dict(name='four-paused-workers-cancel-without-join-and-fifth-remains-queued',passed=True,ownerObservedMs=(time.perf_counter()-began)*1000))
        assert k.SetEvent(release);reply,_=c.reply(ids[4]);assert reply['result']['data'];report['cases'].append(dict(name='retired-jobs-reclaimed-and-queued-work-recovers',passed=True))
        while k.WaitForSingleObject(entered,0)==0:pass
        assert k.ResetEvent(release);stale=send_full(path);wait_entered();c.request+=1;control=c.request
        c.batch([dict(jsonrpc='2.0',method='textDocument/didChange',params=dict(textDocument=dict(uri=path.as_uri(),version=2),contentChanges=[dict(text=text.replace('=7','=8'))])),dict(jsonrpc='2.0',method='xlang/analysisStats',params={},id=control)])
        replies=collect([stale,control]);assert replies[stale].get('error',{}).get('code')==-32801 and 'result'in replies[control];assert k.SetEvent(release)
        latest=c.call('textDocument/semanticTokens/full',dict(textDocument=dict(uri=path.as_uri())));assert latest['data'];report['cases'].append(dict(name='edit-invalidates-paused-request-before-native-helper-returns',passed=True))
        while k.WaitForSingleObject(entered,0)==0:pass
        dep=out/'dep.xl';dep.write_text('export func value() as int\n return 7\nend\n',encoding='utf8');entry=out/'disk-main.xl';c.open(entry,'import dep\nfunc main(...) as int\n return dep.value()\nend\n',1);assert checked(c,entry,1)['ok'];assert k.ResetEvent(release);stale=send_full(entry);wait_entered();dep.write_text('export func value() as string\n return "bad"\nend\n',encoding='utf8');assert k.SetEvent(release);reply,_=c.reply(stale);assert reply.get('error',{}).get('code')==-32801,reply;report['cases'].append(dict(name='same-generation-disk-mutation-rejected-after-prevalidation',passed=True))
        report['status']='passed'
    finally:
        k.SetEvent(release)
        if c:
            stderr=c.stop();(out/'stderr.log').write_text(stderr,encoding='utf8');(out/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8');assert c.process.returncode==0
        for name,value in env.items():
            if value is None:os.environ.pop(name,None)
            else:os.environ[name]=value
        k.CloseHandle(entered);k.CloseHandle(release);assert sha(alias)==h;alias.unlink()
        assert all(sha(p)==h for p,h in protected.items());(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('PASS semantic token worker barriers',len(report['cases']),'cases',flush=True)

if __name__=='__main__':main()
