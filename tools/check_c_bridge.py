"""Real compiler native declarations, overlays, freshness and compatibility gate."""
import argparse,datetime,hashlib,json,os,shutil,subprocess,uuid
from pathlib import Path

def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);ap.add_argument('--compiler',type=Path);ap.add_argument('--gcc',default='gcc');ap.add_argument('--output-root',type=Path)
 a=ap.parse_args();root=a.root.resolve();exe=(a.compiler or root/'release/xl.exe').resolve();gcc=shutil.which(a.gcc)
 if os.name!='nt' or not gcc:raise RuntimeError('Windows and MinGW GCC required')
 work=(a.output_root or root/'build/test').resolve()/('c-bridge-'+str(uuid.uuid4()));src=work/'源码 Ω';src.mkdir(parents=True)
 protected={str(p):sha(p.read_bytes()) for p in [exe,exe.with_name('xlc.dll'),exe.with_name('xl.dll')]}
 started=datetime.datetime.now(datetime.timezone.utc).isoformat();results=[];status='failed';preserved={};validated=0
 env=dict(os.environ,PATH=str(exe.parent)+os.pathsep+os.environ.get('PATH',''))
 def write(name,text):
  p=src/name;p.write_bytes(text.encode('utf8'));preserved[p]=p.read_bytes();return p
 def run(name,cmd,expected=0):
  args=list(map(str,cmd));(work/(name+'.args.json')).write_text(json.dumps(args,ensure_ascii=False,indent=2),encoding='utf8')
  c=subprocess.run(args,cwd=src,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
  (work/(name+'.stdout')).write_bytes(c.stdout);(work/(name+'.stderr')).write_bytes(c.stderr)
  results.append({'name':name,'exitCode':c.returncode,'stdoutBytes':len(c.stdout),'stderrBytes':len(c.stderr)})
  assert c.returncode==expected,(name,c.returncode,c.stdout[:800],c.stderr[:800]);return c
 def payload(c):return json.loads(c.stdout.decode('utf-8-sig'))
 def query(name,path='main.xl',extra=(),expected=0):return payload(run(name,[exe,'c-bridge',path,'--json',*extra],expected))
 def valid(v):
  nonlocal validated
  validated+=1;assert v['ok'] and v['sourceReport']['complete'] and not v['diagnostics']
  b=v['cBridge'];assert b['schema']==1 and b['scope']=='lowered-xlang-native-declarations' and b['complete']
  assert b['headerGuard']=='XL6_MODULE_H' and b['cInputsObserved'] is False
  assert b['runtimePreamble']=='#include <__xl_runtime/xruntime_decl.h>\n'
  assert '#ifndef XL6_MODULE_H' in b['declarations'] and len(b['declarations'].encode())<=1024*1024
  assert v['resolutionReport']['scope']=='xlang-import-resolution';return b
 def failed(v):
  nonlocal validated
  validated+=1;assert not v['ok'] and v['diagnostics'] and not v['cBridge']['complete']
  assert v['cBridge']['declarations']==v['cBridge']['runtimePreamble']==''
 def origin(v,name,kind,data):
  if kind=='overlay':data=data.removeprefix(bytes((239,187,191)))
  row=next(f for f in v['sourceReport']['files'] if Path(f['path']).name==name and f['origin']==kind)
  assert Path(row['path']).is_absolute() and row['rawSha256']==sha(data) and row['rawSize']==len(data),(name,row)
 try:
  main=write('main.xl','export "xl_add" func add(c_int left, c_int right) as c_int\n return left + right\nend\nextern "native_answer" func nativeAnswer() as c_int\nfunc main(...) as int\n if nativeAnswer() != 42 then\n  return 1\n end\n return 0\nend\n')
  native=write('native.c','int native_answer(void) { return xl_add(19, 23); }\n')
  baseline=query('disk');b=valid(baseline);assert 'xl_add' in b['declarations'];origin(baseline,'main.xl','disk',main.read_bytes())
  run('cgen',[exe,'cgen',main,'--emit-header',src/'module.h'])
  ordinary=(src/'module.h').read_text(encoding='utf8')
  assert b['declarations']==ordinary
  draft=write('main.snapshot','\ufeffexport "draft_add" func 加法(c_double left, c_double right) as c_double\r\n return left + right\r\nend\r\n')
  v=query('entry-overlay',extra=['--source-overlay',main,draft]);d=valid(v);origin(v,'main.xl','overlay',draft.read_bytes());assert 'draft_add' in d['declarations'] and 'double' in d['declarations'] and 'xl_add' not in d['declarations']
  (src/'draft-output').mkdir()
  run('overlay-cgen',[exe,'cgen',main,'--source-overlay',main,draft,'--emit-header',src/'draft-output/module.h'])
  assert d['declarations']==(src/'draft-output/module.h').read_text(encoding='utf8')
  helper=write('helper.xl','export "dep_old" func answer() as c_int\n return 42\nend\n')
  module=write('module.xl','import helper\nfunc main(...) as int\n return helper.answer() - 42\nend\n')
  depdraft=write('helper.snapshot','export "dep_draft" func answer() as c_int\n return 42\nend\n')
  v=query('recursive-overlay',module,['--source-overlay',helper,depdraft]);d=valid(v);origin(v,'helper.xl','overlay',depdraft.read_bytes());assert 'dep_draft' in d['declarations'] and 'dep_old' not in d['declarations']
  v=query('recursive-disk',module);d=valid(v);assert 'dep_old' in d['declarations'] and 'dep_draft' not in d['declarations']
  virtual=src/'virtual.xl';v=query('new-virtual',virtual,['--source-overlay',virtual,draft]);valid(v);assert not virtual.exists()
  invalid=write('bad.snapshot','func broken() as int\n return undefined_name\nend\n')
  failed(query('semantic-failure',extra=['--source-overlay',main,invalid],expected=1))
  failed(query('import-failure',module,['--source-overlay',helper,invalid],expected=1))
  syntax=write('syntax.snapshot','func broken(\n')
  failed(query('syntax-failure',extra=['--source-overlay',main,syntax],expected=1))
  absent=write('missing-import.xl','import unavailable_dependency\n')
  failed(query('missing-import',absent,expected=1))
  valid(query('recovery'))
  explicit=write('explicit.xl','import "helper.xl"\nfunc main(...) as int\n return helper.answer() - 42\nend\n')
  failed(query('explicit-overlay-source-conflict',explicit,['--source-overlay',helper,depdraft],expected=1))
  near=write('near-limit.xl','export "cap_'+('a'*(1024*1024-16384))+'" func huge(c_int n) as c_int\n return n\nend\n')
  assert len(valid(query('near-header-limit',near))['declarations'].encode())>1000000
  huge=write('over-limit.xl','export "cap_'+('a'*(1024*1024))+'" func huge(c_int n) as c_int\n return n\nend\n')
  v=query('over-header-limit',huge,expected=1);failed(v);assert v['phase']=='c-bridge' and '1 MiB' in v['diagnostics'][-1]['message']
  (src/'overflow-output').mkdir();run('ordinary-large-header',[exe,'cgen',huge,'--emit-header',src/'overflow-output/module.h'])
  assert (src/'overflow-output/module.h').stat().st_size>1024*1024
  plain=payload(run('legacy-check',[exe,'check',main,'--json']));assert set(plain)=={'ok','phase','diagnostics'}
  plain=payload(run('legacy-report',[exe,'check',main,'--json','--source-report']));assert 'cBridge' not in plain
  for name,extra in [('no-json',[]),('wrong-index',['--json','--semantic-index']),('wrong-completion',['--json','--completion-at',main,'0']),('wrong-source-ranges',['--json','--source-ranges']),('wrong-source-report',['--json','--source-report']),('value-json',['--inc-path','--json']),('boundary-json',['--','--json'])]:
   run(name,[exe,'c-bridge',main,*extra],1)
  project=write('app.xlp',json.dumps({'schema':1,'name':'bridge','kind':'exe','entry':'main.xl','files':['main.xl','native.c'],'cSources':['native.c']},ensure_ascii=False))
  v=query('project',project);valid(v);assert not any(Path(f['path']).name=='app.xlp' for f in v['sourceReport']['files'])
  mixed=run('mixed-build',[exe,'build',project,'--save-temps','-o',src/'mixed.exe'])
  marker='temporary files kept: ';trace=mixed.stderr.decode('utf8');assert marker in trace
  temps=Path(trace.split(marker,1)[1].splitlines()[0]);assert (temps/'module.h').read_text(encoding='utf8')==b['declarations']
  assert (temps/'csrc_0.c').read_text(encoding='utf8').startswith(b['runtimePreamble']+b['declarations'])
  (work/'mixed-temp-directory.txt').write_text(str(temps),encoding='utf8')
  run('mixed-execute',[src/'mixed.exe'])
  host=work/'c-bridge-host.exe';run('host-build',[gcc,'-std=c11','-Wall','-Wextra','-Werror','-municode','-I',root/'release',root/'test/cases/c_bridge_host.c','-o',host])
  for dllname in ['xlc.dll']:
   directory=src/dllname;directory.mkdir();run('host-'+dllname,[host,exe.with_name(dllname),directory])
   values={n:json.loads((directory/(n+'.json')).read_text(encoding='utf-8-sig')) for n in ['first','authoritative','virtual','invalid','recovered','removed','replaced','other','unchanged','legacy','disk-before','disk-changed']}
   assert valid(values['first'])['declarations']==(directory/'first.h').read_text(encoding='utf8')
   assert values['first']==values['authoritative'];failed(values['invalid']);failed(values['removed'])
   assert 'memory_second' in valid(values['virtual'])['declarations'];assert 'memory_first' in valid(values['recovered'])['declarations']
   assert 'memory_second' in valid(values['replaced'])['declarations'] and values['replaced']==values['unchanged']
   assert 'memory_first' in valid(values['other'])['declarations'];assert 'cBridge' not in values['legacy']
   assert 'memory_first' in valid(values['disk-before'])['declarations'] and 'memory_second' in valid(values['disk-changed'])['declarations']
   origin(values['disk-changed'],'disk.xl','disk',(directory/'disk.xl').read_bytes())
  run('runtime-api-unavailable',[host,exe.with_name('xl.dll'),src,'--runtime-only'])
  assert all(p.read_bytes()==data for p,data in preserved.items());status='passed'
 finally:
  assert all(sha(Path(p).read_bytes())==h for p,h in protected.items())
  record={'status':status,'startedUtc':started,'finishedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(exe),'commands':len(results),'responsesValidated':validated,'preservedSources':len(preserved),'results':results,'protected':protected}
  (work/'evidence.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf8')
  print(json.dumps({'status':status,'commands':len(results),'responsesValidated':validated,'evidence':str(work/'evidence.json')},ensure_ascii=False))
if __name__=='__main__':main()
