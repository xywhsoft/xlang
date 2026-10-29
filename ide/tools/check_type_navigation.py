"""TypeId navigation over actual checked inputs, with hostile identity mutations."""
import argparse,copy,datetime,hashlib,json,shutil,subprocess,uuid
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position

LOCAL='''typedef First = int
typedef Second = int
typedef Chain = First
extern type Native = c_int
class Box
    int value
end
class Child extends Box
end
union Number
    int integer
    float real
end
enum Status as int32
    ok = 1
end
class Generic<T>
    T value
end
func sample(First a, Second b, Chain c, Box box, Number value, Status status, Generic<First> generic, array<Second> items, Box? maybe, Native native) as Chain
    return a + b + c
end
func main(...) as int
    return 0
end
'''
DEPENDENCY='''export typedef Count = int
export class Item
    int value
end
export union Data
    int value
end
'''
IMPORTED='''import helper
func sample(helper.Count amount, helper.Item item, helper.Data data) as helper.Count
    return amount
end
func main(...) as int
    return 0
end
'''

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--compiler',type=Path,required=True);ap.add_argument('--server',type=Path);ap.add_argument('--gcc',default='gcc');ap.add_argument('--type-facts',action='store_true');args=ap.parse_args()
 ide=Path(__file__).resolve().parents[1];compiler=args.compiler.resolve();server=(args.server or (ide/'release/xlang-ls.exe' if ide.parent.name=='build' and ide.name.startswith('cb-') else ide.parent/'release/xlang-ls.exe')).resolve()
 work=ide/'build/targeted'/('types-'+uuid.uuid4().hex);root=work/'来源😀';root.mkdir(parents=True);entry=root/'main.xl';dep=root/'helper.xl'
 entry.write_text('func main(...) as int\n    return 0\nend\n',encoding='utf8');dep.write_bytes(b'\xef\xbb\xbf'+DEPENDENCY.replace('\n','\r\n').encode())
 protected={str(p):sha(p) for p in (entry,dep,compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'),ide/'third_party/xge_sdk/xge.dll')}
 results=[];clients=[];status='failed';request=10;start=datetime.datetime.now(datetime.timezone.utc).isoformat()
 def done(name,**fields):results.append({'name':name,'passed':True,**fields})
 def client(text,tool=compiler,extra=()):
  c=Client(server,tool,root);clients.append(c);c.initialize();c.open(entry,text,7)
  for p,t,v in extra:c.open(p,t,v)
  c.check(entry,7,2);m,_=c.reply(2);assert m.get('result',{}).get('semanticDefinitions') and m['result']['count']==0,m
  return c
 def at(text,needle,nth=1):
  start=-1
  for _ in range(nth):start=text.index(needle,start+1)
  return start
 def target(path,text,needle,nth=1):
  begin=at(text,needle,nth);return {'uri':path.as_uri(),'range':{'start':position(text,begin),'end':position(text,begin+len(needle))}}
 def query(c,text,needle,expected,nth=1,offset=0,path=entry,version=7,batch=2):
  nonlocal request
  p={'textDocument':{'uri':path.as_uri()},'position':position(text,at(text,needle,nth)+offset),'version':version,'checkId':batch}
  c.send('textDocument/definition',p,request);m,_=c.reply(request);request+=1;assert m.get('result','missing')==expected,(p,m,expected)
 def cli(text):
  overlay=root/'capture.utf8';overlay.write_bytes(text.encode());cp=subprocess.run([str(compiler),'check',str(entry),'--json','--source-report','--semantic-index','--source-overlay',str(entry),str(overlay)],cwd=root,capture_output=True,timeout=30);assert cp.returncode==0,cp.stderr;return json.loads(cp.stdout)
 def rows(value,kind,name):return [n for n in value['semanticIndex']['nodes'] if n['kind']==kind and n['text']==name]
 try:
  c=client(LOCAL)
  for name in ('First','Second','Chain','Box','Number','Status','Generic','Native'):
   query(c,LOCAL,name,target(entry,LOCAL,name),nth={'First':3,'Second':2,'Chain':2,'Box':3,'Number':2,'Status':2,'Generic':2,'Native':2}[name]);done('explicit-type-'+name)
  query(c,LOCAL,'First',target(entry,LOCAL,'First'),nth=2);done('alias-chain-targets-written-alias')
  query(c,LOCAL,'First',target(entry,LOCAL,'First'),nth=4);query(c,LOCAL,'Second',target(entry,LOCAL,'Second'),nth=3);query(c,LOCAL,'Box',target(entry,LOCAL,'Box'),nth=4);done('generic-array-nullable-inner-types')
  query(c,LOCAL,'Box',target(entry,LOCAL,'Box') if args.type_facts else None,nth=2);done('checked-class-base-type' if args.type_facts else 'legacy-unannotated-class-base-keeps-null')
  query(c,LOCAL,'int',None)
  query(c,LOCAL,'a + b',{'uri':entry.as_uri(),'range':{'start':position(LOCAL,LOCAL.index('a, Second')),'end':position(LOCAL,LOCAL.index('a, Second')+1)}});done('builtin-null-and-variable-symbol-priority');c.stop()
  unicode='\ufeff// 中文 😀 before declaration\r\n'+LOCAL.replace('\n','\r\n');c=client(unicode);query(c,unicode,'First',target(entry,unicode,'First'),nth=3);query(c,unicode,'Box',target(entry,unicode,'Box'),nth=3);done('bom-crlf-non-bmp-type-coordinates');c.stop()
  c=client(IMPORTED)
  for name in ('Count','Item','Data'):query(c,IMPORTED,'helper.'+name,target(dep,DEPENDENCY.replace('\n','\r\n'),name),offset=7);done('disk-import-'+name)
  c.stop();overlay='\ufeff// 未保存 😀\r\n\r\n'+DEPENDENCY.replace('\n','\r\n');c=client(IMPORTED,extra=[(dep,overlay,11)])
  for name in ('Count','Item','Data'):query(c,IMPORTED,'helper.'+name,target(dep,overlay,name),offset=7);done('unsaved-import-'+name)
  c.change(dep,'\r\n'+overlay,12);c.send('textDocument/definition',{'textDocument':{'uri':entry.as_uri()},'position':position(IMPORTED,IMPORTED.index('helper.Count')+7),'version':7,'checkId':2},request);m,_=c.reply(request);request+=1;assert m['error']['code']==-32801;done('type-batch-invalidated-by-dependency-edit');c.stop()
  raw=cli(LOCAL);(work/'actual-compiler.json').write_text(json.dumps(raw,ensure_ascii=False,indent=2),encoding='utf8')
  first=rows(raw,'type_alias','First')[0];second=rows(raw,'type_alias','Second')[0];assert first['typeId']!=second['typeId'];done('same-canonical-type-distinct-alias-identities',first=first['typeId'],second=second['typeId'])
  fixture=root/'fixture.exe';cp=subprocess.run([args.gcc,'-std=c11','-Wall','-Wextra','-Werror','-O2',str(ide/'tests/snapshot_compiler_program.c'),'-lshell32','-o',str(work/'fixture.exe')],capture_output=True,timeout=30);assert cp.returncode==0,cp.stderr;shutil.copy2(work/'fixture.exe',fixture)
  def controlled(value):
   (root/'snapshot-response.json').write_text(json.dumps(value,ensure_ascii=False),encoding='utf8');(root/'snapshot-mode.txt').write_text('indexed exit0');return client(LOCAL,tool=fixture)
  def identity(row,id,abi=None):
   row['typeId']=id
   for key in ('declaredTypeId','referencedTypeId'):
    if row.get(key):row[key]=id
   if abi is not None:row['typeAbiId']=abi
  c=controlled(raw);query(c,LOCAL,'First',target(entry,LOCAL,'First'),nth=3);done('same-actual-index-through-controlled-transport');c.stop()
  for name,mutate in (
   ('abi-mismatch',lambda value:rows(value,'type','First')[1].update(typeAbiId='0000000000000001')),
   ('missing-type-identity',lambda value:identity(rows(value,'type','First')[1],0)),
   ('unknown-type-identity',lambda value:identity(rows(value,'type','First')[1],4294967295)),
   ('duplicate-type-identity',lambda value:identity(rows(value,'type_alias','Second')[0],first['typeId'],first['typeAbiId'])),
   ('equal-display-name-cannot-resolve-unknown-id',lambda value:identity(rows(value,'type','First')[1],4294967295)),
  ):
   value=copy.deepcopy(raw);mutate(value);c=controlled(value);query(c,LOCAL,'First',None,nth=3);done(name);c.stop()
  # A missing location must never be filled from a same-spelling declaration.
  value=copy.deepcopy(raw);rows(value,'type_alias','First')[0].update(selectionSourceId=0,selectionRange=None);c=controlled(value);query(c,LOCAL,'First',None,nth=3);done('unlocated-type-keeps-null');c.stop()
  for p,h in protected.items():assert sha(Path(p))==h,p
  done('original-files-compiler-and-sdk-preserved');status='passed'
 finally:
  for i,c in enumerate(clients):
   c.stop();(work/f'transcript-{i:02}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8');(work/f'stderr-{i:02}.txt').write_text(c.stderr,encoding='utf8')
  report={'status':status,'started':start,'finished':datetime.datetime.now(datetime.timezone.utc).isoformat(),'server':str(server),'serverSha256':sha(server),'compiler':str(compiler),'results':results,'protected':protected}
  (work/'evidence.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print(f'{status}: {len(results)} type navigation cases; {work}')
if __name__=='__main__':main()
