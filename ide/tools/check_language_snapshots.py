"""Real stdio snapshot checks and hostile compiler-response contract tests."""
import argparse,copy,datetime,hashlib,json,os,queue,re,shutil,subprocess,threading,time,uuid
from pathlib import Path
ENTRY="import dep\nfunc main(...) as int\n    return dep.value()\nend\n"
GOOD="export func value() as int\n    return 7\nend\n"
BAD="export func value() as int\n    return \"bad\"\nend\n"
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
class Client:
 def __init__(self,server,compiler,root):
  self.process=subprocess.Popen([str(server),'--compiler',str(compiler)],cwd=root,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  self.messages=queue.Queue();self.transcript=[];self.stderr=None
  def reader():
   try:
    while True:
     line=self.process.stdout.readline()
     if not line:break
     assert line.startswith(b'Content-Length: '),line
     length=int(line.split(b':',1)[1]);assert self.process.stdout.readline()==b'\r\n'
     data=self.process.stdout.read(length);assert len(data)==length
     self.messages.put(json.loads(data))
   except BaseException as exc:self.messages.put(exc)
   self.messages.put(EOFError('server ended'))
  self.thread=threading.Thread(target=reader,daemon=True);self.thread.start()
 def send(self,method,params=None,id=None):
  message={'jsonrpc':'2.0','method':method}
  if params is not None:message['params']=params
  if id is not None:message['id']=id
  self.transcript.append({'send':message});data=json.dumps(message,ensure_ascii=False,separators=(',',':')).encode()
  self.process.stdin.write(f'Content-Length: {len(data)}\r\n\r\n'.encode()+data);self.process.stdin.flush()
 def next(self):
  m=self.messages.get(timeout=20)
  if isinstance(m,BaseException):raise m
  self.transcript.append({'receive':m});return m
 def reply(self,id):
  seen=[]
  while True:
   m=self.next();seen.append(m)
   if m.get('id')==id:return m,seen
 def initialize(self):
  self.send('initialize',{'capabilities':{},'initializationOptions':{'checkOnSave':False}},1)
  m,_=self.reply(1);cap=m['result']['capabilities'];assert cap['positionEncoding']=='utf-16' and cap['experimental']['unsavedDependencies'] and cap['experimental']['snapshotInputGuard']=='background-poll';self.send('initialized',{})
 def open(self,path,text,version=1):self.send('textDocument/didOpen',{'textDocument':{'uri':path.as_uri(),'text':text,'version':version,'languageId':'xlang'}})
 def change(self,path,text,version):self.send('textDocument/didChange',{'textDocument':{'uri':path.as_uri(),'version':version},'contentChanges':[{'text':text}]})
 def check(self,path,version,id=2,method='xlang/check'):self.send(method,{'textDocument':{'uri':path.as_uri()},'version':version},id)
 def stop(self):
  if self.stderr is not None:return self.stderr
  if self.process.poll() is None:
   try:self.send('shutdown',{},99999);self.reply(99999);self.send('exit');self.process.stdin.close();self.process.wait(timeout=5)
   except BaseException:self.process.kill();self.process.wait(timeout=5)
  self.thread.join(timeout=2);self.stderr=self.process.stderr.read().decode('utf-8','replace');return self.stderr
def main():
 p=argparse.ArgumentParser();p.add_argument('--compiler',type=Path,required=True);p.add_argument('--server',type=Path);p.add_argument('--gcc',default='gcc');args=p.parse_args();ide=Path(__file__).resolve().parents[1];server=(args.server or ide/'release/xlang-ls.exe').resolve();compiler=args.compiler.resolve();work=ide/'build/targeted'/('snapshots-'+str(uuid.uuid4()));work.mkdir(parents=True)
 root=work/'中文 快照😀';root.mkdir();entry=root/'main.xl';dep=root/'dep.xl';virtual=root/'virtual.xl';extra=root/'extra.xl';disk=root/'disk.xl'
 entry.write_text('func main(...) as int\n    return 0\nend\n',encoding='utf-8');dep.write_text(GOOD,encoding='utf-8');disk.write_bytes(b'\xef\xbb\xbf'+GOOD.replace('\n','\r\n').encode());source_before={str(x):digest(x) for x in [entry,dep,disk]};tool_before={str(x):digest(x) for x in [compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'),ide/'third_party/xge_sdk/xge.dll']};temp_before={x.name for x in Path(os.environ['TEMP']).glob('xide-check-*')}
 results=[];clients=[];start=datetime.datetime.now(datetime.timezone.utc).isoformat();status='failed'
 def done(name,**fields):results.append({'name':name,'passed':True,**fields})
 def client(tool=compiler):
  c=Client(server,tool,root);clients.append(c);c.initialize();return c
 def result(c,id=2):
  m,seen=c.reply(id);assert 'error' not in m,m
  r=m['result'];assert r['scope']=='entry-with-open-overlays' and r['guardScope']=='background-poll';return r,seen
 def cli(texts):
  command=[str(compiler),'check',str(entry),'--json','--source-report']
  for i,(path,text) in enumerate(texts):
   snap=root/f'capture-{i}.utf8';snap.write_bytes(text.encode('utf-8'));command+=['--source-overlay',str(path),str(snap)]
  cp=subprocess.run(command,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30);assert cp.returncode in (0,1),(cp.returncode,cp.stderr);return json.loads(cp.stdout)
 def response(raw,mode='exit1'):
  (root/'snapshot-response.json').write_text(json.dumps(raw,ensure_ascii=False,separators=(',',':')),encoding='utf-8');(root/'snapshot-mode.txt').write_text(mode,encoding='utf-8');(root/'snapshot-signal.txt').unlink(missing_ok=True)
 def signal():
  deadline=time.monotonic()+10
  while not (root/'snapshot-signal.txt').exists():
   assert time.monotonic()<deadline;time.sleep(.005)
 try:
  c=client();c.open(entry,ENTRY,11);c.open(dep,BAD,27);c.check(entry,11);r,seen=result(c)
  assert r['count']==1 and len(r['documents'])==2;assert r['diagnostics'][0]['file'].lower().endswith('/dep.xl');assert any(m.get('params',{}).get('uri')==dep.as_uri() and m['params']['version']==27 and m['params']['diagnostics'] for m in seen);done('dirty-entry-and-dependency',count=r['count'])
  c.change(dep,GOOD,28);c.send('unknown',{},3);_,clears=c.reply(3);assert {entry.as_uri(),dep.as_uri()} <= {m.get('params',{}).get('uri') for m in clears if m.get('method')=='textDocument/publishDiagnostics' and not m['params']['diagnostics']};assert any(m.get('params',{}).get('uri')==dep.as_uri() and m['params']['version']==28 for m in clears);done('dependency-change-clears-all-groups')
  c.check(entry,11,4);r,_=result(c,4);assert r['count']==0;done('fixed-unsaved-dependency');c.stop()
  c=client();c.open(entry,ENTRY,1);c.open(virtual,BAD,9);c.open(dep,'import virtual\nexport func value() as int\n    return virtual.value()\nend\n',2);c.check(entry,1);r,_=result(c);assert r['count']==1 and r['diagnostics'][0]['file'].lower().endswith('/virtual.xl') and not virtual.exists();done('recursive-virtual-import-diagnostic');c.stop()
  # Two identical errors stay attached to distinct overlay files.
  left=root/'left.xl';right=root/'right.xl';two='import left\nimport right\nfunc main(...) as int\n    return left.value()+right.value()\nend\n';c=client();c.open(entry,two,1);c.open(left,BAD,3);c.open(right,BAD,4);c.check(entry,1);r,_=result(c);assert r['count']==2 and {Path(d['file']).name for d in r['diagnostics']}=={'left.xl','right.xl'};done('same-error-two-files');c.stop()
  c=client();c.open(entry,'import disk\nfunc main(...) as int\n    return disk.value()\nend\n',1);c.check(entry,1);r,seen=result(c);assert r['count']==0 and any(m.get('params',{}).get('uri')==disk.as_uri() and 'version' not in m['params'] for m in seen);done('unopened-bom-crlf-disk-input');c.stop()
  unicode_bad='export func value() as int\n    string 中文😀 = missingName\n    return 7\nend\n'
  # Identifier grammar may reject emoji; an emoji in a preceding string tests scalar columns.
  unicode_bad='export func value() as int\n    string 中文 = "😀"; int x = missingName\n    return 7\nend\n';raw=cli([(entry,ENTRY),(dep,unicode_bad)]);assert raw['diagnostics'];expected=raw['diagnostics'][0];c=client();c.open(entry,ENTRY,1);c.open(dep,unicode_bad,5);c.check(entry,1);r,_=result(c);d=r['diagnostics'][0];prefix=unicode_bad.splitlines()[expected['line']-1][:expected['column']-1];assert d['range']['start']=={'line':expected['line']-1,'character':len(prefix.encode('utf-16-le'))//2};done('cross-file-unicode-utf16-position');c.stop()
  # Unopened disk diagnostics use that file's BOM/CRLF text and omit version.
  saved=disk.read_bytes();disk.write_bytes(b'\xef\xbb\xbf'+unicode_bad.replace('\n','\r\n').encode());c=client();c.open(entry,'import disk\nfunc main(...) as int\n    return disk.value()\nend\n',1);c.check(entry,1);r,seen=result(c);assert r['count'] and all(Path(x['file']).resolve()==disk.resolve() for x in r['diagnostics']);assert any(m.get('params',{}).get('uri')==disk.as_uri() and m['params']['diagnostics'] and 'version' not in m['params'] for m in seen);done('unopened-disk-unicode-diagnostic');c.stop();disk.write_bytes(saved)
  c=client();c.open(entry,'func main(...) as int\n    return 0\nend\n',1)
  for i in range(63):c.open(root/f'unused-{i}.xl',GOOD,i+1)
  c.check(entry,1);r,_=result(c);assert len(r['documents'])==64 and not r['count'];c.open(extra,GOOD,1);c.check(extra,1,3);m,_=c.reply(3);assert m['error']['code']==-32602;done('64-open-documents-and-limit');c.stop()
  # Protocol rejects duplicate physical/lexical aliases and stale versions.
  c=client();c.open(entry,ENTRY,1);c.open(dep,BAD,2);alias=Path(str(dep).replace('dep.xl','DEP.XL'));c.open(alias,GOOD,1);c.check(alias,1);m,_=c.reply(2);assert m['error']['code']==-32602;done('duplicate-path-alias-rejected');c.check(entry,99,3);m,_=c.reply(3);assert m['error']['code']==-32602;done('stale-request-version-rejected');c.stop()
  # Publication retains the exact opened URI, including percent-escape spelling.
  c=client();opened=re.sub(r'%[0-9A-F]{2}',lambda m:m.group(0).lower(),entry.as_uri());c.send('textDocument/didOpen',{'textDocument':{'uri':opened,'text':ENTRY,'version':31,'languageId':'xlang'}});c.open(dep,BAD,2);c.send('xlang/check',{'textDocument':{'uri':opened},'version':31},2);r,seen=result(c);assert r['count']==1 and any(m.get('params',{}).get('uri')==opened and m['params']['version']==31 for m in seen);c.change(dep,GOOD,3);c.send('unknown',{},3);_,clears=c.reply(3);assert any(m.get('params',{}).get('uri')==opened and m['params'].get('version')==31 and not m['params']['diagnostics'] for m in clears);done('exact-opened-uri-publication-and-invalidation');c.stop()
  # Capture real compiler JSON for controlled response corruption/timing.
  raw=cli([(entry,ENTRY),(dep,BAD)]);assert not raw['ok'] and raw['sourceReport']['complete'] and raw['resolutionReport']['probes'];fixture=root/'fixture-xl.exe'
  cp=subprocess.run([args.gcc,'-std=c11','-Wall','-Wextra','-Werror','-O2',str(ide/'tests/snapshot_compiler_program.c'),'-lshell32','-o',str(work/'fixture-ascii.exe')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30);assert cp.returncode==0,cp.stderr;shutil.copy2(work/'fixture-ascii.exe',fixture);done('compile-controlled-fixture')
  response(raw);c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);r,_=result(c);assert r['count']==1;done('controlled-baseline-before-corruption');c.stop()
  bad_cases={}
  def altered(name,fn):v=copy.deepcopy(raw);fn(v);bad_cases[name]=v
  altered('incomplete-source-report',lambda v:v['sourceReport'].update(complete=False))
  altered('wrong-overlay-hash',lambda v:v['sourceReport']['files'][0].update(textSha256='0'*64))
  altered('wrong-overlay-size',lambda v:v['sourceReport']['files'][0].update(textSize=1))
  altered('missing-entry-report',lambda v:v['sourceReport'].update(files=v['sourceReport']['files'][1:]))
  altered('drive-relative-source-report',lambda v:v['sourceReport']['files'][0].update(path=entry.drive+entry.name))
  altered('drive-relative-resolution-probe',lambda v:v['resolutionReport']['probes'][0].update(path=entry.drive+'dep/xlang.package.json'))
  altered('duplicate-source-report',lambda v:v['sourceReport']['files'].append(v['sourceReport']['files'][0]))
  altered('unsubmitted-overlay',lambda v:v['sourceReport']['files'][0].update(path=str(extra)))
  altered('foreign-diagnostic',lambda v:v['diagnostics'][0].update(file=str(extra)))
  altered('invalid-diagnostic-position',lambda v:v['diagnostics'][0].update(column=999999))
  altered('missing-resolution-report',lambda v:v.pop('resolutionReport'))
  altered('invalid-resolution-schema',lambda v:v['resolutionReport'].update(schema=2))
  altered('duplicate-resolution-probe',lambda v:v['resolutionReport']['probes'].append(v['resolutionReport']['probes'][0]))
  altered('wrong-candidate-state',lambda v:v['resolutionReport']['probes'][0].update(exists=not v['resolutionReport']['probes'][0]['exists']))
  altered('wrong-package-environment',lambda v:v['resolutionReport'].update(packagePathObserved=True,XLANG_PACKAGE_PATH='different-path'))
  altered('wrong-exit-status',lambda v:v.update(ok=True))
  for name,bad in bad_cases.items():
   response(bad);c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);m,seen=c.reply(2);assert m['error']['code']==-32603 and not any(x.get('params',{}).get('diagnostics') for x in seen),(name,m);done(name);c.stop()
  response(raw);c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);r,_=result(c);assert r['count']==1;done('controlled-valid-response');c.stop()
  response(raw,'unsupported');c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);m,seen=c.reply(2);assert m['error']['code']==-32603 and not (root/'snapshot-signal.txt').exists() and not any(x.get('params',{}).get('diagnostics') for x in seen);done('legacy-help-capability-rejected-before-check');c.stop()
  for name,operation,code in [
   ('dependency-change-cancels-check',lambda c:c.change(dep,GOOD,3),-32801),
   ('dependency-close-cancels-check',lambda c:c.send('textDocument/didClose',{'textDocument':{'uri':dep.as_uri()}}),-32800),
   ('new-open-document-cancels-check',lambda c:c.open(extra,GOOD,3),-32801),
   ('explicit-cancel',lambda c:c.send('$/cancelRequest',{'id':2}),-32800)]:
   response(raw,'slow');c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);signal();operation(c);m,seen=c.reply(2);assert m['error']['code']==code and not any(x.get('params',{}).get('diagnostics') for x in seen);done(name);c.stop()
  # A new higher-priority manifest can invalidate a previously missing candidate.
  probe=next(x for x in raw['resolutionReport']['probes'] if not x['exists'] and Path(x['path']).name=='xlang.package.json');candidate=Path(probe['path']);assert candidate.is_relative_to(root) and not candidate.exists();response(raw,'slow');c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);signal();candidate.parent.mkdir(parents=True,exist_ok=True);candidate.write_text('{}',encoding='utf-8');m,seen=c.reply(2);assert m['error']['code']==-32603 and not any(x.get('params',{}).get('diagnostics') for x in seen);candidate.unlink();done('new-import-candidate-rejects-result');c.stop()
  # Disk source changed after compiler capture must never be published.
  disk_entry='import disk\nfunc main(...) as int\n    return disk.value()\nend\n';disk_raw=cli([(entry,disk_entry)]);response(disk_raw,'slow exit0');c=client(fixture);c.open(entry,disk_entry,1);c.check(entry,1);signal();saved=disk.read_bytes();disk.write_bytes(saved+b'\n');m,seen=c.reply(2);assert m['error']['code']==-32603 and not any(x.get('params',{}).get('diagnostics') for x in seen);disk.write_bytes(saved);done('disk-import-change-rejects-result');c.stop()
  disk_raw=cli([(entry,disk_entry)]);response(disk_raw,'slow exit0');c=client(fixture);c.open(entry,disk_entry,1);c.check(entry,1);signal();replacement=root/'replacement.tmp';replacement.write_bytes(disk.read_bytes());os.replace(replacement,disk);m,seen=c.reply(2);assert m['error']['code']==-32603 and not any(x.get('params',{}).get('diagnostics') for x in seen);done('same-byte-disk-replacement-rejects-result');c.stop()
  package=root/'pkg';package.mkdir();manifest=package/'xlang.package.json';manifest.write_bytes(json.dumps({'format':'xlang-package-1','name':'pkg','version':'1.0.0','abiVersion':'1','kind':'source','artifact':{'kind':'source','xlSources':['src/api.xl']},'dependencies':[],'links':[],'loadlibs':[]}).encode());api=package/'src/api.xl';package_entry='import pkg\nfunc main(...) as int\n    return pkg.value()\nend\n'
  c=client();c.open(entry,package_entry,1);c.open(api,BAD,17);c.check(entry,1);r,_=result(c);assert r['count']==1 and Path(r['diagnostics'][0]['file']).resolve()==api.resolve() and not api.exists();done('package-virtual-source-diagnostic');c.stop()
  package_raw=cli([(entry,package_entry),(api,BAD)]);response(package_raw,'slow');c=client(fixture);c.open(entry,package_entry,1);c.open(api,BAD,17);c.check(entry,1);signal();manifest.write_bytes(manifest.read_bytes()+b'\n');m,seen=c.reply(2);assert m['error']['code']==-32603 and not any(x.get('params',{}).get('diagnostics') for x in seen);done('package-manifest-change-rejects-result');c.stop()
  c=client();c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);r,_=result(c);assert r['count']==1;c.process.kill();c.process.wait();c.stop();c=client();c.open(entry,ENTRY,10);c.open(dep,GOOD,20);c.check(entry,10);r,_=result(c);assert r['count']==0;done('restart-full-resync');c.stop()
  response(raw,'slow hold');c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);signal();assert (root/'snapshot-signal.txt').read_text()=='held';c.process.kill();c.process.wait(timeout=5);c.stop();deadline=time.monotonic()+5
  while {x.name for x in Path(os.environ['TEMP']).glob('xide-check-*')}-temp_before and time.monotonic()<deadline:time.sleep(.01)
  assert not ({x.name for x in Path(os.environ['TEMP']).glob('xide-check-*')}-temp_before);done('busy-server-crash-kernel-snapshot-cleanup')
  # Actual mutations AFTER successful publication exercise the retained worker.
  def invalidated(c,check_id,uris):
   seen=[]
   while True:
    m=c.next();seen.append(m)
    if m.get('method')=='xlang/diagnosticsInvalidated':
     p=m['params'];assert p['checkId']==check_id and p['uri']==entry.as_uri() and p['version']==1 and p['reason'];break
   c.send('unknown',{},8800);_,tail=c.reply(8800);seen+=tail
   assert uris <= {m.get('params',{}).get('uri') for m in seen if m.get('method')=='textDocument/publishDiagnostics' and not m['params']['diagnostics']}
   return seen
  def preserved(c,id=8801):
   time.sleep(1.3);c.send('unknown',{},id);_,seen=c.reply(id);assert not any(m.get('method')=='xlang/diagnosticsInvalidated' for m in seen),seen
  for mode in ['content','same-byte-replacement','delete','directory','bom-only']:
   saved=disk.read_bytes();c=client();c.open(entry,disk_entry,1);c.check(entry,1);r,_=result(c);assert r['checkId']==2
   if mode=='content':disk.write_bytes(saved+b'\n')
   elif mode=='same-byte-replacement':replacement.write_bytes(saved);os.replace(replacement,disk)
   elif mode=='delete':disk.unlink()
   elif mode=='directory':disk.unlink();disk.mkdir()
   else:assert saved.startswith(b'\xef\xbb\xbf');disk.write_bytes(saved[3:])
   invalidated(c,2,{entry.as_uri(),disk.as_uri()});done('published-disk-'+mode);c.stop()
   if disk.is_dir():disk.rmdir()
   disk.write_bytes(saved)
  # A directory does not become an ordinary-file candidate; file creation does.
  c=client();c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);r,_=result(c)
  candidate.mkdir();preserved(c);done('published-candidate-directory-preserves-result');candidate.rmdir();candidate.write_bytes(b'{}')
  invalidated(c,2,{entry.as_uri(),dep.as_uri()});done('published-new-candidate-invalidates-all-groups');c.stop();candidate.unlink()
  c=client();c.open(entry,package_entry,1);c.open(api,BAD,17);c.check(entry,1);r,_=result(c);assert r['count']==1
  saved=manifest.read_bytes();replacement.write_bytes(saved);os.replace(replacement,manifest)
  invalidated(c,2,{entry.as_uri(),api.as_uri(),manifest.as_uri()});done('published-same-byte-manifest-replacement');c.stop()
  # A real compiler response drives a standalone fixture; no frozen tool is edited.
  response(raw);c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);result(c)
  sibling=fixture.with_name('xl.dll');assert not sibling.exists();sibling.write_bytes(b'new-runtime')
  invalidated(c,2,{entry.as_uri(),dep.as_uri()});done('published-new-toolchain-sibling');c.stop();sibling.unlink()
  response(raw);c=client(fixture);c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);result(c)
  replacement.write_bytes(fixture.read_bytes());os.replace(replacement,fixture);invalidated(c,2,{entry.as_uri(),dep.as_uri()});done('published-same-byte-compiler-replacement');c.stop()
  c=client();c.open(entry,ENTRY,1);c.open(dep,BAD,2);c.check(entry,1);result(c)
  unrelated=root/'unrelated.txt';unrelated.write_bytes(b'unrelated');preserved(c);done('published-unrelated-file-preserves-result')
  saved=dep.read_bytes();dep.write_bytes(saved+b'\n');preserved(c,8802);done('published-overlay-keeps-disk-independent-text');c.stop();dep.write_bytes(saved)
  # Retiring an old guard cannot invalidate a replacement request/generation.
  c=client();c.open(entry,disk_entry,1);c.check(entry,1);result(c);c.change(entry,'func main(...) as int\n    return 0\nend\n',2);c.check(entry,2,3);r,_=result(c,3);assert r['checkId']==3
  saved=disk.read_bytes();disk.write_bytes(saved+b'\n');preserved(c);done('retired-guard-cannot-clear-new-generation');c.stop();disk.write_bytes(saved)
  assert source_before=={str(x):digest(x) for x in [entry,dep,disk]};assert tool_before=={str(Path(x)):digest(Path(x)) for x in tool_before};assert not virtual.exists() and not left.exists() and not right.exists() and not extra.exists();assert not ({x.name for x in Path(os.environ['TEMP']).glob('xide-check-*')}-temp_before);done('original-bytes-toolchain-and-temporary-cleanup');status='passed'
 finally:
  for i,c in enumerate(clients):
   stderr=c.stop();(work/f'server-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(work/f'server-{i}.stderr').write_text(stderr,encoding='utf-8')
  evidence={'startedUtc':start,'completedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':status,'compiler':str(compiler),'server':str(server),'results':results,'sourceBefore':source_before,'toolchainBefore':tool_before};(work/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'status':status,'checks':len(results),'evidence':str(work/'evidence.json')}))
if __name__=='__main__':main()
