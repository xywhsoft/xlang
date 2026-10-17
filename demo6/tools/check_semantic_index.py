"""Same-compilation semantic IDs, types, provenance and opt-in compatibility gate."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid

FIXTURES = {'alias.xl': 'typedef 数字 = int\nfunc main(...) as int\n    数字 值 = 4\n    auto 副本 = 值\n    return 副本 - 4\nend\n', 'class.xl': 'class Box\n    int value\n    create(int value)\n        self.value = value\n    end\n    func getValue() as int\n        return self.value\n    end\nend\nfunc main(...) as int\n    auto box = Box(3)\n    return box.getValue() - 3\nend\n', 'helper.xl': 'export "semantic_answer" func answer() as int\n    return 42\nend\n', 'import.xl': 'import "helper.xl"\nfunc main(...) as int\n    return helper.answer() - 42\nend\n', 'overload.xl': 'func pick(int value) as int\n    return value\nend\nfunc pick(string value) as int\n    return 2\nend\nfunc main(...) as int\n    auto first = pick(1)\n    auto second = pick("😀")\n    return first + second - 3\nend\n', 'shadow.xl': 'int 值 = 9\nfunc 计算(int 值) as int\n    if true\n        auto 值 = 2\n        assert 值 == 2\n    end\n    return 值\nend\nfunc main(...) as int\n    return 计算(值) - 9\nend\n'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def position(data, offset):
    text=data[:offset].decode('utf-8')
    line,column,i=1,1,0
    while i<len(text):
        if text[i]=='\r':
            if i+1<len(text) and text[i+1]=='\n': i+=1
            line,column=line+1,1
        elif text[i]=='\n': line,column=line+1,1
        else: column+=1
        i+=1
    return line,column


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    ap.add_argument('--compiler',type=Path)
    ap.add_argument('--gcc',default='gcc')
    args=ap.parse_args()
    root=args.root.resolve(); exe=(args.compiler or root/'release/xl.exe').resolve()
    gcc=shutil.which(args.gcc)
    if os.name!='nt' or not gcc: raise RuntimeError('Windows and MinGW GCC required')
    work=root/'build/test'/('semantic-index-'+str(uuid.uuid4())); src=work/'源码 Ω';src.mkdir(parents=True)
    env=dict(os.environ,PATH=str(exe.parent)+os.pathsep+os.environ.get('PATH',''))
    protected={str(p):sha(p) for p in [exe,exe.with_name('xlc.dll'),exe.with_name('xl.dll')]}
    started=datetime.datetime.now(datetime.timezone.utc).isoformat()
    results=[]; checked_nodes=0; checked_ranges=0; status='failed'
    raw={}
    for name,text in FIXTURES.items():
        (src/name).write_text(text,encoding='utf-8',newline='\n');raw[name]=text.encode('utf-8')
    def run(name,cmd,expected=0):
        c=subprocess.run([str(x) for x in cmd],cwd=src,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
        (work/(name+'.stdout')).write_bytes(c.stdout);(work/(name+'.stderr')).write_bytes(c.stderr)
        results.append({'name':name,'exitCode':c.returncode,'stdoutBytes':len(c.stdout),'stderrBytes':len(c.stderr)})
        assert c.returncode==expected,(name,c.returncode,c.stdout[:400],c.stderr[:400])
        return c
    def payload(c): return json.loads(c.stdout.decode('utf-8-sig'))
    def check(name,path,extra=(),expected=0): return payload(run(name,[exe,'check',path,'--json','--semantic-index',*extra],expected))
    def data_for(report,source):
        item=report['files'][source-1]
        path=Path(item['path']); assert path.is_absolute()
        return raw.get(path.name,path.read_bytes() if path.exists() else b'').removeprefix(b'\xef\xbb\xbf')
    def validate(value):
        nonlocal checked_nodes,checked_ranges
        assert value['ok'] and value['sourceReport']['complete']
        ix=value['semanticIndex']; assert ix['schema']==1 and ix['scope']=='checked-compilation' and ix['complete']
        assert ix['offsetEncoding']=='utf-8-after-bom' and ix['columnEncoding']=='unicode-scalar-1-based' and ix['rangeConvention']=='half-open'
        rows=ix['nodes']; assert rows and rows[0]['parent']==0
        ids=set()
        for n in rows:
            checked_nodes+=1; assert n['id']==len(ids)+1;assert n['parent'] in ids or n['parent']==0;ids.add(n['id'])
            for key,source in [('range',n['sourceId']),('tokenRange',n['sourceId']),('selectionRange',n['selectionSourceId'])]:
                span=n[key]
                if span is None: continue
                checked_ranges+=1;assert 1<=source<=len(value['sourceReport']['files']);data=data_for(value['sourceReport'],source)
                a,b=span['startOffset'],span['endOffset']; assert 0<=a<=b<=len(data),(n,key,len(data))
                assert position(data,a)==(span['startLine'],span['startColumn']),(n,key)
                assert position(data,b)==(span['endLine'],span['endColumn']),(n,key)
                data[a:b].decode('utf-8')
            if n['declaredSymbolId']: assert n['declaredSymbolId']==n['symbolId']
            if n['type']: assert n['type']['origin'] in ('syntax','declaration','inferred') and isinstance(n['type']['canonical'],str)
        return rows
    def selection(value,n):
        span=n['selectionRange']; assert span
        data=data_for(value['sourceReport'],n['selectionSourceId'])
        return data[span['startOffset']:span['endOffset']].decode('utf-8')
    def nofacts(value):
        assert not value['ok'] and not value['semanticIndex']['complete'] and value['semanticIndex']['nodes']==[]
    try:
        shadow=check('shadow','shadow.xl'); rows=validate(shadow)
        declarations=[n for n in rows if n['declaredSymbolId'] and n['text']=='值']
        assert len(declarations)==3 and len({n['declaredSymbolId'] for n in declarations})==3
        byline={n['selectionRange']['startLine']:n['declaredSymbolId'] for n in declarations}
        assert set(byline)=={1,2,4}
        for refline,declline in [(5,4),(7,2),(10,1)]:
            refs=[n for n in rows if n['kind']=='name' and n['text']=='值' and n['range']['startLine']==refline]
            assert len(refs)==1 and not refs[0]['declaredSymbolId'] and refs[0]['targetSymbolId']==byline[declline]
        assert all(selection(shadow,n)=='值' and n['type']['canonical']=='int' for n in declarations)
        overload=check('overload','overload.xl'); rows=validate(overload)
        decls={n['declaredSymbolId']:n for n in rows if n['declaredSymbolId']}
        calls=[n for n in rows if n['kind']=='call' and n['targetSymbolId'] and n['range']['startLine'] in (8,9)]
        assert len(calls)==2 and len({n['targetSymbolId'] for n in calls})==2
        assert {decls[n['targetSymbolId']]['range']['startLine'] for n in calls}=={1,4}
        assert all(selection(overload,decls[n['targetSymbolId']])=='pick' for n in calls)
        variables=[n for n in rows if n['declaredSymbolId'] and n['text'] in ('first','second')]
        assert len(variables)==2 and all(n['type']['origin']=='declaration' and n['type']['canonical']=='int' for n in variables)
        imported=check('import','import.xl'); rows=validate(imported)
        call=next(n for n in rows if n['kind']=='call' and n['range'] and n['range']['startLine']==3)
        decl=next(n for n in rows if n['declaredSymbolId']==call['targetSymbolId'])
        assert selection(imported,decl)=='answer'
        assert Path(imported['sourceReport']['files'][decl['selectionSourceId']-1]['path']).name=='helper.xl'
        assert imported['sourceReport']['files'][decl['selectionSourceId']-1]['kind'] & 1
        assert decl['text']!='answer' or any(n['text'] and '__xl6_import_' in n['text'] for n in rows)
        replacement='export "semantic_answer" func answer() as int\n    auto replacement = 7\n    return replacement\nend\n'
        (src/'helper.snapshot').write_text(replacement,encoding='utf-8',newline='\n');raw['helper.xl']=replacement.encode()
        # Existing file-target package probing reads disk metadata before source
        # overlay loading. Keep its fail-closed behavior visible; do not claim
        # this compiler-index increment solves the shared report limitation.
        nofacts(check('explicit-file-overlay-report-conflict','import.xl',['--source-overlay','helper.xl','helper.snapshot'],expected=1))
        module_source=FIXTURES['import.xl'].replace('import "helper.xl"','import helper')
        (src/'module.xl').write_text(module_source,encoding='utf-8',newline='\n');raw['module.xl']=module_source.encode()
        overlay=check('module-overlay','module.xl',['--source-overlay','helper.xl','helper.snapshot']);rows=validate(overlay)
        assert any(n['declaredSymbolId'] and n['text']=='replacement' for n in rows)
        assert next(f for f in overlay['sourceReport']['files'] if Path(f['path']).name=='helper.xl')['origin']=='overlay'
        raw['helper.xl']=FIXTURES['helper.xl'].encode()
        alias=check('alias','alias.xl'); rows=validate(alias)
        alias_decl=next(n for n in rows if n['kind']=='type_alias')
        assert alias_decl['typeId']>0 and selection(alias,alias_decl)=='数字'
        uses=[n for n in rows if n['kind']=='type' and n['text']=='数字']
        assert uses and all(n['typeId']==alias_decl['typeId'] and n['type']['canonical']=='int' for n in uses)
        cls=check('class','class.xl');rows=validate(cls)
        class_decl=next(n for n in rows if n['kind']=='class'); assert class_decl['typeId']>0 and selection(cls,class_decl)=='Box'
        auto_box=next(n for n in rows if n['text']=='box' and n['declaredSymbolId'])
        assert auto_box['type']['id']==class_decl['typeId'] and auto_box['type']['origin']=='declaration'
        bom=b'\xef\xbb\xbf'+FIXTURES['shadow.xl'].replace('\n','\r\n').rstrip('\r\n').encode();(src/'中文 Ω.xl').write_bytes(bom);raw['中文 Ω.xl']=bom
        validate(check('unicode-bom-crlf','中文 Ω.xl'))
        fstring='int value = 2\nauto text = f"😀值 {value + 1}"\n'
        (src/'fstring.xl').write_text(fstring,encoding='utf-8',newline='\n');raw['fstring.xl']=fstring.encode();validate(check('fstring','fstring.xl'))
        empty=src/'empty.xl';empty.write_bytes(b'');raw['empty.xl']=b'';validate(check('empty','empty.xl'))
        invalid=src/'invalid.xl';invalid.write_text('func broken() as int\n return missing\nend\n',encoding='utf-8',newline='\n');nofacts(check('invalid-semantic','invalid.xl',expected=1))
        invalid.write_text('func incomplete(\n',encoding='utf-8',newline='\n');nofacts(check('invalid-parse','invalid.xl',expected=1))
        nofacts(check('missing-source','missing.xl',expected=1))
        (src/'deep.xl').write_text('func deep() as int\n return '+'1 + '*280+'1\nend\n',encoding='utf-8',newline='\n')
        run('deep-ordinary-check',[exe,'check','deep.xl','--json'])
        nofacts(check('deep-index-limit','deep.xl',expected=1))
        run('index-needs-json',[exe,'check','shadow.xl','--semantic-index'],1)
        run('index-command-rejected',[exe,'parse','shadow.xl','--json','--semantic-index'],1)
        (src/'--semantic-index').write_text(FIXTURES['helper.xl'],encoding='utf-8',newline='\n')
        plain=payload(run('emit-value',[exe,'check','shadow.xl','--json','--emit','--semantic-index']))
        assert 'semanticIndex' not in plain
        plain=payload(run('boundary-flag',[exe,'check','shadow.xl','--json','--','--semantic-index']))
        assert 'semanticIndex' not in plain
        plain=payload(run('legacy-json',[exe,'check','shadow.xl','--json']))
        assert set(plain)=={'ok','phase','diagnostics'}
        plain=payload(run('legacy-report',[exe,'check','shadow.xl','--json','--source-report']))
        assert set(plain)=={'ok','phase','diagnostics','sourceReport','resolutionReport'}
        both=check('both-flags','shadow.xl',['--source-report']);validate(both)
        # Compile hosts against the current public header and actual query implementation.
        host=work/'semantic-index-host.exe'
        run('host-build',[gcc,'-std=c11','-Wall','-Wextra','-Werror','-municode','-I',root/'release',root/'test/cases/semantic_index_host.c','-o',host])
        run('host-execute',[host,exe.with_name('xlc.dll'),src])
        raw['native-memory.xl']=b'\xef\xbb\xbffunc native(int value) as int\r\n    auto copy = value\r\n    return copy\r\nend'
        memory=json.loads((src/'native-memory.json').read_text(encoding='utf-8-sig'));validate(memory)
        assert next(f for f in memory['sourceReport']['files'] if Path(f['path']).name=='native-memory.xl')['origin']=='memory'
        authoritative=json.loads((src/'native-string-authoritative.json').read_text(encoding='utf-8-sig'));validate(authoritative)
        assert memory==authoritative
        raw['native-virtual.xl']=b'func replacement() as int\n    return 2\nend\n'
        validate(json.loads((src/'native-virtual.json').read_text(encoding='utf-8-sig')))
        nofacts(json.loads((src/'native-invalid.json').read_text(encoding='utf-8-sig')))
        validate(json.loads((src/'native-recovery.json').read_text(encoding='utf-8-sig')))
        assert 'semanticIndex' not in json.loads((src/'native-legacy.json').read_text(encoding='utf-8-sig'))
        bounds=work/'semantic-index-bounds.exe'
        run('bounds-build',[gcc,'-std=c11','-Wall','-Wextra','-Werror','-I',root,'-I',root/'release','-I',root/'release/include',root/'test/cases/semantic_index_bounds_host.c',root/'src/ast/xl6_ast.c',root/'src/type/xl6_type_model.c',root/'src/common/xl6_output.c',root/'src/sem/xl6_sem_index.c','-L'+str(exe.parent),'-l:xl.dll','-o',bounds])
        run('bounds-execute',[bounds])
        status='passed'
    finally:
        assert all(sha(Path(p))==h for p,h in protected.items())
        evidence={'status':status,'startedUtc':started,'finishedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(exe),'commands':len(results),'nodesVerified':checked_nodes,'rangesVerified':checked_ranges,'results':results,'protected':protected}
        (work/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf-8',newline='\n')
        print(json.dumps({'status':status,'commands':len(results),'nodesVerified':checked_nodes,'rangesVerified':checked_ranges,'evidence':str(work/'evidence.json')},ensure_ascii=False))


if __name__=='__main__':
    main()
