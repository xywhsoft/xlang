"""Real checked-compilation definition queries, stale batches and hostile ranges."""
import argparse,copy,datetime,hashlib,json,os,shutil,subprocess,time,uuid
from pathlib import Path
from check_language_snapshots import Client

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def position(text,at):
 prefix=text[:at].replace('\r\n','\n').replace('\r','\n'); pieces=prefix.split('\n')
 return {'line':len(pieces)-1,'character':len(pieces[-1].encode('utf-16-le'))//2}
def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--compiler',type=Path,required=True); ap.add_argument('--server',type=Path); ap.add_argument('--gcc',default='gcc'); args=ap.parse_args()
 ide=Path(__file__).resolve().parents[1]; compiler=args.compiler.resolve(); server=(args.server or ide/'release/xlang-ls.exe').resolve()
 work=ide/'build/targeted'/('definitions-'+str(uuid.uuid4())); root=work/'源码😀'; root.mkdir(parents=True); entry=root/'main.xl'; dep=root/'helper.xl'
 protected={str(p):sha(p) for p in [compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'),ide/'third_party/xge_sdk/xge.dll']}
 results=[]; clients=[]; status='failed'; start=datetime.datetime.now(datetime.timezone.utc).isoformat(); rid=10
 def done(name,**data): results.append({'name':name,'passed':True,**data})
 def client(tool=compiler):
  c=Client(server,tool,root); clients.append(c); c.send('initialize',{'capabilities':{},'initializationOptions':{'checkOnSave':False}},1); m,_=c.reply(1); cap=m['result']['capabilities']; assert cap['definitionProvider'] and cap['experimental']['snapshotDefinition'] and not cap['experimental']['cSemantics']; c.send('initialized',{}); return c
 def check(c,text,version=1,extra=()):
  c.open(entry,text,version)
  for p,t,v in extra: c.open(p,t,v)
  c.check(entry,version,2); m,_=c.reply(2); assert 'error' not in m,m; assert m['result']['count']==0 and m['result']['semanticDefinitions'],m; return m['result']
 def query(c,text,needle,expected=None,path=entry,version=1,batch=2,standard=False,nth=1,offset=0):
  nonlocal rid
  at=-1
  for _ in range(nth): at=text.index(needle,at+1)
  params={'textDocument':{'uri':path.as_uri()},'position':position(text,at+offset)}
  if not standard: params.update(version=version,checkId=batch)
  c.send('textDocument/definition',params,rid); m,_=c.reply(rid); rid+=1
  if expected is not None: assert m.get('result')==expected,(params,m,expected)
  return m
 def location(path,text,needle,nth=1):
  at=-1
  for _ in range(nth): at=text.index(needle,at+1)
  return {'uri':path.as_uri(),'range':{'start':position(text,at),'end':position(text,at+len(needle))}}
 def error(c,params,code):
  nonlocal rid
  c.send('textDocument/definition',params,rid); m,_=c.reply(rid); rid+=1; assert m['error']['code']==code,m
 try:
  shadow='int 值 = 9\nfunc 计算(int 值) as int\n    if true\n        auto 值 = 2\n        assert 值 == 2\n    end\n    return 值\nend\nfunc main(...) as int\n    return 计算(值) - 9\nend\n'
  entry.write_text(shadow,encoding='utf-8',newline='\n'); original=entry.read_bytes(); c=client(); check(c,shadow)
  for nth,target in [(4,3),(5,2),(6,1)]: query(c,shadow,'值',location(entry,shadow,'值',target),nth=nth)
  query(c,shadow,'计算',location(entry,shadow,'计算'),nth=2,standard=True); done('real-shadowed-local-parameter-global-and-standard-lsp'); c.stop()
  overload='func pick(int value) as int\n    return value\nend\nfunc pick(string value) as int\n    return 2\nend\nfunc main(...) as int\n    auto first = pick(1)\n    auto second = pick("😀")\n    return first + second - 3\nend\n'
  c=client(); check(c,overload); query(c,overload,'pick',location(entry,overload,'pick'),nth=3); query(c,overload,'pick',location(entry,overload,'pick',2),nth=4); query(c,overload,'pick',location(entry,overload,'pick',2),nth=4,offset=4); query(c,overload,'pick',location(entry,overload,'pick',2),nth=2); done('resolved-overload-target-callee-parenthesis-and-declaration'); c.stop()
  unicode='int 值 = 4\r\nfunc main(...) as int\r\n    string 文本 = "😀"; auto 结果 = 值\r\n    return 结果 - 4\r\nend\r\n'
  c=client(); check(c,unicode); query(c,unicode,'值',location(entry,unicode,'值'),nth=2); query(c,unicode,'结果',location(entry,unicode,'结果'),nth=2)
  p=position(unicode,unicode.index('😀')); p['character']+=1; error(c,{'textDocument':{'uri':entry.as_uri()},'position':p,'version':1,'checkId':2},-32602); done('crlf-chinese-non-bmp-utf16-and-surrogate-rejection'); c.stop()
  fstring='int 值 = 4\nfunc main(...) as int\n    auto 文本 = f"😀={值}"\n    return 值 - 4\nend\n'
  c=client(); check(c,fstring); query(c,fstring,'值',location(entry,fstring,'值'),nth=2); done('interpolated-reference-original-token-range'); c.stop()
  bom_text='\ufeff'+unicode; c=client(); check(c,bom_text); query(c,bom_text,'值',location(entry,bom_text,'值'),nth=2); done('overlay-bom-position-in-document-text'); c.stop()
  helper='export "renamed_answer" func answer() as int\r\n    int 值 = 42\r\n    return 值\r\nend\r\n'; dep.write_bytes(b'\xef\xbb\xbf'+helper.encode()); root_text='import helper\nfunc main(...) as int\n    return helper.answer() - 42\nend\n'
  c=client(); check(c,root_text); query(c,root_text,'answer',location(dep,helper,'answer',2)); done('bom-crlf-disk-import-original-export-name');
  # The retained input guard makes a same-byte replacement a stale compilation.
  replacement=root/'replacement.tmp'; replacement.write_bytes(dep.read_bytes()); os.replace(replacement,dep); deadline=time.monotonic()+8
  while True:
   m=c.next()
   if m.get('method')=='xlang/diagnosticsInvalidated': break
   assert time.monotonic()<deadline,m
  error(c,{'textDocument':{'uri':entry.as_uri()},'position':position(root_text,root_text.index('answer')),'version':1,'checkId':2},-32801); done('published-disk-identity-replacement-retires-definition'); c.stop()
  overlay='\n\nexport func answer() as int\n    int 值 = 42\n    return 值\nend\n'
  c=client(); check(c,root_text,7,[(dep,overlay,11)]); query(c,root_text,'answer',location(dep,overlay,'answer'),version=7); query(c,overlay,'值',location(dep,overlay,'值'),path=dep,version=11,nth=2); done('unsaved-module-definition-and-dependency-origin-query')
  error(c,{'textDocument':{'uri':entry.as_uri()},'position':{'line':2,'character':18},'version':6,'checkId':2},-32602)
  error(c,{'textDocument':{'uri':entry.as_uri()},'position':{'line':2,'character':18},'version':7,'checkId':999},-32801)
  error(c,{'textDocument':{'uri':(root/'unopened.xl').as_uri()},'position':{'line':0,'character':0},'version':1,'checkId':2},-32602); done('wrong-version-batch-and-unopened-uri-rejected')
  c.change(dep,'\n'+overlay,12); error(c,{'textDocument':{'uri':entry.as_uri()},'position':position(root_text,root_text.index('answer')),'version':7,'checkId':2},-32801)
  c.check(entry,7,3); m,_=c.reply(3); assert m['result']['semanticDefinitions']; query(c,root_text,'answer',location(dep,'\n'+overlay,'answer'),version=7,batch=3); done('dependency-edit-invalidates-and-new-batch-restores'); c.stop()
  c=client(); check(c,root_text,1,[(dep,overlay,2)]); c.send('textDocument/didClose',{'textDocument':{'uri':dep.as_uri()}}); error(c,{'textDocument':{'uri':entry.as_uri()},'position':{'line':2,'character':18},'version':1,'checkId':2},-32801); done('dependency-close-invalidates-definition'); c.stop()
  c=client(); check(c,shadow); m=query(c,shadow,'9',standard=True); assert m['result'] is None; done('unknown-literal-keeps-null-definition'); c.stop()
  c=client(); c.open(entry,'func main(...) as int\n    return "bad"\nend\n'); c.check(entry,1); m,_=c.reply(2); assert m['result']['count']==1 and not m['result']['semanticDefinitions']; error(c,{'textDocument':{'uri':entry.as_uri()},'position':{'line':0,'character':5}},-32801); done('failed-check-publishes-diagnostics-without-semantic-facts'); c.stop()
  # Controlled payloads keep genuine source reports, alter only index contracts.
  snap=root/'entry.utf8'; snap.write_bytes(overload.encode()); cp=subprocess.run([str(compiler),'check',str(entry),'--json','--semantic-index','--source-overlay',str(entry),str(snap)],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30); assert cp.returncode==0,cp.stderr; raw=json.loads(cp.stdout)
  fixture=root/'fixture-xl.exe'; cp=subprocess.run([args.gcc,'-std=c11','-Wall','-Wextra','-Werror','-O2',str(ide/'tests/snapshot_compiler_program.c'),'-lshell32','-o',str(work/'fixture.exe')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30); assert cp.returncode==0,cp.stderr; shutil.copy2(work/'fixture.exe',fixture)
  def controlled(value,mode='indexed exit0'):
   (root/'snapshot-response.json').write_text(json.dumps(value,ensure_ascii=False),encoding='utf-8'); (root/'snapshot-mode.txt').write_text(mode); c=client(fixture); c.open(entry,overload); c.check(entry,1); m,seen=c.reply(2); return c,m,seen
  c,m,_=controlled(raw); assert m['result']['semanticDefinitions'],m; before=(root/'snapshot-signal.txt').stat().st_mtime_ns; query(c,overload,'pick',location(entry,overload,'pick',2),nth=4); assert before==(root/'snapshot-signal.txt').stat().st_mtime_ns; done('query-reuses-checked-compilation-without-compiler-rerun'); c.stop()
  def alter(fn): v=copy.deepcopy(raw); fn(v['semanticIndex']); return v
  malformed={
   'missing-index':{k:v for k,v in raw.items() if k!='semanticIndex'},
   'incomplete-success':alter(lambda x:x.update(complete=False)),
   'wrong-schema':alter(lambda x:x.update(schema=2)),
   'wrong-offset-encoding':alter(lambda x:x.update(offsetEncoding='utf-16')),
   'wrong-parent':alter(lambda x:x['nodes'][1].update(parent=2)),
   'wrong-id':alter(lambda x:x['nodes'][0].update(id=2)),
   'negative-symbol':alter(lambda x:x['nodes'][1].update(targetSymbolId=-1)),
   'declaration-identity-mismatch':alter(lambda x:x['nodes'][1].update(declaredSymbolId=999)),
   'foreign-source':alter(lambda x:x['nodes'][1].update(sourceId=999)),
   'null-source-with-range':alter(lambda x:x['nodes'][1].update(sourceId=0)),
   'wrong-byte-offset':alter(lambda x:x['nodes'][1]['selectionRange'].update(startOffset=6)),
   'wrong-scalar-coordinate':alter(lambda x:x['nodes'][1]['selectionRange'].update(startColumn=7)),
   'reversed-range':alter(lambda x:x['nodes'][1]['selectionRange'].update(endOffset=0)),
   'missing-range-field':alter(lambda x:x['nodes'][1].pop('tokenRange')),
   'duplicate-declaration':alter(lambda x:x['nodes'][2].update(declaredSymbolId=1,symbolId=1)),
   'oversized-node-array':alter(lambda x:x.update(nodes=[None]*65537)),
  }
  for name,value in malformed.items():
   c,m,seen=controlled(value); assert m['error']['code']==-32603 and not any(x.get('params',{}).get('diagnostics') for x in seen),(name,m); done(name); c.stop()
  c,m,_=controlled(raw,'exit0'); assert not m['result']['semanticDefinitions']; error(c,{'textDocument':{'uri':entry.as_uri()},'position':{'line':7,'character':18}},-32801); done('legacy-compiler-keeps-diagnostics-and-declines-definition'); c.stop()
  assert entry.read_bytes()==original
  for p,h in protected.items(): assert sha(Path(p))==h,p
  done('original-source-and-frozen-toolchain-preserved'); status='passed'
 finally:
  for i,c in enumerate(clients):
   c.stop(); (work/f'transcript-{i:02}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8'); (work/f'stderr-{i:02}.txt').write_text(c.stderr,encoding='utf-8')
  (work/'evidence.json').write_text(json.dumps({'status':status,'started':start,'finished':datetime.datetime.now(datetime.timezone.utc).isoformat(),'server':str(server),'compiler':str(compiler),'results':results,'protected':protected},ensure_ascii=False,indent=2),encoding='utf-8')
  print(f'{status}: {len(results)} definition cases; {work}')
if __name__=='__main__': main()
