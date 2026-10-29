"""Real cursor-query LSP, whole-token UTF-16 edits and guarded async lifecycle."""
import argparse,copy,ctypes,datetime,hashlib,json,os,shutil,subprocess,time,uuid
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--compiler',type=Path,required=True); ap.add_argument('--server',type=Path); ap.add_argument('--gcc',default='gcc'); a=ap.parse_args()
 ide=Path(__file__).resolve().parents[1]; compiler=a.compiler.resolve(); server=(a.server or (ide/'release/xlang-ls.exe' if ide.parent.name=='build' and ide.name.startswith('cb-') else ide.parent/'release/xlang-ls.exe')).resolve(); work=ide/'build/targeted'/('completions-'+str(uuid.uuid4())); root=work/'中文 补全😀'; root.mkdir(parents=True); entry=root/'main.xl'; dep=root/'dep.xl'; extra=root/'extra.xl'
 original=b'func main(...) as int\n    return 0\nend\n'; entry.write_bytes(original)
 protected={str(p):sha(p) for p in [compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'),ide/'third_party/xge_sdk/xge.dll']}; cases=[]; clients=[]; status='failed'; rid=10; temp_before={p.name for p in Path(os.environ['TEMP']).glob('xide-check-*')}
 def done(name): cases.append({'name':name,'passed':True})
 def client(tool=compiler):
  # Controlled malformed/slow compiler responses exercise only the explicit
  # CLI oracle. Real-source cases use the host's default native backend.
  options={'checkOnSave':False,'automaticDiagnostics':False}
  if tool!=compiler: options['completionBackend']='cli'
  c=Client(server,tool,root); clients.append(c); c.send('initialize',{'capabilities':{},'initializationOptions':options},1); m,_=c.reply(1); cap=m['result']['capabilities']; assert cap['completionProvider']=={'resolveProvider':False,'triggerCharacters':['.']} and cap['experimental']['cursorCompletion'] and not cap['experimental']['cSemantics']; c.send('initialized',{}); return c
 def params(text,at,path=entry,version=1,standard=False,**kwargs):
  p={'textDocument':{'uri':path.as_uri()},'position':position(text,at)}
  if not standard: p['version']=version
  p.update(kwargs); return p
 def request(c,text,at,path=entry,version=1,standard=False,id=None,**kwargs):
  nonlocal rid
  if id is None: id=rid; rid+=1
  c.send('textDocument/completion',params(text,at,path,version,standard,**kwargs),id); return id
 def result(c,id):
  m,seen=c.reply(id); assert 'error' not in m,m; r=m['result']; assert r['queryId']==id and r['scope']=='cursor-query' and r['guardScope']=='background-poll' and not r['checkedCompilation'] and not r['isIncomplete']; assert not any(x.get('method')=='textDocument/publishDiagnostics' and x.get('params',{}).get('diagnostics') for x in seen); return r,seen
 def query(text,needle,at=None,extra_inputs=(),**kwargs):
  c=client(); c.open(entry,text,1)
  for path,body,version in extra_inputs: c.open(path,body,version)
  at=text.rindex(needle)+len(needle) if at is None else at
  r,_=result(c,request(c,text,at,**kwargs)); return c,r,at
 def names(r): return [v['label'] for v in r['items']]
 def cli(text,at,inputs=(),query_path=entry):
  command=[str(compiler),'check',str(entry),'--json','--completion-at',str(query_path),str(len(text[:at].removeprefix('\ufeff').encode('utf-8')))]
  for i,(path,body) in enumerate([(entry,text)]+list(inputs)):
   p=root/f'capture-{i}.utf8'; p.write_bytes(body.encode('utf-8')); command+=['--source-overlay',str(path),str(p)]
  cp=subprocess.run(command,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30); assert cp.returncode in (0,1),(cp.returncode,cp.stderr); return json.loads(cp.stdout)
 def controlled(raw,mode='exit0 completion'):
  (root/'snapshot-response.json').write_text(json.dumps(raw,ensure_ascii=False),encoding='utf-8'); (root/'snapshot-mode.txt').write_text(mode,encoding='utf-8'); (root/'snapshot-signal.txt').unlink(missing_ok=True); return client(fixture)
 def signal():
  deadline=time.monotonic()+10
  while not (root/'snapshot-signal.txt').exists(): assert time.monotonic()<deadline; time.sleep(.005)
 try:
  text='func main(...) as int\n    int result = 7\n    return res\nend\n'; c,r,at=query(text,'res'); assert r['available'] and names(r)==['result']; item=r['items'][0]; assert item['kind']==6 and item['detail']=='int' and item['data']['symbolId']>0 and item['data']['declaration']['uri']==entry.as_uri(); assert item['textEdit']=={'range':{'start':position(text,at-3),'end':position(text,at)},'newText':'result'}; done('unsaved-partial-name-with-real-identity-and-text-edit'); c.stop()
  snap=root/'ordinary.utf8'; snap.write_bytes(text.encode()); normal=subprocess.run([str(compiler),'check',str(entry),'--json','--source-overlay',str(entry),str(snap)],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30); assert normal.returncode==1 and not json.loads(normal.stdout)['ok']; done('query-success-does-not-imply-check-success')
  text='func main(...) as int\n    int result = 7\n    return reOther\nend\n'; at=text.index('reOther')+2; c,r,_=query(text,'',at=at,standard=True); assert names(r)==['result'] and r['items'][0]['textEdit']['range']=={'start':position(text,at-2),'end':position(text,at+5)}; done('standard-request-replaces-whole-token-including-suffix'); c.stop()
  text='\ufefffunc main(...) as int\r\n    string 文本 = "😀"; int 结果 = 7\r\n    return 结后缀\r\nend\r\n'; at=text.index('结后缀')+1; c,r,_=query(text,'',at=at); assert names(r)==['结果'] and r['items'][0]['textEdit']['range']=={'start':position(text,at-1),'end':position(text,at+2)}; done('bom-crlf-non-bmp-and-chinese-replacement'); pos=position(text,text.index('😀')); pos['character']+=1; c.send('textDocument/completion',{'textDocument':{'uri':entry.as_uri()},'position':pos},77); assert c.reply(77)[0]['error']['code']==-32602; done('surrogate-midpoint-rejected'); c.stop()
  klass='class Box\n    int value\n    func getValue() as int\n        return self.value\n    end\nend\nfunc main(...) as int\n    auto box = Box()\n    return box.\nend\n'; c,r,at=query(klass,'box.',context={'triggerKind':2,'triggerCharacter':'.'}); assert set(names(r))=={'value','getValue'} and all(v['textEdit']['range']['start']==v['textEdit']['range']['end']==position(klass,at) for v in r['items']); assert {v['kind'] for v in r['items']}=={2,5}; done('automatic-member-trigger-and-zero-width-edit'); c.stop()
  shadow='int value = 1\nfunc main(...) as int\n    string value = "s"\n    val\n    return 0\nend\n'; c,r,_=query(shadow,'val',at=shadow.index('\n    val\n')+8); assert names(r)==['value'] and r['items'][0]['detail']=='string'; done('live-scope-shadowing-not-global-name-scan'); c.stop()
  overload='func pick(int v) as int\n    return v\nend\nfunc pick(string v) as int\n    return 1\nend\nfunc main(...) as int\n    pic\n    return 0\nend\n'; c,r,_=query(overload,'pic',at=overload.index('\n    pic\n')+8); assert names(r)==['pick','pick'] and len({v['data']['symbolId'] for v in r['items']})==2 and len({v['detail'] for v in r['items']})==2; done('same-name-overloads-retain-distinct-identities'); c.stop()
  deptext='export class Box\n    int value\nend\n'; imported='import dep\nfunc main(...) as int\n    auto box = dep.Box()\n    return box.\nend\n'; c,r,at=query(imported,'box.',extra_inputs=[(dep,deptext,31)]); assert 'value' in names(r) and not dep.exists(); done('unsaved-virtual-import-supplies-real-member-type'); c.stop()
  dependency='export func value() as int\n    int member = 7\n    return mem\nend\n'; roottext='import dep\nfunc main(...) as int\n    return dep.value()\nend\n'; c=client(); c.open(entry,roottext,5); c.open(dep,dependency,31); q=request(c,dependency,dependency.rindex('mem')+3,path=dep,version=31,entryDocument={'uri':entry.as_uri()}); r,_=result(c,q); assert names(r)==['member'] and r['version']==31 and r['items'][0]['data']['declaration']['uri']==dep.as_uri(); done('query-in-unsaved-dependency-keeps-entry-and-query-versions-distinct'); c.stop()
  c=client(); c.open(entry,'\ufeff'+text,1); c.send('textDocument/completion',{'textDocument':{'uri':entry.as_uri()},'position':{'line':0,'character':0}},79); assert c.reply(79)[0]['error']['code']==-32602; done('position-before-source-bom-rejected'); c.stop()
  c=client(); c.open(entry,text,1)
  for n in range(63): c.open(root/f'unused-{n}.xl','int unused = 0\n',n+1)
  r,_=result(c,request(c,text,text.index('结后缀')+1)); assert names(r)==['结果']; c.open(extra,'int unused = 0\n',1); m,_=c.reply(request(c,'int unused = 0\n',4,path=extra)); assert m['error']['code']==-32602; done('64-overlays-use-completion-arguments-without-overflow-and-65th-is-rejected'); c.stop()
  c=client(); c.open(entry,text,1)
  bad=[params(text,0,version=0),{'textDocument':{'uri':extra.as_uri()},'position':{'line':0,'character':0}},params(text,0,context={'triggerKind':2}),{'textDocument':{'uri':entry.as_uri()},'position':{'line':-1,'character':0}}]
  for p in bad: c.send('textDocument/completion',p,80); assert c.reply(80)[0]['error']['code']==-32602
  done('stale-version-unopened-negative-position-and-context-rejected'); c.stop()
  base='func main(...) as int\n    int result = 7\n    return res\nend\n'; at=base.rindex('res')+3; raw=cli(base,at); assert raw['ok'] and raw['completion']['items']
  fixture=root/'fixture-xl.exe'; cp=subprocess.run([a.gcc,'-std=c11','-Wall','-Wextra','-Werror','-O2',str(ide/'tests/snapshot_compiler_program.c'),'-lshell32','-o',str(work/'fixture-ascii.exe')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30); assert cp.returncode==0,cp.stderr; shutil.copy2(work/'fixture-ascii.exe',fixture); done('controlled-fixture-compiled')
  c=controlled(raw); c.open(entry,base); r,_=result(c,request(c,base,at)); assert names(r)==['result']; done('controlled-valid-completion'); c.stop()
  mutations={
   'schema':lambda v:v['completion'].update(schema=2),
   'checked-compilation':lambda v:v['completion'].update(checkedCompilation=True),
   'complete-status':lambda v:v['completion'].update(complete=False),
   'scope':lambda v:v['completion'].update(scope='checked-compilation'),
   'encoding':lambda v:v['completion'].update(offsetEncoding='utf-16'),
   'source-id':lambda v:v['completion'].update(sourceId=9999),
   'query-offset':lambda v:v['completion'].update(offset=at+1),
   'prefix':lambda v:v['completion'].update(prefix='fake'),
   'replacement-column':lambda v:v['completion']['replacementRange'].update(startColumn=9999),
   'replacement-token-suffix':lambda v:v['completion']['replacementRange'].update(endOffset=v['completion']['replacementRange']['endOffset']+1),
   'selection-source':lambda v:v['completion']['items'][0].update(selectionSourceId=9999),
   'selection-column':lambda v:v['completion']['items'][0]['selectionRange'].update(startColumn=9999),
   'candidate-kind':lambda v:v['completion']['items'][0].update(kind='invented'),
   'candidate-prefix':lambda v:v['completion']['items'][0].update(name='other'),
   'candidate-internal-name':lambda v:v['completion']['items'][0].update(name='__xl6_private'),
   'candidate-name-limit':lambda v:v['completion']['items'][0].update(name='res'+'a'*510),
   'candidate-type-limit':lambda v:v['completion']['items'][0].update(canonicalType='x'*8193),
   'candidate-symbol':lambda v:v['completion']['items'][0].update(symbolId=-1),
   'candidate-count':lambda v:v['completion'].update(items=v['completion']['items']*4097),
   'candidate-null-type-field':lambda v:v['completion']['items'][0].pop('canonicalType'),
   'wrong-phase':lambda v:v.update(phase='check'),
   'mixed-diagnostics':lambda v:v.update(diagnostics=[{'message':'invented'}]),
   'source-hash':lambda v:v['sourceReport']['files'][0].update(textSha256='0'*64),
   'missing-resolution':lambda v:v.pop('resolutionReport'),
  }
  for name,modify in mutations.items():
   bad=copy.deepcopy(raw); modify(bad); c=controlled(bad); c.open(entry,base); m,seen=c.reply(request(c,base,at)); assert m['error']['code']==-32603,(name,m); assert not any(x.get('params',{}).get('diagnostics') for x in seen); done('malformed-'+name+'-rejects-entire-query'); c.stop()
  for name,count,name_size,type_size in [('retained-text-limit',513,6,8192),('encoded-frame-limit',4096,500,500)]:
   bad=copy.deepcopy(raw); row=copy.deepcopy(raw['completion']['items'][0]); row['name']='res'+'a'*(name_size-3); row['canonicalType']='x'*type_size; bad['completion']['items']=[row]*count; c=controlled(bad); c.open(entry,base); q=request(c,base,at); m,_=c.reply(q); assert m['error']['code']==-32603; c.send('unknown',{},7654); assert c.reply(7654)[0]['error']['code']==-32601; done(name+'-rejects-query-without-breaking-server'); c.stop()
  # Virtual long paths are genuine accepted overlay inputs; declaration URI
  # repetition must be budgeted before building a very large result.
  long_entry=root.joinpath(*(['目录'*64]*24),'main.xl'); long_raw=copy.deepcopy(raw); long_raw['sourceReport']['files'][0]['path']=str(long_entry); long_raw['completion']['items']*=4096
  class Counters(ctypes.Structure):
   _fields_=[('cb',ctypes.c_ulong),('PageFaultCount',ctypes.c_ulong)]+[(n,ctypes.c_size_t) for n in ['PeakWorkingSetSize','WorkingSetSize','QuotaPeakPagedPoolUsage','QuotaPagedPoolUsage','QuotaPeakNonPagedPoolUsage','QuotaNonPagedPoolUsage','PagefileUsage','PeakPagefileUsage','PrivateUsage']]
  kernel=ctypes.WinDLL('kernel32',use_last_error=True); psapi=ctypes.WinDLL('psapi',use_last_error=True); kernel.OpenProcess.argtypes=[ctypes.c_ulong,ctypes.c_int,ctypes.c_ulong]; kernel.OpenProcess.restype=ctypes.c_void_p; kernel.CloseHandle.argtypes=[ctypes.c_void_p]; psapi.GetProcessMemoryInfo.argtypes=[ctypes.c_void_p,ctypes.POINTER(Counters),ctypes.c_ulong]; psapi.GetProcessMemoryInfo.restype=ctypes.c_int
  c=controlled(long_raw); c.open(long_entry,base); handle=kernel.OpenProcess(0x410,False,c.process.pid); assert handle
  try:
   before= Counters(); before.cb=ctypes.sizeof(before); assert psapi.GetProcessMemoryInfo(handle,ctypes.byref(before),before.cb)
   m,_=c.reply(request(c,base,at,path=long_entry)); assert m['error']['code']==-32603; after=Counters(); after.cb=ctypes.sizeof(after); assert psapi.GetProcessMemoryInfo(handle,ctypes.byref(after),after.cb); growth=after.PeakPagefileUsage-before.PeakPagefileUsage; assert growth<96*1024*1024,growth; c.send('unknown',{},7655); assert c.reply(7655)[0]['error']['code']==-32601; cases.append({'name':'long-declaration-uri-budgeted-during-construction','passed':True,'peakCommitGrowth':growth})
  finally: kernel.CloseHandle(handle); c.stop()
  c=controlled(raw,'completion'); c.open(entry,base); m,_=c.reply(request(c,base,at)); assert m['error']['code']==-32603; done('compiler-exit-status-must-match-query'); c.stop()
  c=controlled(raw,'exit0'); c.open(entry,base); m,_=c.reply(request(c,base,at)); assert m['error']['code']==-32603 and not (root/'snapshot-signal.txt').exists(); done('old-help-rejects-before-starting-query'); c.stop()
  c=controlled(raw,'exit0 completionfake'); c.open(entry,base); m,_=c.reply(request(c,base,at)); assert m['error']['code']==-32603 and not (root/'snapshot-signal.txt').exists(); done('completion-help-token-suffix-is-not-capability'); c.stop()
  old=ide/'build/toolchains/demo6-semantic-862cd3b4-9827-4983-ae59-0f5562ab97bd/xl.exe'
  if old.exists():
   oldhash=sha(old); c=client(old); c.open(entry,base); m,_=c.reply(request(c,base,at)); assert m['error']['code']==-32603 and sha(old)==oldhash; done('real-legacy-compiler-without-completion-keeps-honest-failure'); c.stop()
  unavailable='func main(...) as int\n    return missing.\nend\n'; c,r,_=query(unavailable,'missing.'); assert not r['available'] and not r['items'] and r['error']; done('unavailable-query-is-empty-and-explicit'); c.stop()
  for name,operation,code in [
   ('target-change',lambda c,q:c.change(entry,base+'\n',2),-32801),
   ('unrelated-open',lambda c,q:c.open(extra,'int extra = 0\n',1),-32801),
   ('dependency-close',lambda c,q:c.send('textDocument/didClose',{'textDocument':{'uri':dep.as_uri()}}),-32800),
   ('explicit-cancel',lambda c,q:c.send('$/cancelRequest',{'id':q}),-32800)]:
   c=controlled(raw,'slow exit0 completion'); c.open(entry,base); c.open(dep,'int extra = 0\n',1); q=request(c,base,at); signal(); operation(c,q); m,_=c.reply(q); assert m['error']['code']==code,(name,m); done(name+'-cancels-running-query'); c.stop()
  c=client(); c.open(entry,base); key='请求😀'; r,_=result(c,request(c,base,at,id=key)); assert r['queryId']==key; done('unicode-string-request-id-round-trip'); c.stop()
  c=client(); c.open(entry,base); key='k'*65536; m,_=c.reply(request(c,base,at,id=key)); assert m['error']['code']==-32602; done('oversized-request-id-rejected'); c.stop()
  c=controlled(raw,'slow exit0 completion'); c.open(entry,base); first=request(c,base,at); signal(); second=request(c,base,at); third=request(c,base,at); replies={}
  while len(replies)<3:
   m=c.next()
   if m.get('id') in [first,second,third]: replies[m['id']]=m
  assert replies[first]['error']['code']==replies[second]['error']['code']==-32800 and replies[third]['result']['available']; done('latest-query-wins-with-bounded-queue-and-retirement'); c.stop()
  c=controlled(raw,'slow exit0 completion'); c.open(entry,base); first=request(c,base,at); signal(); waiting=request(c,base,at); c.send('$/cancelRequest',{'id':waiting}); replies={}
  while len(replies)<2:
   m=c.next()
   if m.get('id') in [first,waiting]: replies[m['id']]=m
  assert all(m['error']['code']==-32800 for m in replies.values()); done('queued-request-can-be-cancelled'); c.stop()
  c=controlled(raw,'slow exit0 completion'); c.open(entry,base); q=request(c,base,at); signal(); c.send('unknown',{},1234); before=time.monotonic(); m,_=c.reply(1234); assert m['error']['code']==-32601 and time.monotonic()-before<.5; result(c,q); done('owner-dispatch-stays-responsive-during-query'); c.stop()
  # Completion runs without replacing the successful diagnostic/semantic guard.
  valid='func main(...) as int\n    int result = 7\n    return result\nend\n'; caret=valid.rindex('result')+3; c=client(); c.open(entry,valid); c.check(entry,1,90); q=request(c,valid,caret,id=91); replies={}
  while len(replies)<2:
   m=c.next()
   if m.get('id') in [90,91]: replies[m['id']]=m
  assert replies[90]['result']['semanticDefinitions'] and replies[91]['result']['available']; c.send('textDocument/hover',params(valid,valid.rindex('result'),checkId=90),92); assert c.reply(92)[0]['result']['contents']['value']; done('completion-and-check-coexist-with-independent-semantic-guards'); c.stop()
  dep.write_bytes(b'export class Box\n    int value\nend\n'); disktext='import dep\nfunc main(...) as int\n    auto box = dep.Box()\n    return box.\nend\n'; disk_at=disktext.rindex('box.')+4; diskraw=cli(disktext,disk_at); assert diskraw['ok']
  saved=dep.read_bytes(); c=controlled(diskraw,'slow exit0 completion'); c.open(entry,disktext); q=request(c,disktext,disk_at); signal(); dep.write_bytes(saved+b'\n'); m,_=c.reply(q); assert m['error']['code']==-32603; done('disk-change-during-query-rejects-publication'); c.stop(); dep.write_bytes(saved)
  def invalidation(c,q):
   while True:
    m=c.next()
    if m.get('method')=='xlang/completionInvalidated': assert m['params']['queryId']==q and m['params']['version']==1 and m['params']['uri']==entry.as_uri() and m['params']['reason']; return m
  for name,operation in [
   ('disk-bytes',lambda:dep.write_bytes(saved+b'\n')),
   ('disk-identity',lambda:(root/'replacement.tmp').write_bytes(saved) or None)]:
   c=client(); c.open(entry,disktext); q=request(c,disktext,disk_at); result(c,q)
   if name=='disk-identity': replacement=root/'replacement.tmp'; replacement.write_bytes(saved); os.replace(replacement,dep)
   else: operation()
   invalidation(c,q); done(name+'-invalidates-retained-completion'); c.stop(); dep.write_bytes(saved)
  diskraw=cli(disktext,disk_at); probe=next(v for v in diskraw['resolutionReport']['probes'] if not v['exists'] and Path(v['path']).name=='xlang.package.json' and Path(v['path']).is_relative_to(root)); candidate=Path(probe['path']); c=client(); c.open(entry,disktext); q=request(c,disktext,disk_at); result(c,q); candidate.parent.mkdir(parents=True,exist_ok=True); candidate.write_bytes(b'{}'); invalidation(c,q); candidate.unlink(); done('new-import-candidate-invalidates-retained-completion'); c.stop()
  c=controlled(raw); c.open(entry,base); q=request(c,base,at); result(c,q); toolbytes=fixture.read_bytes(); fixture.write_bytes(toolbytes+b'\0'); invalidation(c,q); done('compiler-identity-invalidates-retained-completion'); c.stop(); fixture.write_bytes(toolbytes)
  c=client(); c.open(entry,base); q=request(c,base,at); result(c,q); c.change(entry,base+'\n',2); invalidation(c,q); done('published-query-invalidated-on-document-change'); q=request(c,base+'\n',at,version=2); r,_=result(c,q); assert r['version']==2; done('fresh-query-recovers-after-invalidation'); c.stop()
  # Independent server restart does not inherit cursor guards.
  c=client(); c.open(entry,base,7); q=request(c,base,at,version=7); r,_=result(c,q); assert r['version']==7; done('fresh-server-restarts-with-new-document-version'); c.stop()
  assert entry.read_bytes()==original
  for path,h in protected.items(): assert sha(Path(path))==h,path
  for c in clients: c.stop()
  leftovers={p.name for p in Path(os.environ['TEMP']).glob('xide-check-*')}-temp_before; assert not leftovers,leftovers; done('source-toolchain-sdk-and-temporary-leases-preserved')
  status='passed'; print(f'PASS {len(cases)} completion cases; evidence: {work}')
 finally:
  for c in clients: c.stop()
  (work/'evidence.json').write_text(json.dumps({'status':status,'cases':cases,'compiler':str(compiler),'server':str(server),'protected':protected,'finished':datetime.datetime.now(datetime.timezone.utc).isoformat()},ensure_ascii=False,indent=2),encoding='utf-8')
  (work/'transcripts.json').write_text(json.dumps([c.transcript for c in clients],ensure_ascii=False,indent=2),encoding='utf-8')
if __name__=='__main__': main()
