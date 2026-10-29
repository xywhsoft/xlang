"""Compiler-configured operations and exact embedded TCC header byte gate."""
import argparse,base64,datetime,hashlib,json,os,shutil,subprocess,uuid
from pathlib import Path

def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);ap.add_argument('--compiler',type=Path,required=True);ap.add_argument('--clangd',type=Path);ap.add_argument('--gcc',default='gcc');ap.add_argument('--output-root',type=Path);a=ap.parse_args()
 root=a.root.resolve();exe=a.compiler.resolve();gcc=shutil.which(a.gcc);assert os.name=='nt' and gcc
 work=(a.output_root or root/'build/test').resolve()/('c-bridge-environment-'+str(uuid.uuid4()));src=work/'源码 Ω';src.mkdir(parents=True)
 protected={str(p):sha(p.read_bytes()) for p in [exe,exe.with_name('xlc.dll'),exe.with_name('xl.dll')]}
 stamp=exe.parent.parent/'build/tmp/tcc_builtin_resources.c.stamp.json';resources=json.loads(stamp.read_text())['resources']
 expected={x['virtual'].replace('\\','/').lower():x for x in resources if x['virtual'].startswith(('include/','lib/xrt/','lib/xruntime/')) and x['virtual'].endswith('.h')}
 results=[];responses=0;header_comparisons=0;status='failed';started=datetime.datetime.now(datetime.timezone.utc).isoformat();original={}
 env=dict(os.environ,PATH=str(exe.parent)+os.pathsep+os.environ.get('PATH',''))
 def write(name,text):
  p=src/name;p.write_text(text,encoding='utf8',newline='');original[p]=p.read_bytes();return p
 def run(name,args,expected=0):
  cmd=list(map(str,args));(work/(name+'.args.json')).write_text(json.dumps(cmd,ensure_ascii=False,indent=2),encoding='utf8')
  p=subprocess.run(cmd,cwd=src,env=env,capture_output=True,timeout=120);(work/(name+'.stdout')).write_bytes(p.stdout);(work/(name+'.stderr')).write_bytes(p.stderr)
  results.append({'name':name,'exitCode':p.returncode,'stdoutBytes':len(p.stdout),'stderrBytes':len(p.stderr)});assert p.returncode==expected,(name,p.returncode,p.stderr[:800]);return p
 def query(name,path,extra=(),expected=0):return json.loads(run(name,[exe,'c-bridge',path,'--json','--c-environment',*extra],expected).stdout.decode('utf-8-sig'))
 def validate(v,shared=False,full=False):
  nonlocal responses,header_comparisons
  responses+=1;assert v['ok'] and v['cBridge']['complete'] and v['sourceReport']['complete']
  e=v['cBridge']['nativeEnvironment'];assert e['schema']==1 and e['scope']=='compiler-configured-native-c' and e['complete']
  assert e['outputKind']==('shared' if shared else 'exe') and Path(e['directory']).is_absolute()
  assert e['embeddedHeadersAvailable'] and not e['cPreprocessorGraphComplete'] and not e['abiEquivalenceGuaranteed']
  ops=e['operations'];assert ops and len(ops)<=2048 and ops[0]==({'kind':'define','text':'XL6_BUILD_SHARED','value':'1','required':False} if shared else {'kind':'options','text':'-std=c11 -D_CRT_SECURE_NO_WARNINGS','value':'','required':False})
  defines={x['text']:x['value'] for x in ops if x['kind']=='define'}
  assert ('XL6_BUILD_SHARED' in defines)==shared
  if full:assert defines['XRT_MODULE_ALL']==defines['XRUNTIME_MODULE_ALL']=='1'
  headers=e['builtinHeaders'];assert len(headers)==len(expected)<=512;assert {h['name'] for h in headers}==set(expected)
  total=0
  for h in headers:
   data=base64.b64decode(h['base64'],validate=True);row=expected[h['name']];total+=len(data);header_comparisons+=1
   assert len(data)==h['size']<=16*1024*1024 and sha(data)==h['sha256']==row['sha256'] and data==Path(row['source']).read_bytes()
  assert total==e['builtinBytes']<=32*1024*1024;return e
 def failed(v):
  nonlocal responses
  responses+=1;assert not v['ok'] and not v['cBridge']['complete'] and not v['cBridge']['declarations'] and not v['cBridge']['runtimePreamble']
  e=v['cBridge']['nativeEnvironment'];assert not e['complete'] and e['operations']==e['builtinHeaders']==[]
 try:
  source=write('main.xl','export "xl_add" func add(c_int a,c_int b) as c_int\n return a+b\nend\nextern "native_answer" func answer() as c_int\nfunc main(...) as int\n return answer()-42\nend\n')
  native=write('native.c','int native_answer(void) { return xl_add(19,23); }\n')
  cli=query('exe',source);e=validate(cli)
  include=src/'custom headers';include.mkdir();v=query('custom',source,['--inc-path',include]);custom=validate(v)
  assert [o for o in custom['operations'] if o['kind']=='include' and o['required']][-1]['text']==str(include)
  v=query('shared',source,['-shared']);validate(v,True)
  draft=write('draft.utf8','export "xl_draft" func add(c_double a,c_double b) as c_double\n return a+b\nend\n')
  v=query('overlay',source,['--source-overlay',source,draft]);validate(v);assert 'xl_draft' in v['cBridge']['declarations'] and 'xl_add' not in v['cBridge']['declarations']
  project=write('app.xlp',json.dumps({'schema':1,'name':'bridge','kind':'exe','entry':'main.xl','cSources':['native.c']},ensure_ascii=False))
  v=query('project-native',project);validate(v,full=True)
  run('project-kind-conflict',[exe,'c-bridge',project,'--json','--c-environment','-shared'],1)
  dllproject=write('shared.xlp',json.dumps({'schema':1,'name':'bridge','kind':'dll','entry':'main.xl','cSources':['native.c']},ensure_ascii=False))
  v=query('project-shared',dllproject);validate(v,True,True)
  invalid=write('invalid.utf8','func broken() as int\n return unknown\nend\n');failed(query('invalid',source,['--source-overlay',source,invalid],1))
  for name,args in [('wrong-command',['check',source,'--json','--c-environment']),('no-json',['c-bridge',source,'--c-environment']),('value-env',['c-bridge',source,'--json','--inc-path','--c-environment']),('boundary-env',['c-bridge',source,'--json','--','--c-environment'])]:
   p=run(name,[exe,*args],1 if name in ('wrong-command','no-json') else 0)
   if p.returncode==0:assert 'nativeEnvironment' not in json.loads(p.stdout)['cBridge']
  host=work/'environment-host.exe';run('host-build',[gcc,'-std=c11','-Wall','-Wextra','-Werror','-municode','-I',root/'release',root/'test/cases/c_bridge_environment_host.c','-o',host])
  directory=src/'native host';directory.mkdir();run('host-execute',[host,exe.with_name('xlc.dll'),directory])
  for name,shared,full in [('environment-exe',False,False),('environment-native',False,True),('environment-shared',True,True)]:validate(json.loads((directory/(name+'.json')).read_text(encoding='utf-8-sig')),shared,full)
  for name in ['environment-invalid','environment-limit']:failed(json.loads((directory/(name+'.json')).read_text(encoding='utf-8-sig')))
  plain=json.loads((directory/'plain-after-limit.json').read_text(encoding='utf-8-sig'));assert plain['ok'] and plain['cBridge']['complete'] and 'nativeEnvironment' not in plain['cBridge']
  if a.clangd:
   cache=src/'exported cache';cache.mkdir()
   for h in e['builtinHeaders']:
    p=cache/h['name'];assert p.resolve().is_relative_to(cache.resolve());p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(base64.b64decode(h['base64'],validate=True))
   forced=cache/'bridge.h';forced.write_text(cli['cBridge']['runtimePreamble']+cli['cBridge']['declarations'],encoding='utf8',newline='')
   arguments=['clang','-x','c','-std=c11','-D_CRT_SECURE_NO_WARNINGS','-target','x86_64-w64-windows-gnu']
   for o in e['operations']:
    if o['kind']=='include':
     path=o['text'].replace('\\','/');real=Path(path);real=real if real.is_absolute() else Path(e['directory'])/real;arguments+=['-I',str(real)]
     if path in ('.','include','include/winapi','lib/xrt','lib/xruntime'):arguments+=['-I',str(cache/path)]
    if o['kind']=='define':arguments+=['-D',o['text']+'='+o['value']]
   arguments+=['-include',str(forced),'-c',str(native)];db=[{'directory':e['directory'],'file':str(native),'arguments':arguments}]
   (src/'compile_commands.json').write_text(json.dumps(db,ensure_ascii=False,indent=2),encoding='utf8')
   p=run('clangd-actual-exported-heads',[a.clangd,'--check='+str(native),'--compile-commands-dir='+str(src),'--log=verbose'])
   assert b'All checks completed, 0 errors' in p.stderr
  run('mixed-build',[exe,'build',project,'--save-temps','-o',src/'mixed.exe']);run('mixed-execute',[src/'mixed.exe'])
  assert all(p.read_bytes()==data for p,data in original.items());status='passed'
 finally:
  assert all(sha(Path(p).read_bytes())==h for p,h in protected.items())
  record={'status':status,'startedUtc':started,'finishedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(exe),'commands':len(results),'responsesValidated':responses,'headerComparisons':header_comparisons,'headerCount':len(expected),'clangdRequested':bool(a.clangd),'results':results,'protected':protected}
  (work/'evidence.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps({'status':status,'commands':len(results),'responsesValidated':responses,'headerComparisons':header_comparisons,'evidence':str(work/'evidence.json')},ensure_ascii=False))
if __name__=='__main__':main()
