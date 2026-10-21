"""Real outline/hover queries from a single checked compilation, never spelling scans."""
import argparse,copy,datetime,hashlib,json,os,shutil,subprocess,time,uuid
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--compiler',type=Path,required=True); ap.add_argument('--server',type=Path); ap.add_argument('--gcc',default='gcc'); a=ap.parse_args()
 ide=Path(__file__).resolve().parents[1]; compiler=a.compiler.resolve(); server=(a.server or ide/'release/xlang-ls.exe').resolve(); work=ide/'build/targeted'/('symbols-'+str(uuid.uuid4())); root=work/'源码😀'; root.mkdir(parents=True); entry=root/'main.xl'; dep=root/'helper.xl'
 protected={str(p):sha(p) for p in [compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'),ide/'third_party/xge_sdk/xge.dll']}; results=[]; clients=[]; rid=10; status='failed'
 def done(name): results.append({'name':name,'passed':True})
 def client(tool=compiler):
  options={'checkOnSave':False}
  if tool!=compiler: options['diagnosticsBackend']='cli'
  c=Client(server,tool,root); clients.append(c); c.send('initialize',{'capabilities':{},'initializationOptions':options},1); m,_=c.reply(1); cap=m['result']['capabilities']; assert cap['hoverProvider'] and cap['documentSymbolProvider'] and cap['experimental']['snapshotHover'] and cap['experimental']['snapshotDocumentSymbols'] and not cap['experimental']['cSemantics']; c.send('initialized',{}); return c
 def check(c,text,version=1,extra=()):
  c.open(entry,text,version)
  for p,t,v in extra: c.open(p,t,v)
  c.check(entry,version,2); m,_=c.reply(2); assert m.get('result',{}).get('semanticDefinitions') and m['result']['count']==0,m
 def query(c,method,text=None,needle=None,path=entry,version=1,batch=2,standard=False,nth=1,params=None):
  nonlocal rid
  if params is None:
   params={'textDocument':{'uri':path.as_uri()}}
   if text is not None:
    at=-1
    for _ in range(nth): at=text.index(needle,at+1)
    params['position']=position(text,at)
   if not standard: params.update(version=version,checkId=batch)
  c.send('textDocument/'+method,params,rid); m,_=c.reply(rid); rid+=1; return m
 def hover(c,text,needle,**kwargs):
  m=query(c,'hover',text,needle,**kwargs); assert 'error' not in m,m; r=m['result']; assert r['contents']['kind']=='plaintext' and r['contents']['value'] and r['range']['start']!=r['range']['end'],m; return r
 def outline(c,**kwargs):
  m=query(c,'documentSymbol',**kwargs); assert isinstance(m.get('result'),list),m; return m['result']
 def flatten(items):
  return [s for v in items for s in [v]+flatten(v['children'])]
 try:
  text='int 值 = 9\nfunc 计算(int 值) as int\n    if true\n        auto 值 = 2\n        assert 值 == 2\n    end\n    return 值\nend\nfunc main(...) as int\n    return 计算(值) - 9\nend\n'
  entry.write_text(text,encoding='utf-8',newline='\n'); original=entry.read_bytes(); c=client(); check(c,text)
  h=hover(c,text,'值',nth=4); assert 'auto 值 = 2' in h['contents']['value'] and '类型（推导）：int' in h['contents']['value']; done('shadowed-local-inferred-type-and-actual-source-header')
  h=hover(c,text,'值',nth=5); assert '类型（推导）：int' in h['contents']['value'] and 'int 值' in h['contents']['value']; done('parameter-identity-and-type')
  h=hover(c,text,'值',nth=6,standard=True); assert 'int 值 = 9' in h['contents']['value']; done('standard-hover-current-global')
  items=outline(c,standard=True); assert [v['name'] for v in items]==['值','计算','main']; assert [v['kind'] for v in items]==[13,12,12] and all(not v['children'] for v in items); done('source-order-outline-excludes-local-parameter-and-name-duplicates')
  assert items[1]['selectionRange']['start']==position(text,text.index('计算')); done('unicode-outline-selection-original-source')
  m=query(c,'hover',params={'textDocument':{'uri':entry.as_uri()},'position':position(text,len(text))}); assert m['result'] is None,m; done('end-of-document-has-no-invented-hover'); c.stop()
  overload='func pick(int value) as int\n    return value\nend\nfunc pick(string value) as int\n    return 2\nend\nfunc main(...) as int\n    auto first = pick(1)\n    auto second = pick("😀")\n    return first + second - 3\nend\n'
  c=client(); check(c,overload); assert 'pick(int value)' in hover(c,overload,'pick',nth=3)['contents']['value']; assert 'pick(string value)' in hover(c,overload,'pick',nth=4)['contents']['value']; assert [v['name'] for v in outline(c)]==['pick','pick','main']; done('actual-resolved-overload-hover-and-separate-outline-items'); assert 'func pick(string value) as int' in hover(c,overload,'pick',nth=2)['contents']['value']; done('declaration-name-hover-preserves-full-function-header'); c.stop()
  klass='class Box\n    int value\n    create(int value)\n        self.value = value\n    end\n    func getValue() as int\n        return self.value\n    end\nend\nfunc main(...) as int\n    auto box = Box(3)\n    return box.getValue() - 3\nend\n'
  c=client(); check(c,klass); items=outline(c); box=next(v for v in items if v['name']=='Box'); assert box['kind']==5 and [v['name'] for v in box['children']]==['getValue'] and box['children'][0]['kind']==6; assert 'func getValue() as int' in hover(c,klass,'getValue',nth=2)['contents']['value']; done('class-method-hierarchy-and-real-member-target'); c.stop()
  alias='typedef 数字 = int\nfunc main(...) as int\n    数字 值 = 4\n    return 值 - 4\nend\n'; c=client(); check(c,alias); assert outline(c)[0]['name']=='数字' and outline(c)[0]['kind']==26; assert 'int' in hover(c,alias,'值',nth=2)['contents']['value']; done('type-alias-outline-and-resolved-variable-type'); c.stop()
  unicode='\ufeffint 值 = 4\r\nfunc main(...) as int\r\n    string 文本 = "😀"; auto 结果 = 值\r\n    return 结果 - 4\r\nend\r\n'; c=client(); check(c,unicode); items=outline(c); assert items[0]['selectionRange']['start']=={'line':0,'character':5}; h=hover(c,unicode,'结果',nth=2); assert 'int' in h['contents']['value'] and h['range']['start']==position(unicode,unicode.rindex('结果')); done('bom-crlf-chinese-and-non-bmp-utf16')
  pos=position(unicode,unicode.index('😀')); pos['character']+=1; m=query(c,'hover',params={'textDocument':{'uri':entry.as_uri()},'position':pos}); assert m['error']['code']==-32602; done('surrogate-midpoint-rejected'); c.stop()
  helper='export "internal_answer" func answer() as int\r\n    return 42\r\nend\r\n'; dep.write_bytes(b'\xef\xbb\xbf'+helper.encode()); imported='import helper\nfunc main(...) as int\n    return helper.answer() - 42\nend\n'; c=client(); check(c,imported)
  h=hover(c,imported,'answer'); assert 'func answer() as int' in h['contents']['value'] and '__xl6_import' not in h['contents']['value']; assert [v['name'] for v in outline(c)]==['main']; done('disk-import-original-name-hover-and-document-scoped-outline'); c.stop()
  overlay='\n\nexport func answer() as int\n    return 42\nend\n'; c=client(); check(c,imported,7,[(dep,overlay,11)]); items=outline(c,path=dep,version=11); assert [v['name'] for v in items]==['answer'] and items[0]['selectionRange']['start']['line']==2; assert 'answer' in hover(c,imported,'answer',version=7)['contents']['value']; done('unsaved-imported-document-outline-and-root-hover')
  for method in ['hover','documentSymbol']:
   params={'textDocument':{'uri':entry.as_uri()},'version':6,'checkId':2}
   if method=='hover': params['position']=position(imported,imported.index('answer'))
   assert query(c,method,params=params)['error']['code']==-32602; params['version']=7; params['checkId']=999; assert query(c,method,params=params)['error']['code']==-32801
  done('both-queries-reject-wrong-version-and-batch')
  c.change(dep,'\n'+overlay,12)
  for method in ['hover','documentSymbol']:
   assert query(c,method,imported if method=='hover' else None,'answer',version=7)['error']['code']==-32801
  done('dependency-edit-retires-both-semantic-queries'); c.stop()
  c=client(); check(c,imported); replacement=root/'replacement.tmp'; replacement.write_bytes(dep.read_bytes()); os.replace(replacement,dep)
  until=time.monotonic()+8
  while True:
   m=c.next()
   if m.get('method')=='xlang/diagnosticsInvalidated': break
   assert time.monotonic()<until
  assert query(c,'documentSymbol')['error']['code']==-32801 and query(c,'hover',imported,'answer')['error']['code']==-32801; done('disk-identity-replacement-retires-both-queries'); c.stop()
  c=client(); c.open(entry,'func main(...) as int\n    return "bad"\nend\n'); c.check(entry,1); m,_=c.reply(2); assert m['result']['count']==1 and not m['result']['semanticDefinitions']; assert query(c,'documentSymbol')['error']['code']==-32801; done('failed-check-declines-outline-and-type-information'); c.stop()
  # Controlled mutations retain genuine source reports and resolution guards.
  snapshot=root/'entry.utf8'; snapshot.write_bytes(overload.encode()); cp=subprocess.run([str(compiler),'check',str(entry),'--json','--semantic-index','--source-overlay',str(entry),str(snapshot)],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30); assert cp.returncode==0,cp.stderr; raw=json.loads(cp.stdout)
  fixture=root/'fixture-xl.exe'; cp=subprocess.run([a.gcc,'-std=c11','-Wall','-Wextra','-Werror','-O2',str(ide/'tests/snapshot_compiler_program.c'),'-lshell32','-o',str(work/'fixture.exe')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30); assert cp.returncode==0,cp.stderr; shutil.copy2(work/'fixture.exe',fixture)
  def controlled(value,mode='indexed exit0'):
   (root/'snapshot-response.json').write_text(json.dumps(value,ensure_ascii=False),encoding='utf-8'); (root/'snapshot-mode.txt').write_text(mode); c=client(fixture); c.open(entry,overload); c.check(entry,1); m,_=c.reply(2); return c,m
  c,m=controlled(raw); assert m['result']['semanticDefinitions']; stamp=(root/'snapshot-signal.txt').stat().st_mtime_ns; hover(c,overload,'pick',nth=4); outline(c); assert (root/'snapshot-signal.txt').stat().st_mtime_ns==stamp; done('hover-and-outline-reuse-index-without-compiling-again'); c.stop()
  typed=next(i for i,n in enumerate(raw['semanticIndex']['nodes']) if n['type'])
  mutations={
   'type-origin':lambda n:n['type'].update(origin='invented'),
   'type-identity':lambda n:n['type'].update(id=-1),
   'type-abi':lambda n:n['type'].update(abiId='invalid'),
   'type-canonical-limit':lambda n:n['type'].update(canonical='x'*8193),
   'missing-type-contract':lambda n:n.pop('type'),
   'type-object-kind':lambda n:n.update(type=[]),
  }
  for name,change in mutations.items():
   value=copy.deepcopy(raw); change(value['semanticIndex']['nodes'][typed]); c,m=controlled(value); assert m['error']['code']==-32603,(name,m); done('malformed-'+name+'-rejects-entire-index'); c.stop()
  c,m=controlled(raw,'exit0'); assert not m['result']['semanticDefinitions']; assert query(c,'documentSymbol')['error']['code']==-32801 and query(c,'hover',overload,'pick')['error']['code']==-32801; done('legacy-compiler-keeps-diagnostics-without-advertising-facts'); c.stop()
  assert entry.read_bytes()==original
  for p,h in protected.items(): assert sha(Path(p))==h,p
  status='passed'; print(f'PASS {len(results)} outline/hover cases; evidence: {work}')
 finally:
  for c in clients: c.stop()
  (work/'evidence.json').write_text(json.dumps({'status':status,'cases':results,'compiler':str(compiler),'server':str(server),'protected':protected,'finished':datetime.datetime.now(datetime.timezone.utc).isoformat()},ensure_ascii=False,indent=2),encoding='utf-8')
if __name__=='__main__': main()
