"""Real nominal C type emission, checked source identities and mixed build."""
import argparse, datetime, hashlib, json, os, subprocess, uuid
from pathlib import Path

SOURCE = '''// 中文 😀 source positions
union Number
 c_int integer
 c_double real
end
union Outer
 Number nested
 c_int integer
end
union Unused
 c_int value
end
typedef Alias = Number
typedef Scalar = c_int
typedef Maybe = c_int?
export "take_number" func takeNumber(Alias value) as Outer
 Outer result
 result.nested = value
 return result
end
export "take_pair" func takePair() as (c_int,c_double)
 return 1,2.0
end
export "take_maybe" func takeMaybe(Maybe value) as c_int
 return 0
end
export "take_scalar" func takeScalar(Scalar value) as c_int
 return value
end
extern "native_answer" func nativeAnswer() as c_int
func main(...) as int
 return nativeAnswer() - 42
end
'''

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--compiler',type=Path,required=True); ap.add_argument('--baseline',type=Path)
    a=ap.parse_args(); ide=Path(__file__).resolve().parents[1]; compiler=a.compiler.resolve(); work=ide/'build/targeted'/('public-types-'+uuid.uuid4().hex); work.mkdir(parents=True)
    src=work/'源码 😀'; src.mkdir(); entry=src/'entry.xl'; entry.write_bytes(b'\xef\xbb\xbf'+SOURCE.replace('\n','\r\n').encode())
    protected={str(p):sha(p) for p in (compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'),entry)}; results=[]; status='failed'; started=datetime.datetime.now(datetime.timezone.utc).isoformat()
    env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''))
    def done(name,**data): results.append({'name':name,'passed':True,**data})
    def run(label,args,tool=compiler,expected=0):
        cmd=[str(tool),*map(str,args)]; (work/(label+'.args.json')).write_text(json.dumps(cmd,ensure_ascii=False,indent=2),encoding='utf8')
        p=subprocess.run(cmd,cwd=src,env=env,capture_output=True,timeout=120); (work/(label+'.stdout')).write_bytes(p.stdout); (work/(label+'.stderr')).write_bytes(p.stderr)
        assert p.returncode==expected,(label,p.returncode,p.stdout[-1500:],p.stderr[-1500:]); return p
    def query(label,path,extra=()): return json.loads(run(label,['c-bridge',path,'--json',*extra]).stdout)
    def validate(label,value,texts,index):
        assert value['ok'] and value['sourceReport']['complete'] and value['cBridge']['complete']; bridge=value['cBridge']; rows=bridge['declarationMap']['records']; header=bridge['declarations'].encode(); facts=index['semanticIndex']['nodes']; ranges=[]; types=[]
        assert bridge['declarationMap']['complete']; done(label+'-complete')
        for row in rows:
            span=row['headerRange']; start,end=span['startOffset'],span['endOffset']; assert header[start:end]==row['cName'].encode() and start<end; ranges.append((start,end))
            target=row['target']; assert target; file=value['sourceReport']['files'][target['sourceId']-1]; data=texts[Path(file['path']).name].removeprefix(b'\xef\xbb\xbf'); assert file['textSha256']==hashlib.sha256(data).hexdigest()
            inner,outer=target['selectionRange'],target['range']; assert outer['startOffset']<=inner['startOffset']<inner['endOffset']<=outer['endOffset']<=len(data)
            for span in (inner,outer):
                for side in ('start','end'):
                    before=data[:span[side+'Offset']].decode(); assert span[side+'Line']==before.count('\n')+1 and span[side+'Column']==len(before.rsplit('\n',1)[-1])+1
            if row['kind']=='type':
                n=next(n for n in facts if n['kind']=='union' and n['declaredSymbolId']==row['symbolId']); assert row['typeId']==n['declaredTypeId']==n['typeId'] and row['typeAbiId']==n['typeAbiId']!='0000000000000000'; assert n['sourceId']==target['sourceId'] and n['selectionRange']==inner and n['range']==outer
                types.append(row)
        assert ranges==sorted(ranges) and all(a[1]<=b[0] for a,b in zip(ranges,ranges[1:])); done(label+'-exact-header-source-and-semantic-facts',records=len(rows))
        groups={}
        for row in types: groups.setdefault(row['symbolId'],[]).append(row)
        for pair in groups.values():
            assert len(pair)==2 and pair[0]['target']==pair[1]['target'] and pair[0]['typeId']==pair[1]['typeId'] and pair[0]['typeAbiId']==pair[1]['typeAbiId'] and pair[0]['cName']==pair[1]['cName']; start=pair[0]['headerRange']['startOffset']; end=pair[1]['headerRange']['endOffset']; assert header[:start].endswith(b'typedef union ') and header[end:end+1]==b';'
        done(label+'-tag-and-typedef-share-one-real-identity',nominalTypes=len(groups)); return rows,types
    try:
        index=json.loads(run('index',['check',entry,'--json','--source-report','--semantic-index']).stdout); value=query('disk',entry); rows,types=validate('disk',value,{'entry.xl':entry.read_bytes()},index)
        names={next(n['text'] for n in index['semanticIndex']['nodes'] if n['kind']=='union' and n['declaredSymbolId']==r['symbolId']):r['cName'] for r in types}; assert set(names)=={'Number','Outer'}; done('public-reachability-and-alias-canonicalization')
        assert all(r['kind']!='type' or r in types for r in rows) and not any(s in value['cBridge']['declarations'] for s in ('XL_Union_Unused','XL_Union_Alias','XL_Union_Scalar','XL_Union_Maybe')); done('erased-aliases-and-structural-carriers-have-no-invented-binding')
        for label,tool in [('new',compiler)]+([('old',a.baseline.resolve())] if a.baseline else []):
            folder=src/label; folder.mkdir(); run(label+'-plain',['cgen',entry,'--emit-header',folder/'module.h','--emit',folder/'module.c'],tool)
            assert (folder/'module.h').read_bytes()==value['cBridge']['declarations'].encode(); done(label+'-plain-header-byte-equivalence')
            if label=='old':
                previous=json.loads(run('old-index',['check',entry,'--json','--source-report','--semantic-index'],tool).stdout); assert previous['semanticIndex']==index['semanticIndex']; previous=json.loads(run('old-bridge',['c-bridge',entry,'--json'],tool).stdout); assert previous['cBridge']['declarations']==value['cBridge']['declarations']; assert previous['cBridge']['declarationMap']['records']==[r for r in rows if r['kind']!='type']; done('baseline-runtime-identities-and-existing-symbol-bindings-unchanged')
        overlay=src/'entry.snapshot'; overlay.write_bytes(entry.read_bytes().replace(b'// ',b'// draft\r\n// ',1)); protected[str(overlay)]=sha(overlay); changed=query('overlay',entry,['--source-overlay',entry,overlay]); changedIndex=json.loads(run('overlay-index',['check',entry,'--json','--source-report','--semantic-index','--source-overlay',entry,overlay]).stdout); validate('overlay',changed,{'entry.xl':overlay.read_bytes()},changedIndex); assert changed['cBridge']['declarations']==value['cBridge']['declarations'] and changed['cBridge']['declarationMap']['records'][0]['target']!=types[0]['target']; done('unsaved-overlay-source-with-identical-public-abi')
        helper=src/'helper.xl'; helper.write_bytes('\ufeff// 中文 😀 dependency\r\nexport union Imported\r\n c_int integer\r\n c_double real\r\nend\r\n'.encode()); imported=src/'imported.xl'; imported.write_text('import helper\nexport "imported_read" func read(helper.Imported input) as c_int\n return input.integer\nend\n',encoding='utf8')
        protected.update({str(p):sha(p) for p in (helper,imported)}); depValue=query('imported',imported); depIndex=json.loads(run('imported-index',['check',imported,'--json','--source-report','--semantic-index']).stdout); _,depTypes=validate('imported',depValue,{p.name:p.read_bytes() for p in (helper,imported)},depIndex); assert len(depTypes)==2 and Path(depValue['sourceReport']['files'][depTypes[0]['target']['sourceId']-1]['path'])==helper; done('closed-bom-crlf-imported-type-declaration')
        depDraft=src/'helper.snapshot'; depDraft.write_bytes(helper.read_bytes().replace(b'// ',b'// unsaved\r\n// ',1)); protected[str(depDraft)]=sha(depDraft); depValue=query('imported-overlay',imported,['--source-overlay',helper,depDraft]); depIndex=json.loads(run('imported-overlay-index',['check',imported,'--json','--source-report','--semantic-index','--source-overlay',helper,depDraft]).stdout); validate('imported-overlay',depValue,{'helper.xl':depDraft.read_bytes(),'imported.xl':imported.read_bytes()},depIndex)
        native=src/'native.c'; native.write_text('int native_answer(void) { '+names['Number']+' value; value.integer=42; '+names['Outer']+' result=take_number(value); return result.nested.integer; }\n',encoding='utf8'); project=src/'app.xlp'; project.write_text(json.dumps({'schema':1,'name':'types','kind':'exe','entry':'entry.xl','files':['entry.xl','native.c'],'cSources':['native.c']}),encoding='utf8')
        protected.update({str(p):sha(p) for p in (native,project)}); run('mixed-build',['build',project,'--save-temps','-o',src/'mixed.exe']); run('mixed-execute',[],src/'mixed.exe'); done('actual-c-nominal-types-mixed-build-and-execute-42')
        status='passed'
    finally:
        for p,h in protected.items(): assert sha(Path(p))==h,p
        evidence={'status':status,'startedUtc':started,'finishedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(compiler),'baseline':str(a.baseline) if a.baseline else None,'results':results,'protected':protected}; (work/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf8'); print(f"Public C type emission checks: {len(results)} passed; {work/'evidence.json'}")

if __name__=='__main__': main()
