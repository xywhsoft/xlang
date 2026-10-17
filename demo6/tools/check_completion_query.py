#!/usr/bin/env python3
"""Real compiler cursor queries: lexical scopes, partial input and provenance."""
import argparse, ctypes, datetime, hashlib, json, os, shutil, subprocess, uuid
from pathlib import Path

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def position(data, offset):
 text=data[:offset].decode('utf-8');line,column,i=1,1,0
 while i<len(text):
  if text[i]=='\r':
   if i+1<len(text) and text[i+1]=='\n':i+=1
   line,column=line+1,1
  elif text[i]=='\n':line,column=line+1,1
  else:column+=1
  i+=1
 return line,column
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);ap.add_argument('--compiler',type=Path,required=True);args=ap.parse_args()
 if os.name!='nt': raise RuntimeError('Windows native DLL/Unicode API gate required')
 root=args.root.resolve();exe=args.compiler.resolve();work=root/'build/test'/('completion-query-'+str(uuid.uuid4()));src=work/'源码 Ω';src.mkdir(parents=True)
 env=dict(os.environ,PATH=str(exe.parent)+os.pathsep+os.environ.get('PATH',''))
 protected={str(p):sha(p) for p in [exe,exe.with_name('xlc.dll'),exe.with_name('xl.dll')]};cases=[];raw={};status='failed'
 def put(name,text,bom=False,crlf=False):
  data=text.replace('\n','\r\n' if crlf else '\n').encode();data=(b'\xef\xbb\xbf' if bom else b'')+data;p=src/name;p.write_bytes(data);raw[str(p.resolve()).lower()]=data;return p
 def run(label,cmd,expected=0):
  c=subprocess.run([str(x) for x in cmd],cwd=src,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
  (work/(label+'.stdout')).write_bytes(c.stdout);(work/(label+'.stderr')).write_bytes(c.stderr);cases.append({'name':label,'exitCode':c.returncode,'stdoutBytes':len(c.stdout),'stderrBytes':len(c.stderr)})
  assert c.returncode==expected,(label,c.returncode,c.stdout[:500],c.stderr[:500]);return c
 def validate(v,p,offset,available=True):
  assert v['phase']=='completion' and 'semanticIndex' not in v and v['diagnostics']==[]
  q=v['completion'];assert q['schema']==1 and q['scope']=='cursor-query' and q['checkedCompilation'] is False and q['offset']==offset
  assert v['ok']==q['complete']==available
  assert q['offsetEncoding']=='utf-8-after-bom' and q['columnEncoding']=='unicode-scalar-1-based' and q['rangeConvention']=='half-open'
  files=v['sourceReport']['files'];assert v['sourceReport']['complete']
  if available:
   assert 1<=q['sourceId']<=len(files);f=files[q['sourceId']-1];assert Path(f['path']).resolve()==p.resolve()
   data=raw[str(p.resolve()).lower()].removeprefix(b'\xef\xbb\xbf');span=q['replacementRange'];a,b=span['startOffset'],span['endOffset'];assert 0<=a<=offset<=b<=len(data)
   assert position(data,a)==(span['startLine'],span['startColumn']) and position(data,b)==(span['endLine'],span['endColumn']);data[:offset].decode();assert data[a:offset].decode()==q['prefix']
   assert q['error'] is None
   for item in q['items']:
    assert item['name'].startswith(q['prefix']) and not item['name'].startswith('__xl6_') and '.' not in item['name']
    assert item['kind'] in ('variable','constant','function','field','method','type')
    if item['selectionRange'] is not None:
     f=files[item['selectionSourceId']-1];s=item['selectionRange'];d=(raw[str(Path(f['path']).resolve()).lower()] if str(Path(f['path']).resolve()).lower() in raw else Path(f['path']).read_bytes()).removeprefix(b'\xef\xbb\xbf')
     d[s['startOffset']:s['endOffset']].decode('utf-8');assert s['endOffset']<=len(d);assert position(d,s['startOffset'])==(s['startLine'],s['startColumn']) and position(d,s['endOffset'])==(s['endLine'],s['endColumn'])
  else: assert not q['items'] and q['error']
  for f in files:
   data=raw[str(Path(f['path']).resolve()).lower()] if str(Path(f['path']).resolve()).lower() in raw else Path(f['path']).read_bytes();assert hashlib.sha256(data).hexdigest()==f['rawSha256']
  return q
 def query(label,p,needle=None,offset=None,extra=(),available=True):
  data=raw[str(p.resolve()).lower()].removeprefix(b'\xef\xbb\xbf');offset=offset if offset is not None else data.index(needle.encode())+len(needle.encode())
  c=run(label,[exe,'check',p,'--json','--completion-at',p,offset,*extra],0 if available else 1)
  return validate(json.loads(c.stdout.decode('utf-8-sig')),p,offset,available)
 def named(q,name): return [i for i in q['items'] if i['name']==name]
 try:
  p=put('local.xl','int alphabet = 9\nfunc calc(int alphabet) as int\n if true\n  string alphabet = "😀"\n  auto later = al\n end\n return alphabet\nend\n',bom=True,crlf=True)
  q=query('unicode-shadow-local',p,'later = al');x=named(q,'alphabet');assert len(x)==1 and x[0]['canonicalType']=='string' and x[0]['selectionRange']['startLine']==4
  p=put('scope.xl','func calc(int argument) as int\n int unassigned\n if true\n  int hidden = 2\n end\n auto current = a\n int after = 3\n return argument\nend\n')
  q=query('scope-order',p,offset=p.read_bytes().index(b'current = a')+len(b'current = '));assert named(q,'argument') and not any(named(q,n) for n in ['unassigned','hidden','current','after'])
  # Empty prefix in the middle of an existing identifier: replace the entire token.
  p=put('middle.xl','func calc(int alphabet) as int\n return alphabet\nend\n');data=p.read_bytes();off=data.index(b'return alphabet')+len(b'return al');q=query('middle-token',p,offset=off);assert named(q,'alphabet') and q['replacementRange']['endOffset']==off+6
  p=put('function.xl','func pick(int value) as int\n return value\nend\nfunc pick(string value) as string\n return value\nend\nfunc test() as int\n auto selected = pi\n return 0\nend\n');q=query('overload-signatures',p,'selected = pi');assert len(named(q,'pick'))==2 and len({i['symbolId'] for i in named(q,'pick')})==2 and len({i['canonicalType'] for i in named(q,'pick')})==2
  cls='class Box<T>\n T value\n private int secret = 7\n protected int inherited = 3\n static int total = 2\n create(T initial)\n  self.value = initial\n end\n func read() as T\n  return self.value\n end\n static func count() as int\n  return Box.total\n end\nend\n'
  p=put('members.xl',cls+'func test() as int\n auto box = Box::<int>(3)\n auto result = box.\n return 0\nend\n');q=query('member-after-dot',p,'result = box.');assert named(q,'value')[0]['canonicalType']=='int' and named(q,'read') and not any(named(q,n) for n in ['secret','inherited','total','count']) and q['replacementRange']['startOffset']==q['replacementRange']['endOffset']
  p=put('member-prefix.xl',cls+'func test() as int\n auto box = Box::<string>("ready")\n auto result = box.re\n return 0\nend\n');q=query('member-half-name',p,'result = box.re');assert named(q,'read') and 'string' in named(q,'read')[0]['canonicalType'] and not named(q,'value')
  staticcls=cls.replace('class Box<T>','class Box').replace('T value','int value').replace('create(T initial)','create(int initial)').replace('as T','as int')
  p=put('static.xl',staticcls+'func test() as int\n auto result = Box.\n return 0\nend\n');q=query('static-members',p,'result = Box.');assert named(q,'total') and named(q,'count') and not named(q,'value') and not named(q,'read')
  p=put('private.xl','class Box\n private int secret = 2\n func read() as int\n  auto result = self.se\n  return 0\n end\nend\n');q=query('own-private-member',p,'result = self.se');assert named(q,'secret')
  parent='class Parent\n int baseValue = 3\n private int hidden = 2\n protected int protectedValue = 1\n func read() as int\n  return self.baseValue\n end\nend\nclass Child extends Parent\n int childValue = 4\nend\n'
  p=put('inherited.xl',parent+'func test(Child child) as int\n auto result = child.\n return 0\nend\n');q=query('inherited-public-members',p,'result = child.');assert named(q,'baseValue') and named(q,'childValue') and named(q,'read') and not named(q,'hidden') and not named(q,'protectedValue')
  nullable='class Box\n int value = 3\nend\n'
  p=put('nullable.xl',nullable+'func test(Box? box) as int\n auto result = box?.\n return 0\nend\n');q=query('safe-nullable-members',p,'result = box?.');assert named(q,'value')
  p=put('nullable-dot.xl',nullable+'func test(Box? box) as int\n auto result = box.\n return 0\nend\n');query('nullable-needs-narrowing',p,'result = box.',available=False)
  p=put('flow.xl',nullable+'func test(Box? box) as int\n if box != null\n  auto result = box.\n end\n return 0\nend\n');q=query('flow-narrowed-members',p,'result = box.');assert named(q,'value')
  p=put('unsupported.xl','func test() as int\n string text = "ready"\n auto result = text.\n return 0\nend\n');query('unsupported-receiver',p,'result = text.',available=False)
  p=put('invalid-receiver.xl','func test() as int\n auto result = missing.\n return 0\nend\n');query('undeclared-receiver',p,'result = missing.',available=False)
  p=put('prior-error.xl','func test() as int\n auto failed = absent\n auto result = va\n return 0\nend\n');query('prior-semantic-error',p,'result = va',available=False)
  p=put('broken.xl','func broken(\n');query('unrecoverable-parse',p,offset=len(p.read_bytes()),available=False)
  p=put('string.xl','func test() as int\n auto text = "alphabet"\n return 0\nend\n');query('inside-string',p,'"alpha',available=False)
  p=put('comment.xl','func test() as int\n // alphabet\n return 0\nend\n');query('inside-comment',p,'// alpha',available=False)
  p=put('boundary.xl','func test(int 数量) as int\n return 数\nend\n');query('utf8-interior',p,offset=p.read_bytes().index(b'return ')+8,available=False);query('offset-past-eof',p,offset=len(p.read_bytes())+1,available=False)
  run('json-required',[exe,'check',p,'--completion-at',p,0],1);run('check-required',[exe,'parse',p,'--json','--completion-at',p,0],1)
  for name,tail in [('negative',['-1']),('overflow',['18446744073709551616']),('nonnumeric',['1x']),('missing',[])]: run('offset-'+name,[exe,'check',p,'--json','--completion-at',p,*tail],1)
  run('duplicate-query',[exe,'check',p,'--json','--completion-at',p,0,'--completion-at',p,0],1);run('index-conflict',[exe,'check',p,'--json','--semantic-index','--completion-at',p,0],1)
  # Query is not a valid compilation: ordinary checks still reject half identifiers/dots.
  run('partial-ordinary-check',[exe,'check',src/'members.xl','--json'],1);v=json.loads(run('partial-index-check',[exe,'check',src/'members.xl','--json','--semantic-index'],1).stdout);assert not v['semanticIndex']['complete'] and not v['semanticIndex']['nodes']
  p=put('中文.xl','func 计算(int 数量) as int\n auto text = "😀"; auto result = 数\n return 数量\nend\n',bom=True,crlf=True);q=query('unicode-scalar-ranges',p,'result = 数');assert named(q,'数量')[0]['canonicalType']=='int'
  p=put('defaults.xl','func test() as int\n string defaultText\n int unassigned\n auto result = probe\n return 0\nend\n');q=query('real-default-initialization',p,offset=p.read_bytes().index(b'result = probe')+len(b'result = '));assert named(q,'defaultText') and not named(q,'unassigned')
  p=put('underscore.xl','func test(int __userValue) as int\n return __u\nend\n');q=query('user-double-underscore',p,'return __u');assert named(q,'__userValue')
  p=put('--semantic-index','func test(int flagValue) as int\n return fla\nend\n');offset=p.read_bytes().index(b'return fla')+len(b'return fla');v=json.loads(run('option-like-query-path',[exe,'check',p,'--json','--completion-at','--semantic-index',offset]).stdout);q=validate(v,p,offset);assert named(q,'flagValue')
  p=put('--json','func test(int flagValue) as int\n return fla\nend\n');run('query-path-not-json-flag',[exe,'check',p,'--completion-at','--json',offset],1)
  p=put('item-limit.xl',''.join(f'func item{i}() as int\n return 0\nend\n' for i in range(4100))+'func test() as int\n return item\nend\n');q=query('all-or-none-item-limit',p,'return item',available=False);assert 'limit' in q['error']
  p=put('name-limit.xl','int prefix'+('a'*513)+' = 1\nfunc test() as int\n return prefix\nend\n');q=query('all-or-none-name-limit',p,'return prefix',available=False);assert 'limit' in q['error']
  # Unsaved entry and dependency snapshots retain their own exact hashes.
  p=put('overlay-entry.xl','func test(int savedName) as int\n return savedName\nend\n');saved=p.read_bytes();dirty='func test(int dirtyName) as int\n return dir\nend\n';snapshot=put('entry.snapshot',dirty);raw[str(p.resolve()).lower()]=dirty.encode()
  q=query('entry-overlay',p,'return dir',extra=['--source-overlay',p,snapshot]);assert named(q,'dirtyName') and not named(q,'savedName');assert p.read_bytes()==saved;raw[str(p.resolve()).lower()]=saved
  helper=put('helper.xl','export "completion_answer" func answer() as int\n return 42\nend\n');saved=helper.read_bytes();dirty='export "completion_answer" func answer() as int\n auto dirtyLocal = 7\n return dir\nend\n';snapshot=put('helper.snapshot',dirty);raw[str(helper.resolve()).lower()]=dirty.encode()
  entry=put('entry.xl','import helper\nfunc main(...) as int\n return helper.answer()\nend\n');offset=dirty.encode().index(b'return dir')+len(b'return dir')
  c=run('dependency-overlay',[exe,'check',entry,'--json','--completion-at',helper,offset,'--source-overlay',helper,snapshot]);v=json.loads(c.stdout);q=validate(v,helper,offset);assert named(q,'dirtyLocal') and any(f['origin']=='overlay' and Path(f['path']).name=='helper.xl' for f in v['sourceReport']['files']);assert helper.read_bytes()==saved;raw[str(helper.resolve()).lower()]=saved
  # Public API: memory-only entry, unchanged context, failure followed by success.
  dll=ctypes.CDLL(str(exe.with_name('xlc.dll')))
  dll.xlcCreateContext.argtypes=[ctypes.c_void_p];dll.xlcCreateContext.restype=ctypes.c_void_p
  dll.xlcDestroyContext.argtypes=[ctypes.c_void_p]
  dll.xlcSetWorkDir.argtypes=[ctypes.c_void_p,ctypes.c_char_p];dll.xlcSetWorkDir.restype=ctypes.c_int
  dll.xlcAddSourceString.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_char_p];dll.xlcAddSourceString.restype=ctypes.c_int
  dll.xlcComplete.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p];dll.xlcComplete.restype=ctypes.c_int
  dll.xlcCheck.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int];dll.xlcCheck.restype=ctypes.c_int
  dll.xlcGetLastError.argtypes=[ctypes.c_void_p];dll.xlcGetLastError.restype=ctypes.c_char_p
  ctx=dll.xlcCreateContext(None);assert ctx
  try:
   assert dll.xlcSetWorkDir(ctx,str(src).encode())
   virtual=src/'memory Ω.xl';source='func test(int memoryValue) as int\n return mem\nend\n';raw[str(virtual.resolve()).lower()]=source.encode();offset=source.encode().index(b'return mem')+len(b'return mem');output=work/'api-query.json'
   assert dll.xlcAddSourceString(ctx,str(virtual).encode(),source.encode())
   assert dll.xlcComplete(ctx,str(virtual).encode(),len(source.encode())+2,str(output).encode())==0
   assert dll.xlcGetLastError(ctx)
   assert dll.xlcComplete(ctx,str(virtual).encode(),offset,str(output).encode())==1
   assert not dll.xlcGetLastError(ctx)
   v=json.loads(output.read_text(encoding='utf-8'));q=validate(v,virtual,offset);assert named(q,'memoryValue') and v['sourceReport']['files'][q['sourceId']-1]['origin']=='memory' and not virtual.exists()
   check=work/'api-ordinary-check.json';assert dll.xlcCheck(ctx,str(check).encode(),1)==0;assert not json.loads(check.read_text(encoding='utf-8'))['ok']
   cases.append({'name':'public-memory-api-and-context-preservation','exitCode':0})
  finally: dll.xlcDestroyContext(ctx);raw.pop(str(virtual.resolve()).lower())
  # Link a real C consumer against the new public header and exported ABI.
  gcc=shutil.which('gcc');assert gcc
  consumer=work/'completion-consumer.c';consumer.write_text('#include "xlc.h"\n#include <string.h>\nint main(void) {\n const char* source = "func test(int publicValue) as int\\n return pub\\nend\\n";\n XLC_Context* ctx=xlcCreateContext(NULL);\n size_t offset=(size_t)(strstr(source,"return pub")-source)+10;\n int ok=ctx && xlcAddSourceString(ctx,"public-header-memory.xl",source) && xlcComplete(ctx,"public-header-memory.xl",offset,"public-header-query.json");\n if (ctx) xlcDestroyContext(ctx);\n return ok?0:1;\n}\n',encoding='utf-8')
  program=work/'completion-consumer.exe';run('public-header-link',[gcc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(exe.parent/'include'),consumer,exe.with_name('xlc.dll'),'-o',program]);run('public-header-query',[program]);assert json.loads((src/'public-header-query.json').read_text(encoding='utf-8'))['completion']['items'][0]['name']=='publicValue'
  for path,data in raw.items(): assert Path(path).read_bytes()==data
  status='passed'
 finally:
  for path,expected in protected.items(): assert sha(Path(path))==expected,('binary changed',path)
  evidence={'schema':1,'status':status,'finishedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(exe),'protectedHashes':protected,'cases':cases,'work':str(work)}
  (work/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'status':status,'cases':len(cases),'evidence':str(work/'evidence.json')},ensure_ascii=False))
if __name__=='__main__':main()
