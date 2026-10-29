"""Exercise IDE query contracts on the same actual Windows/Linux products.

Only generated assertion inputs and the three pinned products enter isolation.
This CLI gate complements the larger Windows API/IDE suite and source-unit
fault checks; it does not claim whole-compiler OOM or full53 acceptance.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists();windows=os.name=='nt'
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    names=('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so')
    products=[root/'release'/n for n in names]
    recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert all(recorded.get(p)==sha(p) for p in products)
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in [Path(__file__).resolve(),Path(sys.executable),args.build_report.resolve(),*products]]
    out.mkdir();isolated=out/'isolated';isolated.mkdir();work=isolated/'source';work.mkdir()
    for p in products:shutil.copy2(p,isolated/p.name)
    exe=isolated/names[0]
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
         ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT')}
    env['PATH']=str(isolated)+os.pathsep+os.environ.get('PATH',os.environ.get('Path',''))
    rows=[];proofs=[];failures=[];artifacts=[]
    def put(name,text,bom=False):
        p=work/name;data=text.replace('\n','\r\n').encode('utf-8')
        p.write_bytes((b'\xef\xbb\xbf' if bom else b'')+data)
        artifacts.append(p);return p
    def run(name,command,expected=0):
        command=[str(exe),*map(str,command)]
        result=subprocess.run(command,cwd=work,env=env,capture_output=True,timeout=180)
        stdout=out/(name+'.stdout');stderr=out/(name+'.stderr')
        stdout.write_bytes(result.stdout);stderr.write_bytes(result.stderr)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,
                         Logs=[dict(Path=str(p),Sha256=sha(p)) for p in (stdout,stderr)]))
        assert result.returncode==expected,(name,result.returncode,result.stdout[-1500:],result.stderr[-1500:])
        return result.stdout
    def report(value,path,expected,origin='disk'):
        assert value['sourceReport']['complete']
        files=[r for r in value['sourceReport']['files'] if Path(r['path']).resolve()==path.resolve()]
        assert len(files)==1
        row=files[0];assert row['origin']==origin and row['kind']&1
        assert row['textSize']==len(expected) and row['textSha256']==hashlib.sha256(expected).hexdigest()
        return row
    def position(data,offset):
        text=data[:offset].decode('utf-8').replace('\r\n','\n').replace('\r','\n')
        return len(text.split('\n')),len(text.rsplit('\n',1)[-1])+1
    def ranges(value,path,data):
        assert value['schema']==1 and value['scope']=='entry-syntax'
        assert value['offsetEncoding']=='utf-8-after-bom' and value['columnEncoding']=='unicode-scalar-1-based'
        assert value['rangeConvention']=='half-open'
        stack=[value['ast']];count=0
        while stack:
            node=stack.pop();count+=1;assert Path(node['source']).resolve()==path.resolve()
            stack.extend(node.get('children',[]))
            for key in ('range','tokenRange'):
                span=node[key]
                if span is None:continue
                a,b=span['startOffset'],span['endOffset'];assert 0<=a<=b<=len(data)
                assert position(data,a)==(span['startLine'],span['startColumn'])
                assert position(data,b)==(span['endLine'],span['endColumn'])
                data[a:b].decode('utf-8')
            if node['range'] and node['tokenRange']:
                a,b=node['range'],node['tokenRange']
                assert a['startOffset']<=b['startOffset']<=b['endOffset']<=a['endOffset']
        assert count>10
    try:
        source=put('query.xl','export "ide_answer" func answer(int value) as int\n int 数量 = value\n return 数量 + 1\nend\nfunc main(...) as int\n return answer(41) - 42\nend\n',True)
        text=source.read_bytes().removeprefix(b'\xef\xbb\xbf')
        plain=json.loads(run('plain-check',['check',source,'--json']))
        assert plain['ok'] and 'sourceReport' not in plain and 'semanticIndex' not in plain
        value=json.loads(run('source-report',['check',source,'--json','--source-report']))
        report(value,source,text);assert value['ok'];proofs.append('exact-source-report')
        value=json.loads(run('semantic-index',['check',source,'--json','--semantic-index']))
        report(value,source,text)
        assert value['ok'] and value['semanticIndex']['complete'] and value['semanticIndex']['nodes']
        proofs.append('checked-semantic-index')
        value=json.loads(run('ranges',['parse',source,'--json','--source-ranges']))
        ranges(value,source,text);proofs.append('source-ranges-json')
        value=json.loads(run('bridge',['c-bridge',source,'--json']))
        report(value,source,text)
        bridge=value['cBridge'];assert value['ok'] and bridge['complete'] and 'ide_answer' in bridge['declarations']
        assert bridge['declarationMap']['complete'] and bridge['declarationMap']['records']
        badName=put('bad-c-name.xl','export "bad_c_name" func answer(int 数量) as int\n return 数量\nend\n')
        rejected=json.loads(run('bridge-public-name-control',['c-bridge',badName,'--json'],1))
        assert not rejected['cBridge']['complete'] and not rejected['cBridge']['declarationMap']['records']
        assert any('canonical public name/value ABI' in r['message'] for r in rejected['diagnostics'])
        proofs.append('lowered-public-c-bridge')
        run('plain-execution',['run',source]);proofs.append('ordinary-execution-unaffected')
        partial=put('partial.xl','func calc(int alphabet) as int\n return al\nend\n',True)
        raw=partial.read_bytes().removeprefix(b'\xef\xbb\xbf');offset=raw.index(b'return al')+len(b'return al')
        value=json.loads(run('completion',['check',partial,'--json','--completion-at',partial,offset]))
        report(value,partial,raw);q=value['completion']
        assert value['ok'] and q['complete'] and not q['checkedCompilation'] and q['prefix']=='al'
        match=[r for r in q['items'] if r['name']=='alphabet'];assert len(match)==1 and match[0]['canonicalType']=='int'
        span=match[0]['selectionRange'];assert raw[span['startOffset']:span['endOffset']]==b'alphabet'
        proofs.append('completion-source-identity-and-selection')
        value=json.loads(run('partial-normal-rejected',['check',partial,'--json'],1));assert not value['ok']
        proofs.append('query-not-valid-compilation-control')
        dirty=put('dirty.snapshot','func calc(int dirtyName) as int\n return dir\nend\n')
        raw=dirty.read_bytes();offset=raw.index(b'return dir')+len(b'return dir');saved=partial.read_bytes()
        value=json.loads(run('overlay-completion',['check',partial,'--json','--completion-at',partial,offset,'--source-overlay',partial,dirty]))
        report(value,partial,raw,'overlay');assert value['completion']['complete']
        assert any(r['name']=='dirtyName' for r in value['completion']['items']) and partial.read_bytes()==saved
        proofs.append('unsaved-overlay-retains-disk')
        bad=put('bad-offset.xl','func test(int 数量) as int\n return 数\nend\n')
        offset=bad.read_bytes().index('return 数'.encode())+len(b'return ')+1
        value=json.loads(run('utf8-interior-rejected',['check',bad,'--json','--completion-at',bad,offset],1))
        assert not value['completion']['complete'] and not value['completion']['items']
        proofs.append('utf8-interior-control')
        run('report-json-required',['check',source,'--source-report'],1)
        run('completion-index-conflict',['check',partial,'--json','--semantic-index','--completion-at',partial,0],1)
        proofs.append('query-option-controls')
    except Exception as error:failures.append(repr(error))
    unchanged=all(sha(r['Path'])==r['Sha256'] for r in inputs)
    report=dict(Complete=unchanged and not failures and len(proofs)==10,InputsUnchanged=unchanged,
                Inputs=inputs,Products=[dict(Path=str(p),Sha256=sha(p)) for p in artifacts+list(isolated.glob('*')) if p.is_file()],
                Rows=rows,Proofs=proofs,Failures=failures,Scope=__doc__,ActualCompilerProductAcceptance=True,Full53Acceptance=False)
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
