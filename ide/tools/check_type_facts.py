"""Real compiler type declaration/reference facts and guarded IDE navigation."""
import argparse,copy,datetime,hashlib,json,shutil,subprocess,uuid
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position

SOURCE='''protocol Named
    func name() as string
end
protocol Reader<T>
    func read() as T
end
class Base
    func answer() as int
        return 7
    end
end
class Child extends Base
end
class Left<T>
    T value
end
class Right<T>
    T value
end
func keep<T>(T value) as T
    return value
end
func bounded<T is Named>(T value) as int
    return 0
end
func baseBound<T is Base>(T value) as int
    return 0
end
func consume(Named value, Reader<int> reader, Left<int> left, Right<string> right)
end
func main(...) as int
    return keep::<int>(0)
end
'''
ABI='''export protocol Named
    func name() as string
end
export class Base
    func answer() as int
        return 7
    end
end
class Child extends Base
end
export union Number
    int value
    float real
end
func bounded<T is Named>(T value) as int
    return 0
end
export "type_fact_value" func value(Number input, Base instance) as int
    return input.value + instance.answer()
end
'''

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--compiler',type=Path,required=True);ap.add_argument('--server',type=Path);ap.add_argument('--baseline',type=Path);ap.add_argument('--skip-lsp',action='store_true');args=ap.parse_args()
 ide=Path(__file__).resolve().parents[1];compiler=args.compiler.resolve();baseline=args.baseline.resolve() if args.baseline else None;server=(args.server or (ide/'release/xlang-ls.exe' if ide.parent.name=='build' and ide.name.startswith('cb-') else ide.parent/'release/xlang-ls.exe')).resolve();work=ide/'build/targeted'/('type-facts-'+uuid.uuid4().hex);work.mkdir(parents=True);entry=work/'入口😀.xl';entry.write_bytes(b'\xef\xbb\xbf'+SOURCE.replace('\n','\r\n').encode());abi=work/'abi.xl';abi.write_text(ABI,encoding='utf8')
 protected={str(p):sha(p) for p in (entry,abi,compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'))};results=[];clients=[];status='failed';request=10;start=datetime.datetime.now(datetime.timezone.utc).isoformat()
 def done(name,**data):results.append({'name':name,'passed':True,**data})
 def command(label,tool,args):
  cp=subprocess.run([str(tool),*map(str,args)],cwd=work,capture_output=True,timeout=60);(work/(label+'.stdout')).write_bytes(cp.stdout);(work/(label+'.stderr')).write_bytes(cp.stderr);assert cp.returncode==0,(label,cp.returncode,cp.stdout[-1000:],cp.stderr);return json.loads(cp.stdout)
 def verify(value,text):
  rows=value['semanticIndex']['nodes'];assert value['ok'] and value['semanticIndex']['complete'];assert all('declaredTypeId' in n and 'referencedTypeId' in n for n in rows)
  declarations={}
  for n in rows:
   assert not (n['declaredTypeId'] and n['referencedTypeId']);assert not n['declaredTypeId'] or n['declaredTypeId']==n['typeId'];assert not n['referencedTypeId'] or n['referencedTypeId']==n['typeId']
   if n['declaredTypeId'] and n['selectionRange']:declarations.setdefault(n['declaredTypeId'],[]).append(n)
  named=next(n for n in rows if n['kind']=='protocol' and n['text']=='Named');assert named['declaredTypeId'] and not named['declaredSymbolId'] and named['selectionRange'];done('protocol-type-declaration-independent-of-symbols')
  base=next(n for n in rows if n['kind']=='class' and n['text']=='Base');extends=next(n for n in rows if n['kind']=='name' and n['text']=='Base' and n['referencedTypeId']);assert extends['referencedTypeId']==base['declaredTypeId'] and extends['typeAbiId']==base['typeAbiId'];done('real-checked-base-name-reference')
  generics=[n for n in rows if n['kind']=='name' and n['text']=='T' and n['declaredTypeId']];assert len(generics)>=6 and len({n['declaredTypeId'] for n in generics})==len(generics);assert all(n['typeAbiId']=='0000000000000000' and n['selectionRange'] for n in generics);done('six-scoped-same-spelling-generic-declarations',ids=[n['declaredTypeId'] for n in generics])
  refs=[n for n in rows if n['kind']=='type' and n['text']=='T' and n['referencedTypeId']];assert len(refs)>=7
  for n in refs:assert n['referencedTypeId'] in declarations and all(d['text']=='T' for d in declarations[n['referencedTypeId']])
  done('generic-reference-identities-from-real-type-check')
  for name in ('Named','Base'):
   ref=next(n for n in rows if n['kind']=='type' and n['text']==name and n['referencedTypeId'] and rows[n['parent']-1]['kind']=='generic_constraint');assert declarations[ref['referencedTypeId']][0]['text']==name;done('checked-generic-constraint-'+name)
  return rows,declarations
 def client(text,tool=compiler):
  c=Client(server,tool,work);clients.append(c);c.initialize();c.open(entry,text,9);c.check(entry,9,2);m,_=c.reply(2);assert m.get('result',{}).get('semanticDefinitions') and m['result']['count']==0,m;return c
 def query(c,text,start,expected):
  nonlocal request
  c.send('textDocument/definition',{'textDocument':{'uri':entry.as_uri()},'position':position(text,start),'version':9,'checkId':2},request);m,_=c.reply(request);request+=1;assert m.get('result','missing')==expected,(m,expected)
 try:
  raw=command('actual-type-index',compiler,['check',entry,'--json','--source-report','--semantic-index']);rows,declarations=verify(raw,SOURCE)
  # The BOM affects only the first document line; use actual byte offsets to
  # derive the precise UTF-16 query and target on every line.
  text='\ufeff'+SOURCE.replace('\n','\r\n');afterBom=text[1:].encode()
  def pointAt(offset):return position(text,1+len(afterBom[:offset].decode()))
  def wire(n):return {'uri':entry.as_uri(),'range':{'start':pointAt(n['selectionRange']['startOffset']),'end':pointAt(n['selectionRange']['endOffset'])}}
  if baseline:
   old=command('baseline-type-index',baseline,['check',entry,'--json','--source-report','--semantic-index']);oldRows=old['semanticIndex']['nodes'];assert len(oldRows)==len(rows)
   for previous,current in zip(oldRows,rows):
    for key in ('symbolId','declaredSymbolId','targetSymbolId','compoundOperatorSymbolId'):assert previous[key]==current[key],(key,previous,current)
   done('runtime-symbol-identities-unchanged-from-frozen-compiler')
   for label,tool in (('new',compiler),('old',baseline)):
    bridge=command(label+'-abi-bridge',tool,['c-bridge',abi,'--json']);(work/(label+'-declarations.h')).write_text(bridge['cBridge']['declarations'],encoding='utf8')
    folder=work/label;folder.mkdir();header=folder/'public.h';cp=subprocess.run([str(tool),'cgen',str(abi),'--emit-header',str(header),'--emit',str(folder/'module.c')],cwd=work,capture_output=True,timeout=60);(work/(label+'-plain.stderr')).write_bytes(cp.stderr);assert cp.returncode==0,(label,cp.stdout,cp.stderr)
   assert (work/'new-declarations.h').read_bytes()==(work/'old-declarations.h').read_bytes();assert (work/'new/public.h').read_bytes()==(work/'old/public.h').read_bytes();done('plain-and-query-header-bytes-unchanged-from-frozen-compiler')
  if not args.skip_lsp:
   c=client(text)
   for n in rows:
    if n['referencedTypeId'] in declarations and n['tokenRange'] and (n['text'] in ('Named','Base','T','Reader')):
     target=declarations[n['referencedTypeId']][0];offset=n['tokenRange']['startOffset'];query(c,text,1+len(afterBom[:offset].decode()),wire(target));done('lsp-typed-reference-'+n['text'],node=n['id'])
   for n in rows:
    if n['declaredTypeId'] and n['kind'] in ('protocol','name') and n['selectionRange']:
     query(c,text,1+len(afterBom[:n['selectionRange']['startOffset']].decode()),wire(n));done('lsp-type-declaration-self',node=n['id'])
   c.send('textDocument/documentSymbol',{'textDocument':{'uri':entry.as_uri()},'version':9,'checkId':2},request);m,_=c.reply(request);request+=1;outline=m['result'];assert any(n['name']=='Named' and n['kind']==11 for n in outline);assert any(n['name']=='Reader' and any(v['name']=='T' and v['kind']==26 for v in n['children']) for n in outline);done('real-protocol-and-class-type-parameter-outline');c.stop()
   fixture=work/'fixture.exe';cp=subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-O2',str(ide/'tests/snapshot_compiler_program.c'),'-lshell32','-o',str(fixture)],capture_output=True,timeout=30);assert cp.returncode==0,cp.stderr
   overlay=work/'capture.utf8';overlay.write_bytes(text.encode())
   transport=command('actual-overlay-type-index',compiler,['check',entry,'--json','--source-report','--semantic-index','--source-overlay',entry,overlay]);assert transport['semanticIndex']==raw['semanticIndex']
   def controlled(value,expectReady=True):
    (work/'snapshot-response.json').write_text(json.dumps(value,ensure_ascii=False),encoding='utf8');(work/'snapshot-mode.txt').write_text('indexed exit0');(work/'snapshot-signal.txt').unlink(missing_ok=True)
    # Injected compiler responses must use the explicit CLI oracle. Default
    # native analysis deliberately does not execute this fixture program.
    c=Client(server,fixture,work);clients.append(c);c.initialize('cli');c.open(entry,text,9);c.check(entry,9,2);m,_=c.reply(2);assert (work/'snapshot-signal.txt').is_file()
    if expectReady:assert m.get('result',{}).get('semanticDefinitions'),m
    else:assert m['error']['code']==-32603 and '语义索引' in m['error']['message'],m
    return c
   c=controlled(transport);done('valid-overlay-index-through-controlled-transport');c.stop()
   named=next(n for n in rows if n['kind']=='protocol' and n['text']=='Named')
   namedRef=next(n for n in rows if n['kind']=='type' and n['text']=='Named' and n['referencedTypeId'])
   at=1+len(afterBom[:namedRef['tokenRange']['startOffset']].decode())
   for label,other,expected in (
    ('identical-type-copy-retains-one-verified-target',named,wire(named)),
    ('same-type-id-at-different-declarations-is-ambiguous',next(n for n in rows if n['kind']=='protocol' and n['text']=='Reader'),None),
    ('same-type-id-copy-without-a-selection-is-ambiguous',named,None),
   ):
    value=copy.deepcopy(transport);duplicate=copy.deepcopy(other);duplicate.update(id=len(rows)+1,parent=0,typeId=named['typeId'],declaredTypeId=named['declaredTypeId'],typeAbiId=named['typeAbiId'])
    if 'without-a-selection' in label:duplicate['selectionRange']=None
    value['semanticIndex']['nodes'].append(duplicate);c=controlled(value);query(c,text,at,expected);done(label);c.stop()
   for name,mutate in (
    ('mixed-new-old-fields',lambda value:value['semanticIndex']['nodes'][0].pop('declaredTypeId')),
    ('type-declaration-id-mismatch',lambda value:next(n for n in value['semanticIndex']['nodes'] if n['kind']=='protocol').update(declaredTypeId=4294967295)),
    ('negative-reference-id',lambda value:next(n for n in value['semanticIndex']['nodes'] if n['referencedTypeId']).update(referencedTypeId=-1)),
    ('name-expression-cannot-declare-a-type',lambda value:next(n for n in value['semanticIndex']['nodes'] if n['kind']=='name' and n['text']=='value' and n['targetSymbolId']).update(typeId=4294967295,declaredTypeId=4294967295)),
   ):
    value=copy.deepcopy(transport);mutate(value);c=controlled(value,False);done(name);c.stop()
   generic=next(n for n in rows if n['kind']=='name' and n['declaredTypeId']);value=copy.deepcopy(transport);value['semanticIndex']['nodes'][generic['id']-1]['typeAbiId']='0000000000000001';c=controlled(value,False);done('generic-declaration-cannot-invent-a-concrete-abi');c.stop()
  for p,h in protected.items():assert sha(Path(p))==h,p
  done('original-inputs-and-compiler-preserved');status='passed'
 finally:
  for i,c in enumerate(clients):
   c.stop();(work/f'transcript-{i:02}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8');(work/f'stderr-{i:02}.txt').write_text(c.stderr,encoding='utf8')
  (work/'evidence.json').write_text(json.dumps({'status':status,'started':start,'finished':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(compiler),'server':str(server),'skipLsp':args.skip_lsp,'results':results,'protected':protected},ensure_ascii=False,indent=2),encoding='utf8');print(f'{status}: {len(results)} type fact cases; {work}')
if __name__=='__main__':main()
