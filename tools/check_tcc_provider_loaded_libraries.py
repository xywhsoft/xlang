"""Real selected TinyCC provider native-library reference ownership, CPU only.

Uses the existing provider and explicit target distribution unchanged. Builds
only a small publisher and its C harness, checks duplicate loads, two cohosts,
registered-symbol precedence and unloading before process exit. No compiler,
VFS, language-runtime, graphics, browser or shipping qualification is implied.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','provider','tcc-dist','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--fixture',type=Path,help='Explicit test-only fixture successor over immutable source')
    args=parser.parse_args();root,provider,dist,out=[p.resolve()for p in (args.root,args.provider,args.tcc_dist,args.out)]
    fixture=args.fixture.resolve() if args.fixture else root/'test/cases/tcc_loaded_library_lifetime.c'
    cc=shutil.which('gcc');assert cc and provider.is_file() and dist.is_dir()
    out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    if os.name=='nt':shutil.copy2(provider,out/provider.name)
    source=out/'publisher.c'
    source.write_text(('#ifdef _WIN32\n__declspec(dllexport)\n#endif\n'
        'int tcc_lifetime_value(void){return 37;}\n'),encoding='utf-8')
    pins=[Path(__file__).resolve(),Path(cc),provider,fixture,source,root/'lib/tcc/libtcc.h']
    pins.extend(p for p in dist.rglob('*') if p.is_file())
    inputs={str(p):sha(p)for p in pins}
    report=dict(scope=__doc__,complete=False,passed=False,provider=str(provider),inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        start=time.monotonic();result=subprocess.run(list(map(str,command)),capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=result.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,result.returncode,flush=True);assert result.returncode==0,log
    native=out/('publisher.dll'if os.name=='nt'else'libpublisher.so')
    run('publisher-build',[cc,'-std=c11','-O2','-shared',*([]if os.name=='nt'else['-fPIC']),source,'-o',native])
    systems=[]if os.name=='nt'else['-ldl','-Wl,-rpath,'+str(provider.parent)]
    for level in ('O0','O2'):
        program=out/('lifetime-'+level+('.exe'if os.name=='nt'else''))
        run(level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+level,'-I'+str(root/'lib/tcc'),
            fixture,provider,*systems,'-o',program])
        run(level+'-run',[program,native,dist])
    report.update(complete=True,inputsUnchanged=all(sha(p)==value for p,value in inputs.items()))
    report['passed']=report['inputsUnchanged'];save();assert report['passed']

if __name__=='__main__':main()
