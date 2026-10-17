"""Actual ABI emission to checked-source identity/range mapping gate."""
import argparse,datetime,hashlib,json,os,subprocess,uuid
from pathlib import Path

def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--compiler',type=Path,required=True);ap.add_argument('--output-root',type=Path,required=True);a=ap.parse_args()
 exe=a.compiler.resolve();work=a.output_root.resolve()/('c-bridge-map-'+str(uuid.uuid4()));src=work/'源码 😀';src.mkdir(parents=True)
 env=dict(os.environ,PATH=str(exe.parent)+os.pathsep+os.environ.get('PATH',''));protected={str(p):sha(p.read_bytes()) for p in [exe,exe.with_name('xlc.dll'),exe.with_name('xl.dll')]}
 preserved={};results=[];validated=[];header=b'';status='failed';started=datetime.datetime.now(datetime.timezone.utc).isoformat()
 def write(name,text):
  p=src/name;p.write_bytes(text.encode('utf8'));preserved[p]=p.read_bytes();return p
 def run(name,args,expected=0,timeout=120):
  args=list(map(str,args));(work/(name+'.args.json')).write_text(json.dumps(args,ensure_ascii=False,indent=2),encoding='utf8')
  c=subprocess.run(args,cwd=src,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=timeout)
  (work/(name+'.stdout')).write_bytes(c.stdout);(work/(name+'.stderr')).write_bytes(c.stderr);results.append({'name':name,'exitCode':c.returncode})
  assert c.returncode==expected,(name,c.returncode,c.stdout[:1000],c.stderr[:1000]);return c
 def query(name,path,extra=(),expected=0):return json.loads(run(name,[exe,'c-bridge',path,'--json',*extra],expected).stdout.decode('utf-8-sig'))
 def validate(name,v,texts,expected_names=None):
  assert v['ok'] and v['sourceReport']['complete'] and v['cBridge']['complete'],v
  bridge=v['cBridge'];mapping=bridge['declarationMap'];header=bridge['declarations'].encode('utf8')
  assert mapping['schema']==1 and mapping['scope']=='lowered-public-symbol-source-bindings' and mapping['complete'],mapping
  assert mapping['offsetEncoding']=='utf-8-after-bom' and mapping['columnEncoding']=='unicode-scalar-1-based' and mapping['rangeConvention']=='half-open'
  records=mapping['records'];symbols=set();ranges=[];selections=[]
  for row in records:
   start,end=row['headerRange']['startOffset'],row['headerRange']['endOffset'];assert 0<=start<end<=len(header)
   assert header[start:end]==row['cName'].encode('utf8');assert row['symbolId']>0 and row['symbolId'] not in symbols;symbols.add(row['symbolId'])
   assert row['kind'] in ['function','global'];target=row['target'];assert target
   source=v['sourceReport']['files'][target['sourceId']-1];path=Path(source['path']);data=texts[path.name].removeprefix(b'\xef\xbb\xbf')
   assert source['kind']&1 and sha(data)==source['textSha256'] and len(data)==source['textSize'],source
   outer=target['range'];inner=target['selectionRange'];assert outer['startOffset']<=inner['startOffset']<inner['endOffset']<=outer['endOffset']<=len(data)
   for span in [outer,inner]:
    pre=data[:span['startOffset']].decode('utf8');text=data[span['startOffset']:span['endOffset']].decode('utf8');before=pre.split('\n');after=(pre+text).split('\n')
    assert span['startLine']==len(before) and span['startColumn']==len(before[-1])+1,span
    assert span['endLine']==len(after) and span['endColumn']==len(after[-1])+1,span
   selection=data[inner['startOffset']:inner['endOffset']].decode('utf8');selections.append(selection);ranges.append((start,end))
  assert ranges==sorted(ranges) and all(ranges[i-1][1]<=ranges[i][0] for i in range(1,len(ranges)))
  if expected_names is not None:assert sorted(selections)==sorted(expected_names),(name,selections)
  validated.append({'name':name,'records':len(records),'selections':selections});return records
 try:
  main=write('入口.xl','// 中文 😀 preceding UTF-8 offsets\nexport "public_answer" func 计算(c_int n) as c_int\n return n\nend\n')
  v=query('disk-identity',main);validate('disk-identity',v,{'入口.xl':main.read_bytes()},['计算'])
  run('ordinary-header',[exe,'cgen',main,'--emit-header',src/'module.h']);assert (src/'module.h').read_bytes()==v['cBridge']['declarations'].encode('utf8')
  draft=write('draft.snapshot','\ufeff// different 中文 😀\r\n\r\nexport "renamed_abi" func 草稿(c_double n) as c_double\r\n return n\r\nend\r\n')
  v=query('draft-identity',main,['--source-overlay',main,draft]);rows=validate('draft-identity',v,{'入口.xl':draft.read_bytes()},['草稿']);assert rows[0]['cName']=='renamed_abi' and rows[0]['target']['selectionRange']['startLine']==3
  virtual=src/'未创建.xl';v=query('virtual-target',virtual,['--source-overlay',virtual,draft]);validate('virtual-target',v,{'未创建.xl':draft.read_bytes()},['草稿']);assert not virtual.exists()
  helper=write('helper.xl','export "dep_disk" func answer(c_int n) as c_int\n return n\nend\n');entry=write('root.xl','import helper\nexport "entry_public" func call(c_int n) as c_int\n return helper.answer(n)\nend\n')
  v=query('recursive-disk',entry);validate('recursive-disk',v,{'root.xl':entry.read_bytes(),'helper.xl':helper.read_bytes()},['answer','call'])
  overlay=write('helper.snapshot','// 😀\nexport "dep_draft" func answer(c_int n) as c_int\n return n+1\nend\n')
  v=query('recursive-draft',entry,['--source-overlay',helper,overlay]);rows=validate('recursive-draft',v,{'root.xl':entry.read_bytes(),'helper.xl':overlay.read_bytes()},['answer','call']);assert any(row['cName']=='dep_draft' for row in rows) and not any(row['cName']=='dep_disk' for row in rows)
  globals=write('globals.xl','export "counter_abi" int counter = 1\nexport "version_abi" const int version = 7\nexport "get_abi" func read() as int\n return counter+version\nend\n')
  v=query('global-bindings',globals);rows=validate('global-bindings',v,{'globals.xl':globals.read_bytes()},['counter','version','read']);assert sum(row['kind']=='global' for row in rows)==2
  overloaded=write('overloaded.xl','export func add(int value) as int\n return value\nend\nexport func add(bool value) as int\n if value then\n  return 1\n end\n return 0\nend\n')
  v=query('overloaded-identities',overloaded);rows=validate('overloaded-identities',v,{'overloaded.xl':overloaded.read_bytes()},['add','add']);assert len({row['cName'] for row in rows})==2 and len({row['target']['selectionRange']['startOffset'] for row in rows})==2
  external=write('external.xl','extern "native_echo" func nativeEcho(c_int n) as c_int\nexport "forward_echo" func forward(c_int n) as c_int\n return nativeEcho(n)\nend\n')
  v=query('extern-prototype-bindings',external);rows=validate('extern-prototype-bindings',v,{'external.xl':external.read_bytes()},['nativeEcho','forward']);assert {r['cName'] for r in rows}=={'native_echo','forward_echo'}
  external_header=src/'external-output'/'module.h';external_header.parent.mkdir();run('extern-ordinary-header',[exe,'cgen',external,'--emit-header',external_header]);assert external_header.read_bytes()==v['cBridge']['declarations'].encode('utf8')
  external_draft=write('external.snapshot','// 中文 😀 new external name\nextern "changed_echo" func draftEcho(c_int n) as c_int\nexport "forward_draft" func draftForward(c_int n) as c_int\n return draftEcho(n)\nend\n')
  v=query('extern-draft-bindings',external,['--source-overlay',external,external_draft]);rows=validate('extern-draft-bindings',v,{'external.xl':external_draft.read_bytes()},['draftEcho','draftForward']);assert {r['cName'] for r in rows}=={'changed_echo','forward_draft'}
  empty=write('empty.xl','func privateOnly() as int\n return 1\nend\n');v=query('no-public-bindings',empty);assert validate('no-public-bindings',v,{'empty.xl':empty.read_bytes()},[])==[]
  bad=write('bad.snapshot','export "stale" func broken() as int\n return undefined_name\nend\n');v=query('failed-clears-map',main,['--source-overlay',main,bad],1);assert not v['ok'] and not v['cBridge']['complete'] and v['cBridge']['declarationMap']['complete'] is False and not v['cBridge']['declarationMap']['records'] and v['cBridge']['declarations']==''
  v=query('recovery',main);validate('recovery',v,{'入口.xl':main.read_bytes()},['计算'])
  v=query('environment-map',main,['--c-environment']);validate('environment-map',v,{'入口.xl':main.read_bytes()},['计算']);assert v['cBridge']['nativeEnvironment']['complete']
  # One record beyond the optional navigation limit must still yield valid ABI.
  capacity=write('capacity.xl',''.join('export "g%x" c_int v%x = 0\n' % (i,i) for i in range(32769)))
  v=json.loads(run('map-capacity-fallback',[exe,'c-bridge',capacity,'--json'],timeout=540).stdout.decode('utf-8-sig'))
  assert v['ok'] and v['sourceReport']['complete'] and v['cBridge']['complete'],v.get('diagnostics')
  bridge=v['cBridge'];mapping=bridge['declarationMap'];header=bridge['declarations'].encode('utf8')
  assert len(header)<=1024*1024 and header.count(b'XLIR_C_API extern ')==32769,len(header)
  assert not mapping['complete'] and mapping['reason']=='source-map-unavailable' and mapping['records']==[],mapping
  capacity_header=src/'capacity-output'/'module.h';capacity_header.parent.mkdir()
  run('capacity-ordinary-header',[exe,'cgen',capacity,'--emit-header',capacity_header],timeout=540)
  assert capacity_header.read_bytes()==header
  assert all(p.read_bytes()==data for p,data in preserved.items());status='passed'
 finally:
  assert all(sha(Path(path).read_bytes())==digest for path,digest in protected.items())
  evidence={'status':status,'startedUtc':started,'finishedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(exe),'commands':len(results),'validated':validated,'results':results,'protected':protected,'preservedSources':len(preserved),'mapCapacityExports':32769,'mapCapacityHeaderBytes':len(header),'guiNavigationComplete':False}
  (work/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps({'status':status,'commands':len(results),'responsesValidated':len(validated),'mapCapacityExports':32769,'mapCapacityHeaderBytes':len(header),'evidence':str(work/'evidence.json')},ensure_ascii=False))
if __name__=='__main__':main()
