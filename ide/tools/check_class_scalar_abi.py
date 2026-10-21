"""Actual class C scalar defaults, public bridge, overlays and mixed execution."""
import argparse, datetime, hashlib, json, os, subprocess, uuid
from pathlib import Path

TYPES = ('c_char','c_schar','c_uchar','c_short','c_ushort','c_int','c_uint',
         'c_long','c_ulong','c_longlong','c_ulonglong','c_size','c_ssize',
         'c_ptrdiff','c_intptr','c_uintptr','c_bool','c_float','c_double','c_wchar','float32')

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--compiler',type=Path,required=True);ap.add_argument('--baseline',type=Path);ap.add_argument('--original-fixture',type=Path)
    a=ap.parse_args();ide=Path(__file__).resolve().parents[1];compiler=a.compiler.resolve();work=ide/'build/targeted'/('class-scalars-'+uuid.uuid4().hex);work.mkdir(parents=True)
    protected={str(p):sha(p) for p in (compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll'))};results=[];status='failed';started=datetime.datetime.now(datetime.timezone.utc).isoformat()
    env=dict(os.environ,PATH=str(compiler.parent)+os.pathsep+os.environ.get('PATH',''))
    def done(name,**data):results.append({'name':name,'passed':True,**data})
    def run(label,args,tool=compiler,expected=0):
        command=[str(tool),*map(str,args)];(work/(label+'.args.json')).write_text(json.dumps(command,ensure_ascii=False,indent=2),encoding='utf8')
        p=subprocess.run(command,cwd=work,env=env,capture_output=True,timeout=120);(work/(label+'.stdout')).write_bytes(p.stdout);(work/(label+'.stderr')).write_bytes(p.stderr)
        assert p.returncode==expected,(label,p.returncode,p.stdout[-1500:],p.stderr[-1500:]);return p
    def bridge(label,path,extra=()):
        value=json.loads(run(label,['c-bridge',path,'--json',*extra]).stdout)
        assert value['ok'] and value['sourceReport']['complete'] and value['cBridge']['complete']
        assert value['cBridge']['declarationMap']['complete'];return value
    def protect(path):protected[str(path)]=sha(path)
    try:
        if a.original_fixture:
            original=work/'original-class.xl';original.write_bytes(a.original_fixture.read_bytes());protect(original)
            bridge('original-fixed',original);done('original-reported-class-layout-failure-fixed')
            if a.baseline:
                old=json.loads(run('original-baseline',['c-bridge',original,'--json'],a.baseline.resolve(),1).stdout)
                assert not old['ok'] and any('class layout arena is malformed' in d['message'] for d in old['diagnostics']);done('original-baseline-failure-retained')
        baseline=[]
        for number,kind in enumerate(TYPES):
            path=work/('scalar-'+str(number)+'.xl')
            path.write_text('class Scalar\n '+kind+' value\nend\nexport "scalar_read" func read(Scalar input) as '+kind+'\n return input.value\nend\n',encoding='utf8');protect(path)
            value=bridge('scalar-'+str(number),path)
            assert any(row['kind']=='function' and row['cName']=='scalar_read' and row['target'] for row in value['cBridge']['declarationMap']['records'])
            done('default-class-field-'+kind)
            if a.baseline:
                command=[str(a.baseline.resolve()),'c-bridge',str(path),'--json'];p=subprocess.run(command,cwd=work,env=env,capture_output=True,timeout=120)
                (work/('baseline-'+str(number)+'.stdout')).write_bytes(p.stdout);(work/('baseline-'+str(number)+'.stderr')).write_bytes(p.stderr);old=json.loads(p.stdout)
                assert p.returncode in (0,1) and (old['ok'] or any('class layout arena is malformed' in d['message'] for d in old['diagnostics']))
                baseline.append({'type':kind,'exitCode':p.returncode,'ok':old['ok']})
        if a.baseline:
            (work/'baseline-matrix.json').write_text(json.dumps(baseline,indent=2),encoding='utf8');assert any(not x['ok'] for x in baseline);done('baseline-matrix-recorded',rows=len(baseline))
        fields=''.join(' '+kind+' field'+str(i)+'\n' for i,kind in enumerate(TYPES))
        checks=''.join(' if input.field'+str(i)+' != '+('false' if kind=='c_bool' else '0.0' if kind in ('c_float','c_double','float32') else '0')+'\n  return '+str(i+1)+'\n end\n' for i,kind in enumerate(TYPES))
        writes=''.join(' input.field'+str(i)+' = '+('true' if kind=='c_bool' else '3.5' if kind in ('c_float','c_double','float32') else '7')+'\n' for i,kind in enumerate(TYPES))
        reads=''.join(' if input.field'+str(i)+' != '+('true' if kind=='c_bool' else '3.5' if kind in ('c_float','c_double','float32') else '7')+'\n  return '+str(i+51)+'\n end\n' for i,kind in enumerate(TYPES))
        source='class Scalars\n'+fields+'end\nclass Derived extends Scalars\n c_int tail\nend\nclass Box<T>\n T value\nend\nexport "scalar_make" func make() as Derived\n return Derived()\nend\nexport "scalar_check" func verify(Derived input) as c_int\n'+checks+' if input.tail != 0\n  return 100\n end\n'+writes+reads+' input.tail = 42\n Box<c_int> boxed = Box::<c_int>()\n if boxed.value != 0\n  return 101\n end\n boxed.value = input.tail\n return boxed.value\nend\nextern "native_class_test" func nativeTest() as c_int\nfunc main(...) as int\n return nativeTest() - 42\nend\n'
        path=work/'matrix.xl';path.write_bytes(b'\xef\xbb\xbf'+source.replace('\n','\r\n').encode());protect(path)
        value=bridge('matrix',path);index=json.loads(run('matrix-index',['check',path,'--json','--source-report','--semantic-index']).stdout);assert index['ok']
        done('mixed-native-scalar-inherited-generic-class-bridge-and-index')
        draft=work/'matrix.snapshot';draft.write_bytes(path.read_bytes().replace(b'class Scalars',b'// unsaved \xe4\xb8\xad\xf0\x9f\x98\x80\r\nclass Scalars',1));protect(draft)
        overlay=bridge('matrix-overlay',path,['--source-overlay',path,draft]);assert overlay['cBridge']['declarations']==value['cBridge']['declarations'];done('unsaved-class-overlay-retains-exact-public-abi')
        header=work/'module.h';generated=work/'module.c';run('matrix-cgen',['cgen',path,'--emit-header',header,'--emit',generated]);assert header.read_bytes()==value['cBridge']['declarations'].encode();done('class-plain-header-matches-bridge-byte-for-byte')
        native=work/'native.c';native.write_text('void xrtValueRelease(xvalue* value);\n\nint native_class_test(void)\n{\n    xvalue* item = scalar_make();\n    if (!item) return -1;\n    int code = scalar_check(item);\n    xrtValueRelease(item);\n    return code;\n}\n',encoding='utf8');protect(native)
        project=work/'matrix.xlp';project.write_text(json.dumps({'schema':1,'name':'class-scalars','kind':'exe','entry':'matrix.xl','files':['matrix.xl','native.c'],'cSources':['native.c']}),encoding='utf8');protect(project)
        executable=work/'matrix.exe';run('mixed-build',['build',project,'-o',executable]);run('mixed-execute',[],executable);done('actual-c-class-create-defaults-assign-read-release-execute-42')
        status='passed'
    finally:
        for p,h in protected.items():assert sha(Path(p))==h,p
        evidence={'status':status,'startedUtc':started,'finishedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'compiler':str(compiler),'baseline':str(a.baseline) if a.baseline else None,'results':results,'protected':protected};(work/'evidence.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf8');print(f"Class scalar ABI checks: {len(results)} passed; {work/'evidence.json'}")

if __name__=='__main__':main()
