"""Verify included C prototypes remain authoritative, including signature refusal."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess
from xlir_public_build_inputs import verify_public_build
from runtime_layout import runtime_directory

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','public-directory','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root,public,out=[x.resolve() for x in (a.root,a.public_directory,a.out)]
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver']);cc=shutil.which('gcc');assert cc
    pinned.update((Path(__file__).resolve(),Path(cc),root/'test/cases/foreign_managed_view.h'))
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs={str(p):sha(p) for p in pinned};rows=[]
    out.mkdir(parents=True,exist_ok=False);env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root))
    sdk=runtime_directory(root) if os.name=='nt' else Path(build['SdkDirectory'])
    if os.name!='nt':env['LD_LIBRARY_PATH']=str(Path(build['RuntimeLibrary']).parent)+':'+env.get('LD_LIBRARY_PATH','')
    def run(name,command,refused=False):
        r=subprocess.run(list(map(str,command)),cwd=root,env=env,capture_output=True,timeout=60);log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(name=name,exitCode=r.returncode,refused=refused,logSha256=sha(log)));assert r.returncode==(1 if refused else 0),log
        if refused:assert b'declared C function ABI' in r.stdout+r.stderr,log
    for name,result,params,args in [('exact','c_int','',''),('wrong-result','c_double','',''),('wrong-parameter','c_int','c_int extra','0')]:
        source=out/(name+'.xl');generated=out/(name+'.c')
        source.write_text('extern "ManagedRoot*" class Raw @header("test/cases/foreign_managed_view.h") @lifetime("caller")\n'
            ' native point handle\n extern "managed_root_read" @no_raise func read('+params+') as '+result+'\nend\n'
            'export "probe_header" func probe(Raw raw) as '+result+'\n return raw.read('+args+')\nend\n'
            'func main(...) as int\n return 0\nend\n',encoding='utf8')
        run(name+'-cgen',[driver,source,'cgen-default',generated]);text=generated.read_text(encoding='utf-8-sig')
        assert 'XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_FUNCTION(&managed_root_read,' in text and 'extern int managed_root_read(' not in text
        run(name+'-header',[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsyntax-only',*['-I'+str(x) for x in (root,public/'include',sdk,sdk/'include')],generated],name!='exact')
    unchanged=all(sha(p)==v for p,v in inputs.items());assert unchanged
    (out/'results.json').write_text(json.dumps(dict(complete=True,passed=True,inputsUnchanged=True,inputs=inputs,rows=rows,scope=__doc__,networkTests=False),indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()
